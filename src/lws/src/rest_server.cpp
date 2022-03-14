#include "rest_server.h"

#include <algorithm>
#include <boost/utility/string_ref.hpp>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include "common/error.h"              // beldex/src
#include "common/expect.h"             // beldex/src
#include "crypto/crypto.h"             // beldex/src
#include "cryptonote_config.h"         // beldex/src
#include "db/data.h"
#include "db/storage.h"
#include "error.h"
#include "lmdb/util.h"                 // beldex/src
#include "net/http_base.h"             // beldex/contrib/epee/include
#include "net/net_parse_helpers.h"     // beldex/contrib/epee/include
#include "net/net_utils_base.h"        // beldex/contrib/epee/include
#include "byte_slice.h"                // beldex/epee/include/epee
#include "util/http_server.h"
#include "wire/json.h"
#include "wire/json/error.h"

namespace lws
{
  namespace
  {
    namespace http = epee::net_utils::http;

    struct context : epee::net_utils::connection_context_base
    {
      context()
        : epee::net_utils::connection_context_base()
      {}
    };

    struct endpoint
    {
      char const* const name;
      expect<epee::byte_slice> (*const run)(std::string&&, db::storage);
      const unsigned max_size;
    };

    constexpr const endpoint endpoints[] =
    {
      // {"/get_address_info",      call<get_address_info>, 2 * 1024},
      // {"/get_address_txs",       call<get_address_txs>,  2 * 1024},
      // {"/get_random_outs",       call<get_random_outs>,  2 * 1024},
      {"/get_txt_records",       nullptr,                0       }
      // {"/get_unspent_outs",      call<get_unspent_outs>, 2 * 1024},
      // {"/import_wallet_request", call<import_request>,   2 * 1024},
      // {"/login",                 call<login>,            2 * 1024},
      // {"/submit_raw_tx",         call<submit_raw_tx>,   50 * 1024}
    };

    struct by_name_
    {
      bool operator()(endpoint const& left, endpoint const& right) const noexcept
      {
        if (left.name && right.name)
          return std::strcmp(left.name, right.name) < 0;
        return false;
      }
      bool operator()(const boost::string_ref left, endpoint const& right) const noexcept
      {
        if (right.name)
          return left < right.name;
        return false;
      }
      bool operator()(endpoint const& left, const boost::string_ref right) const noexcept
      {
        if (left.name)
          return left.name < right;
        return false;
      }
    };
    constexpr const by_name_ by_name{};

  } //anonymous
  struct rest_server::internal final : public lws::http_server_impl_base<rest_server::internal, context>
  {
    db::storage disk;
    // rpc::client client;

    explicit internal(boost::asio::io_service& io_service, lws::db::storage disk)
      : lws::http_server_impl_base<rest_server::internal, context>(io_service)
      , disk(std::move(disk))
    {
      // assert(std::is_sorted(std::begin(endpoints), std::end(endpoints), by_name));
    }

