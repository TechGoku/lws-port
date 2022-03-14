#include <iostream>
#include <string>
#include <string.h>
#include <algorithm>
#include <boost/utility/string_ref.hpp>

#include "rest_server.h"
#include "common/expect.h" // beldex/src
#include "lmdb/util.h"
#include "db/fwd.h"
#include "wallet/wallet_light_rpc.h"
#include "epee/byte_slice.h"
#include "oxenmq/oxenmq.h"
#include "oxenmq/connections.h"
#include "db/storage.h"
#include "rpc/light_wallet.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "util/random_outputs.h"
#include "wire/read.h"
#include "wire/write.h"
#include "json/write.h"
#include "wire/json/base.h"
#include "nlohmann/json.hpp"
#include "error.h"
using namespace oxenmq;
using namespace std;

string failmsg = " its_failed ";
json msg = " success ";
// // namespace conn_url
// // {
// //     auto connectionURL()
// //     {
// //         cout << __func__ << " function_called "
// //                   << "\n";
// //         auto connection_url = m_LMQ->connect_remote(
// //             "tcp://127.0.0.1:4567",
// //             [](ConnectionID conn)
// //             {
// //                 cout << "Connected \n";
// //             },
// //             [](ConnectionID conn, string_view f)
// //             {
// //                 cout << "connect failed: \n";
// //             });
// //         return connection_url;
// //     }
// // }msg
namespace connectionURL
{
    auto conn()
    {
        using LMQ_ptr = shared_ptr<oxenmq::OxenMQ>;
        string msg = " success ";
        LMQ_ptr m_LMQ = make_shared<oxenmq::OxenMQ>();
        m_LMQ->start();
        auto connection_url = m_LMQ->connect_remote(
            "tcp://127.0.0.1:4567",
            [](ConnectionID conn)
            { cout << "Connected \n"; },
            [](ConnectionID conn, string_view f)
            {
                cout << "connect failed: \n";
            });
    }
}

namespace lws
{
    namespace {
        bool is_hidden(db::account_status status) noexcept
    {
      switch (status)
      {
      case db::account_status::active:
      case db::account_status::inactive:
        return false;
      default:
      case db::account_status::hidden:
        break;
      }
      return true;
    }

    bool key_check(const rpc::account_credentials& creds){
        crypto::public_key verify{};
        if (!crypto::secret_key_to_public_key(creds.key, verify))
            return false;
        if (verify != creds.address.view_public)
            return false;
        return true;
        }
    }
    struct submit_raw_tx
    {

        // using request = tools::light_rpc::SUBMIT_RAW_TX::request;
        struct request
      {
        std::string address;
        std::string view_key;
        std::string tx;
        bool flash;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(address)
          KV_SERIALIZE(view_key)
          KV_SERIALIZE(tx)
          KV_SERIALIZE_OPT(flash, false)
        END_KV_SERIALIZE_MAP()
      };
      struct response
      {
        std::string status;
        std::string error;

        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(status)
          KV_SERIALIZE(error)
        END_KV_SERIALIZE_MAP()

      };
        // using response = tools::light_rpc::SUBMIT_RAW_TX::response;

        request object_1;
        string param = object_1.tx;
        int flash{};
        string tx_hash;
        
        static expect<response> handle(request req, const db::storage &disk)
        {
            using LMQ_ptr = shared_ptr<oxenmq::OxenMQ>;
            
            LMQ_ptr m_LMQ = make_shared<oxenmq::OxenMQ>();
            m_LMQ->start();
            auto connection_url = m_LMQ->connect_remote(
                "tcp://127.0.0.1:4567",
                [](ConnectionID conn)
                { cout << "Connected \n"; },
                [](ConnectionID conn, string_view f)
                {
                    cout << "connect failed: \n";
                });

            // using transaction_rpc = cryptonote::rpc::SendRawTxHex;
            json respond{};
            m_LMQ->request(
                connection_url, "submit_raw_tx", [respond,msg](bool status, auto response_data)
                {
                if (status == 1 && response_data[0] == "200") cout << " response : " << response_data[1] << "\n"; },
                "{\"tx\": \"" + string(msg) + "\"}");
                // return success();
        }

    };
    struct get_random_outs
    {
        using request = rpc::get_random_outs_request;
        using response = rpc::get_random_outs_response;

