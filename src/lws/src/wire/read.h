#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "common/expect.h" // monero/src
#include "span.h"          // monero/contrib/epee/include
#include "wire/error.h"
#include "wire/field.h"
#include "wire/traits.h"

namespace wire
{
  //! Interface for converting "wire" (byte) formats to C/C++ objects without a DOM.
  class reader
  {
    std::size_t depth_; //!< Tracks number of recursive objects and arrays

  protected:
    //! \throw wire::exception if max depth is reached
    void increment_depth();
    void decrement_depth() noexcept { --depth_; }

    reader(const reader&) = default;
    reader(reader&&) = default;
    reader& operator=(const reader&) = default;
    reader& operator=(reader&&) = default;

  public:
    struct key_map
    {
        const char* name;
        unsigned id; //<! For integer key formats;
    };

    //! \return Maximum read depth for both objects and arrays before erroring
    static constexpr std::size_t max_read_depth() noexcept { return 100; }

    reader() noexcept
      : depth_(0)
    {}

    virtual ~reader() noexcept
    {}

    //! \return Number of recursive objects and arrays
    std::size_t depth() const noexcept { return depth_; }

    //! \throw wire::exception if parsing is incomplete.
    virtual void check_complete() const = 0;

    //! \throw wire::exception if next value not a boolean.
    virtual bool boolean() = 0;

    //! \throw wire::expception if next value not an integer.
    virtual std::intmax_t integer() = 0;

    //! \throw wire::exception if next value not an unsigned integer.
    virtual std::uintmax_t unsigned_integer() = 0;

    //! \throw wire::exception if next value not number
    virtual double real() = 0;

    //! throw wire::exception if next value not string
    virtual std::string string() = 0;

    // ! \throw wire::exception if next value cannot be read as binary
    virtual std::vector<std::uint8_t> binary() = 0;

    //! \throw wire::exception if next value cannot be read as binary into `dest`.
    virtual void binary(epee::span<std::uint8_t> dest) = 0;

    //! \throw wire::exception if next value invalid enum. \return Index in `enums`.
    virtual std::size_t enumeration(epee::span<char const* const> enums) = 0;

    /*! \throw wire::exception if next value not array
        \return Number of values to read before calling `is_array_end()`. */
    virtual std::size_t start_array() = 0;

    //! \return True if there is another element to read.
    virtual bool is_array_end(std::size_t count) = 0;

    //! \throw wire::exception if array end delimiter not present.
    void end_array() noexcept { decrement_depth(); }


    //! \throw wire::exception if not object begin. \return State to be given to `key(...)` function.
    virtual std::size_t start_object() = 0;

    /*! Read a key of an object field and match against a known list of keys.
       Skips or throws exceptions on unknown fields depending on implementation
       settings.

      \param map of known keys (strings and integer) that are valid.
      \param[in,out] state returned by `start_object()` or `key(...)` whichever
        was last.
      \param[out] index of match found in `map`.

      \throw wire::exception if next value not a key.
      \throw wire::exception if next key not found in `map` and skipping
        fields disabled.

      \return True if this function found a field in `map` to process.
     */
    virtual bool key(epee::span<const key_map> map, std::size_t& state, std::size_t& index) = 0;

    void end_object() noexcept { decrement_depth(); }
  };

  inline void read_bytes(reader& source, bool& dest)
  {
    dest = source.boolean();
  }

  inline void read_bytes(reader& source, double& dest)
  {
    dest = source.real();
  }

  inline void read_bytes(reader& source, std::string& dest)
  {
    dest = source.string();
  }

  template<typename R>
  inline void read_bytes(R& source, std::vector<std::uint8_t>& dest)
  {
    dest = source.binary();
  }

  template<typename T>
  inline enable_if<is_blob<T>::value> read_bytes(reader& source, T& dest)
  {
    source.binary(epee::as_mut_byte_span(dest));
  }

  namespace integer
  {
    [[noreturn]] void throw_exception(std::intmax_t source, std::intmax_t min);
    [[noreturn]] void throw_exception(std::uintmax_t source, std::uintmax_t max);