    virtual bool
    handle_http_request(const http::http_request_info& query, http::http_response_info& response, context& ctx)
    override final
    {
      const auto handler = std::lower_bound(
        std::begin(endpoints), std::end(endpoints), query.m_URI, by_name
      );
      if (handler == std::end(endpoints) || handler->name != query.m_URI)
      {
        response.m_response_code = 404;
        response.m_response_comment = "Not Found";
        return true;
      }

      if (handler->run == nullptr)
      {
        response.m_response_code = 501;
        response.m_response_comment = "Not Implemented";
        return true;
      }

      if (handler->max_size < query.m_body.size())
      {
        MINFO("Client exceeded maximum body size (" << handler->max_size << " bytes)");
        response.m_response_code = 400;
        response.m_response_comment = "Bad Request";
        return true;
      }

      if (query.m_http_method != http::http_method_post)
      {
        response.m_response_code = 405;
        response.m_response_comment = "Method Not Allowed";
        return true;
      }

      // \TODO remove copy of json string here :/
      auto body = handler->run(std::string{query.m_body}, disk.clone());
      if (!body)
      {
        MINFO(body.error().message() << " from " << ctx.m_remote_address.str() << " on " << handler->name);

        if (body.error().category() == wire::error::rapidjson_category())
        {
          response.m_response_code = 400;
          response.m_response_comment = "Bad Request";
        }
        else if (body == lws::error::account_not_found || body == lws::error::duplicate_request)
        {
          response.m_response_code = 403;
          response.m_response_comment = "Forbidden";
        }
        else if (body.matches(std::errc::timed_out) || body.matches(std::errc::no_lock_available))
        {
          response.m_response_code = 503;
          response.m_response_comment = "Service Unavailable";
        }
        else
        {
          response.m_response_code = 500;
          response.m_response_comment = "Internal Server Error";
        }
        return true;
      }

      response.m_response_code = 200;
      response.m_response_comment = "OK";
      response.m_mime_tipe = "application/json";
      response.m_header_info.m_content_type = "application/json";
      response.m_body.assign(reinterpret_cast<const char*>(body->data()), body->size()); // \TODO Remove copy here too!s
      return true;
    }
  };

  rest_server::rest_server(epee::span<const std::string> addresses, db::storage disk, configuration config)
    : io_service_(), ports_()
  {
    ports_.emplace_back(io_service_, std::move(disk));

    if (addresses.empty())
      MONERO_THROW(common_error::kInvalidArgument, "REST server requires 1 or more addresses");

    const auto init_port = [] (internal& port, const std::string& address, configuration config) -> bool
    {
      epee::net_utils::http::url_content url{};
      if (!epee::net_utils::parse_url(address, url))
        MONERO_THROW(lws::error::configuration, "REST Server URL/address is invalid");

      const bool https = url.schema == "https";
      if (!https && url.schema != "http")
        MONERO_THROW(lws::error::configuration, "Unsupported scheme, only http or https supported");

      if (std::numeric_limits<std::uint16_t>::max() < url.port)
        MONERO_THROW(lws::error::configuration, "Specified port for rest server is out of range");

      if (!https)
      {
        boost::system::error_code error{};
        const boost::asio::ip::address ip_host =
          ip_host.from_string(url.host, error);
        if (error)
          MONERO_THROW(lws::error::configuration, "Invalid IP address for REST server");
        if (!ip_host.is_loopback() && !config.allow_external)
          MONERO_THROW(lws::error::configuration, "Binding to external interface with http - consider using https or secure tunnel (ssh, etc). Use --confirm-external-bind to override");
      }

      if (url.port == 0)
        url.port = https ? 8443 : 8080;

      epee::net_utils::ssl_options_t ssl_options = https ?
        epee::net_utils::ssl_support_t::e_ssl_support_enabled :
        epee::net_utils::ssl_support_t::e_ssl_support_disabled;
      ssl_options.verification = epee::net_utils::ssl_verification_t::none; // clients verified with view key
      ssl_options.auth = std::move(config.auth);

      if (!port.init(std::to_string(url.port), std::move(url.host), std::move(config.access_controls), std::move(ssl_options)))
        MONERO_THROW(lws::error::http_server, "REST server failed to initialize");
      return https;
    };

    bool any_ssl = false;
    for (std::size_t index = 1; index < addresses.size(); ++index)
    {
      ports_.emplace_back(io_service_, ports_.front().disk.clone());
      any_ssl |= init_port(ports_.back(), addresses[index], config);
    }

    const bool expect_ssl = !config.auth.private_key_path.empty();
    const std::size_t threads = config.threads;
    any_ssl |= init_port(ports_.front(), addresses[0], std::move(config));
    if (!any_ssl && expect_ssl)
      MONERO_THROW(lws::error::configuration, "Specified SSL key/cert without specifying https capable REST server");

    if (!ports_.front().run(threads, false))
      MONERO_THROW(lws::error::http_server, "REST server failed to run");
  }

  rest_server::~rest_server() noexcept
  {}
}