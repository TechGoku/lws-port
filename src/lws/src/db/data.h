#pragma once

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <utility> 
 
#include "storage.h"
#include "wire/fwd.h"
#include "ringct/rctTypes.h" //! \TODO brings in lots of includes, try to remove
#include "wire/traits.h"
namespace lws
{
namespace db
{
  enum class account_id : std::uint32_t
  {
    invalid = std::uint32_t(-1) //!< Always represents _not an_ account id.
  };
  WIRE_AS_INTEGER(account_id);
 enum class account_time : std::uint32_t {};
 WIRE_AS_INTEGER(account_time);
 enum class block_id : std::uint64_t {};
 WIRE_AS_INTEGER(block_id);
 struct output_id
  {
    std::uint64_t high; //!< Amount on public chain; rct outputs are `0`
    std::uint64_t low;  //!< Offset within `amount` on the public chain
  };
  WIRE_DECLARE_OBJECT(output_id);

  enum class account_status : std::uint8_t
  {
    active = 0, //!< Actively being scanned and reported by API
    inactive,   //!< Not being scanned, but still reported by API
    hidden      //!< Not being scanned or reported by API
  };
  WIRE_DECLARE_ENUM(account_status);

 enum account_flags : std::uint8_t
  {
    default_account = 0,
    admin_account   = 1,          //!< Not currently used, for future extensions
    account_generated_locally = 2 //!< Flag sent by client on initial login request
  };

  enum class request : std::uint8_t
  {
    create = 0, //!< Add a new account
    import_scan //!< Set account start and scan height to zero.
  };
  WIRE_DECLARE_ENUM(request);

 struct view_key : crypto::ec_scalar {};
  struct account_address
  {
    crypto::public_key view_public; //!< Must be first for LMDB optimizations.
    crypto::public_key spend_public;
  };
  static_assert(sizeof(account_address) == 64, "padding in account_address");
  WIRE_DECLARE_OBJECT(account_address);
 struct account
  {
    account_id id;          //!< Must be first for LMDB optimizations
    account_time access;    //!< Last time `get_address_info` was called.
    account_address address;
    view_key key;           //!< Doubles as authorization handle for REST API.
    block_id scan_height;   //!< Last block scanned; check-ins are always by block
    block_id start_height;  //!< Account started scanning at this block height
    account_time creation;  //!< Time account first appeared in database.
    account_flags flags;    //!< Additional account info bitmask.
    char reserved[3];
  };

   struct block_info
  {
    block_id id;      //!< Must be first for LMDB optimizations
    crypto::hash hash;
  };

  WIRE_DECLARE_OBJECT(block_info);
  struct transaction_link
  {
    block_id height;      //!< Block height containing transaction
    crypto::hash tx_hash; //!< Hash of the transaction
  };

  //! Additional flags stored in `output`s.
  enum extra : std::uint8_t
  {
    coinbase_output = 1,
    ringct_output   = 2
  };

  //! Packed information stored in `output`s.
  enum class extra_and_length : std::uint8_t {};

  //! \return `val` and `length` packed into a single byte.
  inline extra_and_length pack(extra val, std::uint8_t length) noexcept
  {
    assert(length <= 32);
    return extra_and_length((std::uint8_t(val) << 6) | (length & 0x3f));
  }

  //! \return `extra` and length unpacked from a single byte.
  inline std::pair<extra, std::uint8_t> unpack(extra_and_length val) noexcept
  {
    const std::uint8_t real_val = std::uint8_t(val);
    return {extra(real_val >> 6), std::uint8_t(real_val & 0x3f)};
  }

   //! Information for an output that has been received by an `account`.
  struct output
  {
    transaction_link link;        //! Orders and links `output` to `spend`s.

    //! Data that a linked `spend` needs in some REST endpoints.
    struct spend_meta_
    {
      output_id id;             //!< Unique id for output within monero
      // `link` and `id` must be in this order for LMDB optimizations
      std::uint64_t amount;
      std::uint32_t mixin_count;//!< Ring-size of TX
      std::uint32_t index;      //!< Offset within a tx
      crypto::public_key tx_public;
    } spend_meta;

    std::uint64_t timestamp;
    std::uint64_t unlock_time; //!< Not always a timestamp; mirrors chain value.
    crypto::hash tx_prefix_hash;
    crypto::public_key pub;    //!< One-time spendable public key.
    rct::key ringct_mask;      //!< Unencrypted CT mask
    char reserved[7];
    extra_and_length extra;    //!< Extra info + length of payment id
    union payment_id_
    {
      crypto::hash8 short_;  //!< Decrypted short payment id
      crypto::hash long_;    //!< Long version of payment id (always decrypted)
    } payment_id;
  };

    struct spend
  {
    transaction_link link;    //!< Orders and links `spend` to `output`.
    crypto::key_image image;  //!< Unique ID for the spend
    // `link` and `image` must in this order for LMDB optimizations
    output_id source;         //!< The output being spent
    std::uint64_t timestamp;  //!< Timestamp of spend
    std::uint64_t unlock_time;//!< Unlock time of spend
    std::uint32_t mixin_count;//!< Ring-size of TX output
    char reserved[3];
    std::uint8_t length;      //!< Length of `payment_id` field (0..32).
    crypto::hash payment_id;  //!< Unencrypted only, can't decrypt spend
  };
  WIRE_DECLARE_OBJECT(spend);

  struct key_image
  {
    crypto::key_image value; //!< Actual key image value
    // The above field needs to be first for LMDB optimizations
    transaction_link link;   //!< Link to `spend` and `output`.
  };
  WIRE_DECLARE_OBJECT(key_image);
  struct request_info
  {
    account_address address;//!< Must be first for LMDB optimizations
    view_key key;
    block_id start_height;
    account_time creation;        //!< Time the request was created.
    account_flags creation_flags; //!< Generated locally?
    char reserved[3];
  };

  inline constexpr bool operator==(output_id left, output_id right) noexcept
  {
    return left.high == right.high && left.low == right.low;
  }
  inline constexpr bool operator!=(output_id left, output_id right) noexcept
  {
    return left.high != right.high || left.low != right.low;
  }
  inline constexpr bool operator<(output_id left, output_id right) noexcept
  {
    return left.high == right.high ?
      left.low < right.low : left.high < right.high;
  }
  inline constexpr bool operator<=(output_id left, output_id right) noexcept
  {
    return left.high == right.high ?
      left.low <= right.low : left.high < right.high;
  }

  bool operator<(transaction_link const& left, transaction_link const& right) noexcept;
  bool operator<=(transaction_link const& left, transaction_link const& right) noexcept;

  /*!
    Write `address` to `out` in base58 format using `lws::config::network` to
    determine tag. */
  std::ostream& operator<<(std::ostream& out, account_address const& address);

} //db
} //lws

namespace wire
{
  template<>
   struct is_blob<lws::db::view_key>
    : std::true_type
  {};
}