    template<typename Target, typename U>
    inline Target convert_to(const U source)
    {
      using common = typename std::common_type<Target, U>::type;
      static constexpr const Target target_min = std::numeric_limits<Target>::min();
      static constexpr const Target target_max = std::numeric_limits<Target>::max();

      /* After optimizations, this is:
           * 1 check for unsigned -> unsigned (uint, uint)
           * 2 checks for signed -> signed (int, int)
           * 2 checks for signed -> unsigned-- (
           * 1 check for unsigned -> signed (uint, uint)

         Put `WIRE_DLOG_THROW` in cpp to reduce code/ASM duplication. Do not
         remove first check, signed values can be implicitly converted to
         unsigned in some checks. */
      if (!std::numeric_limits<Target>::is_signed && source < 0)
        throw_exception(std::intmax_t(source), std::intmax_t(0));
      else if (common(source) < common(target_min))
        throw_exception(std::intmax_t(source), std::intmax_t(target_min));
      else if (common(target_max) < common(source))
        throw_exception(std::uintmax_t(source), std::uintmax_t(target_max));

      return Target(source);
    }
  }

  inline void read_bytes(reader& source, char& dest)
  {
    dest = integer::convert_to<char>(source.integer());
  }
  inline void read_bytes(reader& source, short& dest)
  {
    dest = integer::convert_to<short>(source.integer());
  }
  inline void read_bytes(reader& source, int& dest)
  {
    dest = integer::convert_to<int>(source.integer());
  }
  inline void read_bytes(reader& source, long& dest)
  {
    dest = integer::convert_to<long>(source.integer());
  }
  inline void read_bytes(reader& source, long long& dest)
  {
    dest = integer::convert_to<long long>(source.integer());
  }

  inline void read_bytes(reader& source, unsigned char& dest)
  {
    dest = integer::convert_to<unsigned char>(source.unsigned_integer());
  }
  inline void read_bytes(reader& source, unsigned short& dest)
  {
    dest = integer::convert_to<unsigned short>(source.unsigned_integer());
  }
  inline void read_bytes(reader& source, unsigned& dest)
  {
    dest = integer::convert_to<unsigned>(source.unsigned_integer());
  }
  inline void read_bytes(reader& source, unsigned long& dest)
  {
    dest = integer::convert_to<unsigned long>(source.unsigned_integer());
  }
  inline void read_bytes(reader& source, unsigned long long& dest)
  {
    dest = integer::convert_to<unsigned long long>(source.unsigned_integer());
  }
} // wire

namespace wire_read
{
    /*! Don't add a function called `read_bytes` to this namespace, it will prevent
      ADL lookup. ADL lookup delays the function searching until the template
      is used instead of when its defined. This allows the unqualified calls to
      `read_bytes` in this namespace to "find" user functions that are declared
      after these functions (the technique behind `boost::serialization`). */

  [[noreturn]] void throw_exception(wire::error::schema code, const char* display, epee::span<char const* const> name_list);

  //! \return `T` converted from `source` or error.
  template<typename T, typename R>
  inline expect<T> to(R& source)
  {
    try
    {
      T dest{};
      // read_bytes(source, dest);
      source.check_complete();
      return dest;
    }
    catch (const wire::exception& e)
    {
      return e.code();
    }
  }

  template<typename R, typename T>
  inline void array(R& source, T& dest)
  {
    using value_type = typename T::value_type;
    static_assert(!std::is_same<value_type, char>::value, "read array of chars as binary");
    static_assert(!std::is_same<value_type, std::uint8_t>::value, "read array of unsigned chars as binary");
    
    std::size_t count = source.start_array();

    dest.clear();
    dest.reserve(count);

    bool more = count;
    while (more || !source.is_array_end(count))
    {
      dest.emplace_back();
      read_bytes(source, dest.back());
      --count;
      more &= bool(count);
    }

    return source.end_array();
  }

  // `unpack_variant_field` identifies which of the variant types was selected. starts with index-0

  template<typename R, typename T>
  inline void unpack_variant_field(std::size_t, R&, const T&)
  {}

  template<typename R, typename T, typename U, typename... X>
  inline void unpack_variant_field(const std::size_t index, R& source, T& variant, const wire::option<U>& head, const wire::option<X>&... tail)
  {
    if (index)
      unpack_variant_field(index - 1, source, variant, tail...);
    else
    {
      U dest{};
      read_bytes(source, dest);
      variant = std::move(dest);
    }
  }

  // `unpack_field` expands `variant_field_`s or reads `field_`s directly

  template<typename R, typename T, bool Required, typename... U>
  inline void unpack_field(const std::size_t index, R& source, wire::variant_field_<T, Required, U...>& dest)
  {
    unpack_variant_field(index, source, dest.get_value(), static_cast< const wire::option<U>& >(dest)...);
  }

  template<typename R, typename T>
  inline void unpack_field(std::size_t, R& source, wire::field_<T, true>& dest)
  {
    read_bytes(source, dest.get_value());
  }