        static expect<response> handle(request req, const db::storage &)
        {
            using distribution_rpc = cryptonote::rpc::GET_OUTPUT_DISTRIBUTION;
            using histogram_rpc = cryptonote::rpc::GET_OUTPUT_HISTOGRAM;

            vector<uint64_t> amounts = move(req.amounts.values);
            const std::size_t rct_count = amounts.end() - std::lower_bound(amounts.begin(), amounts.end(), 0);
            vector<lws::histogram> histogram{};
            if (rct_count < amounts.size())
            {
                json hist_req;
                histogram_rpc::request his_req{};
                his_req.amounts = std::move(amounts);
                his_req.min_count = 0;
                his_req.max_count = 0;
                his_req.unlocked = true;
                his_req.recent_cutoff = 0;

                using LMQ_ptr = shared_ptr<oxenmq::OxenMQ>;
                LMQ_ptr m_LMQ = make_shared<oxenmq::OxenMQ>();
                m_LMQ->start();
                auto connection_url = m_LMQ->connect_remote(
                    "tcp://127.0.0.1:4567",
                    [](ConnectionID conn)
                    { cout << "Connected \n"; },
                    [](ConnectionID conn, string_view f)
                    {
                        cout << "connect failed: \n";
                    });
                // using transaction_rpc = cryptonote::rpc::SendRawTxHex;
                m_LMQ->request(
                    connection_url, "get_output_histogram", [&hist_req,msg](bool status, auto histogram)
                    {
                    if (status == 1 && histogram[0] == "200")cout << " response : " << histogram[1] << "\n";
                    json res = json::parse(histogram[1]); 
                    },"{\"tx\": \"" + std::string(msg) + "\"}");

                amounts = move(his_req.amounts);
                amounts.insert(amounts.end(), rct_count, 0);

                vector<uint64_t> distributions{};
                if (rct_count){
                    distribution_rpc::request dist_req{};
                    if (rct_count == amounts.size())
                     dist_req.amounts = std::move(amounts);

                     dist_req.amounts.resize(1);
                     dist_req.from_height = 0;
                     dist_req.to_height = 0;
                     dist_req.cumulative = true;

                    m_LMQ->request(
                    connection_url, "get_output_histogram", [&dist_req,msg](bool status, auto distribution)
                    {
                    if (status == 1 && distribution[0] == "200")cout << " response : " << distribution[1] << "\n";
                    json dist_req = json::parse(distribution[1]); 
                    },"{\"tx\": \"" + std::string(msg) + "\"}");

                    // distributions = std::move(dist_req[0]);

                    class zmq_fetch_keys{

                    };
                }
              

            }
        }
    };

    struct login
    {
      std::string msg = "error";
      using request = rpc::login_request;
      using response = rpc::login_response;

      static expect<response> handle(request req, db::storage disk)
      {
        if (!key_check(req.creds))
          return {lws::error::bad_view_key};

        {
          auto reader = disk.start_read();
          if (!reader)
            return reader.error();

          const auto account = reader->get_account(req.creds.address);
          reader->finish_read();

          if (account)
          {
            if (is_hidden(account->first))
              return {lws::error::account_not_found};
              
            // Do not count a request for account creation as login
            return response{false, bool(account->second.flags & db::account_generated_locally)};
          }
          else if (!req.create_account || account != lws::error::account_not_found)
            return account.error();
        }

        const auto flags = req.generated_locally ? db::account_generated_locally : db::default_account;
        // MONERO_CHECK(expect<void> storage::add_account(req.creds.address, req.creds.key))
        MONERO_CHECK(disk.creation_request(req.creds.address, req.creds.key, flags));
        return response{true, req.generated_locally};
      }
    };

template <typename E>
expect<epee::byte_slice> call(string &&root, db::storage disk)
{
    using request = typename E::request;
    using response = typename E::response;

    expect<request> req = wire::json::from_bytes<request>(move(root));
    if (!req)
        return req.error();

    expect<response> resp = E::handle(move(*req), move(disk));
    if (!resp)
        return resp.error();
    return wire::json::to_bytes<response>(*resp);
}

struct endpoint
{
    char const *const name;
    expect<epee::byte_slice> (*const run)(string &&, lws::db::storage);
    const unsigned max_size;
};

constexpr const endpoint endpoints[] = {
    {"/submit_raw_tx", call<submit_raw_tx>,2 * 1024},
    {"/get_random_outs", call<get_random_outs>,2 * 1024},
    {"/get_random_outs", call<login>,2 * 1024}
    };

} //namespace lws