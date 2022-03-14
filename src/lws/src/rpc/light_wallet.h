#pragma once

#include <boost/optional/optional.hpp>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "common/expect.h" // monero/src
#include "crypto/crypto.h" // monero/src
#include "db/data.h"
#include "util/random_outputs.h"
#include "wire.h"

namespace lws
{
    namespace rpc
    {
        enum class safe_uint64 : std::uint64_t
        {
        };

        //! Read an array of uint64 values as JSON strings.
        struct safe_uint64_array
        {
            std::vector<std::uint64_t> values; // so this can be passed to another function without copy
        };

        struct get_random_outs_request
        {
            get_random_outs_request() = delete;
            std::uint64_t count;
            safe_uint64_array amounts;
        };
        struct get_random_outs_response
        {
            get_random_outs_response() = delete;
            std::vector<random_ring> amount_outs;
        };
        struct account_credentials
        {
            lws::db::account_address address;
            crypto::secret_key key;
        };

        struct import_response
        {
            import_response() = delete;
            safe_uint64 import_fee;
            const char *status;
            bool new_request;
            bool request_fulfilled;
        };
        
        struct login_request
        {
            login_request() = delete;
            account_credentials creds;
            bool create_account;
            bool generated_locally;
        };

        struct login_response
        {
            login_response() = delete;
            bool new_address;
            bool generated_locally;
        };
    }
}