  template<typename R, typename T>
  inline void unpack_field(std::size_t, R& source, wire::field_<T, false>& dest)
  {
    dest.get_value().emplace();
    read_bytes(source, *dest.get_value());
  }

  // `expand_field_map` writes a single `field_` name or all option names in a `variant_field_` to a table

  template<std::size_t N>
  inline void expand_field_map(std::size_t, wire::reader::key_map (&)[N])
  {}

  template<std::size_t N, typename T, typename... U>
  inline void expand_field_map(std::size_t index, wire::reader::key_map (&map)[N], const T& head, const U&... tail)
  {
    map[index].name = head.name;
    map[index].id = 0;
    expand_field_map(index + 1, map, tail...);
  }

  template<std::size_t N, typename T, bool Required, typename... U>
  inline void expand_field_map(std::size_t index, wire::reader::key_map (&map)[N], const wire::variant_field_<T, Required, U...>& field)
  {
    expand_field_map(index, map, static_cast< const wire::option<U> & >(field)...);
  }

  //! Tracks read status of every object field instance.
  template<typename T>
  class tracker
  {
    T field_;
    std::size_t our_index_;
    bool read_;

  public:
    static constexpr bool is_required() noexcept { return T::is_required(); }
    static constexpr std::size_t count() noexcept { return T::count(); }

    explicit tracker(T field)
      : field_(std::move(field)), our_index_(0), read_(false)
    {}

    //! \return Field name if required and not read, otherwise `nullptr`.
    const char* name_if_missing() const noexcept
    {
      return (is_required() && !read_) ? field_.name : nullptr;
    }


    //! Set all entries in `map` related to this field (expand variant types!).
    template<std::size_t N>
    std::size_t set_mapping(std::size_t index, wire::reader::key_map (&map)[N])
    {
      our_index_ = index;
      expand_field_map(index, map, field_); // expands possible inner options
      return index + count();
    }

    //! Try to read next value if `index` matches `this`. \return 0 if no match, 1 if optional field read, and 2 if required field read
    template<typename R>
    std::size_t try_read(R& source, const std::size_t index)
    {
      if (index < our_index_ || our_index_ + count() <= index)
        return 0;
      if (read_)
        throw_exception(wire::error::schema::invalid_key, "duplicate", {std::addressof(field_.name), 1});

      unpack_field(index - our_index_, source, field_);
      read_ = true;
      return 1 + is_required();
    }
  };

  // `expand_tracker_map` writes all `tracker` types to a table

  template<std::size_t N>
  inline constexpr std::size_t expand_tracker_map(std::size_t index, const wire::reader::key_map (&)[N])
  {
    return index;
  }

  template<std::size_t N, typename T, typename... U>
  inline void expand_tracker_map(std::size_t index, wire::reader::key_map (&map)[N], tracker<T>& head, tracker<U>&... tail)
  {
    expand_tracker_map(head.set_mapping(index, map), map, tail...);
  }

  template<typename R, typename... T>
  inline void object(R& source, tracker<T>... fields)
  {
    static constexpr const std::size_t total_subfields = wire::sum(fields.count()...);
    static_assert(total_subfields < 100, "algorithm uses too much stack space and linear searching");

    std::size_t state = source.start_object();
    std::size_t required = wire::sum(std::size_t(fields.is_required())...);

    wire::reader::key_map map[total_subfields] = {};
    expand_tracker_map(0, map, fields...);

    std::size_t next = 0;
    while (source.key(map, state, next))
    {
      switch (wire::sum(fields.try_read(source, next)...))
      {
      default:
      case 0:
        throw_exception(wire::error::schema::invalid_key, "bad map setup", nullptr);
        break;
      case 2:
        --required; /* fallthrough */
      case 1:
        break;
      }
    }

    if (required)
    {
      const char* missing[] = {fields.name_if_missing()...};
      throw_exception(wire::error::schema::missing_key, "", missing);
    }

    source.end_object();
  }
} // wire_read

namespace wire
{
  template<typename T>
  inline void array(reader& source, T& dest)
  {
    wire_read::array(source, dest);
  }
  template<typename R, typename T>
  inline enable_if<is_array<T>::value> read_bytes(R& source, T& dest)
  {
    wire_read::array(source, dest);
  }

  template<typename... T>
  inline void object(reader& source, T... fields)
  {
    wire_read::object(source, wire_read::tracker<T>{std::move(fields)}...);
  }
}
