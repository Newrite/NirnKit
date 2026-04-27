module;

#include "Prelude.hpp"
#include <expected>

export module NirnKit.STLExtension;

namespace NK {
    
    // New types
    export struct Error
    {
        std::string message;
    };

    export template <class T>
    using Result = std::expected<T, Error>;

    export using VoidResult = std::expected<void, Error>;
    
    
    export template <class F>
    class ScopeExit
    {
    public:
        template <class Fn>
            requires std::constructible_from<F, Fn>
        explicit ScopeExit(Fn&& fn)
            noexcept(std::is_nothrow_constructible_v<F, Fn>)
            : fn_(std::forward<Fn>(fn))
        {
        }

        ScopeExit(ScopeExit&& other)
            noexcept(std::is_nothrow_move_constructible_v<F>)
            : fn_(std::move(other.fn_)),
              active_(std::exchange(other.active_, false))
        {
        }

        ScopeExit(const ScopeExit&) = delete;
        auto operator=(const ScopeExit&) -> ScopeExit& = delete;
        auto operator=(ScopeExit&&) -> ScopeExit& = delete;

        ~ScopeExit() noexcept
        {
            if (active_)
                fn_();
        }

        auto Release() noexcept -> void
        {
            active_ = false;
        }

    private:
        F fn_;
        bool active_ = true;
    };

    template <class F>
    ScopeExit(F) -> ScopeExit<F>;
    
    
    // Overloaded for std::visit
    export template <class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };

    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;
    
    
    export namespace Internal
    {
        [[nodiscard]]
        constexpr auto IsAsciiSpace(char c) noexcept -> bool
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }
        
        [[nodiscard]]
        constexpr auto StripHexPrefix(std::string_view s) noexcept -> std::string_view
        {
            if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                s.remove_prefix(2);

            return s;
        }

        [[nodiscard]]
        constexpr auto TrimAsciiView(std::string_view s) noexcept -> std::string_view
        {
            while (!s.empty() && IsAsciiSpace(s.front()))
                s.remove_prefix(1);

            while (!s.empty() && IsAsciiSpace(s.back()))
                s.remove_suffix(1);

            return s;
        }

        [[nodiscard]]
        constexpr auto ToLowerAscii(char c) noexcept -> char
        {
            if (c >= 'A' && c <= 'Z')
                return static_cast<char>(c - 'A' + 'a');

            return c;
        }

        [[nodiscard]]
        constexpr auto IEqualsAscii(std::string_view a, std::string_view b) noexcept -> bool
        {
            if (a.size() != b.size())
                return false;

            for (size_t i = 0; i < a.size(); ++i) {
                if (ToLowerAscii(a[i]) != ToLowerAscii(b[i]))
                    return false;
            }

            return true;
        }

        [[nodiscard]]
        inline auto MakeParseError(std::string_view function,
                                   std::string_view input,
                                   std::string_view reason) -> Error
        {
            std::string message;
            message += function;
            message += ": ";
            message += reason;
            message += " [input: `";
            message += input;
            message += "`]";

            return Error{std::move(message)};
        }

        template <class T>
        [[nodiscard]]
        auto ParseFailure(std::string_view function,
                          std::string_view input,
                          std::string_view reason) -> Result<T>
        {
            return std::unexpected(MakeParseError(function, input, reason));
        }
    }
    
    
    // String helpers
    export auto Trim(std::string_view str) -> std::string
    {
        return std::string{Internal::TrimAsciiView(str)};
    }

    export auto Split(std::string_view str, char delimiter) -> std::vector<std::string>
    {
        return str
            | std::views::split(delimiter)
            | std::ranges::to<std::vector<std::string>>();
    }

    export auto TrimLeft(std::string_view s) -> std::string
    {
        auto view = s | std::views::drop_while([](char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        });

        return std::string{view.begin(), view.end()};
    }

    export auto ToUpper(std::string str) -> std::string
    {
        std::ranges::transform(str, str.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });

        return str;
    }

    export auto ToLower(std::string str) -> std::string
    {
        std::ranges::transform(str, str.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        return str;
    }
    
    export template <std::ranges::input_range R>
        requires std::constructible_from<std::string_view, std::ranges::range_reference_t<R>>
    auto Join(R&& parts, std::string_view separator) -> std::string
    {
        std::string result;
        bool first = true;

        for (auto&& part : parts) {
            if (!first)
                result += separator;

            first = false;
            result += std::string_view{part};
        }

        return result;
    }
    
    export auto ReplaceAll(std::string_view source,
                           std::string_view from,
                           std::string_view to) -> std::string
    {
        if (from.empty()) return "";

        std::string result;
        size_t pos = 0;

        while (true) {
            const size_t next = source.find(from, pos);

            if (next == std::string_view::npos) {
                result += source.substr(pos);
                break;
            }

            result += source.substr(pos, next - pos);
            result += to;
            pos = next + from.size();
        }

        return result;
    }
    
    
    // STL containers helpers
    export template <std::ranges::input_range R>
    [[nodiscard]]
    auto ToVector(R&& range)
    {
        using T = std::ranges::range_value_t<R>;

        return std::forward<R>(range)
            | std::ranges::to<std::vector<T>>();
    }

    export template <class T, std::ranges::input_range R>
    [[nodiscard]]
    auto ToVector(R&& range) -> std::vector<T>
    {
        return std::forward<R>(range)
            | std::ranges::to<std::vector<T>>();
    }
    
    export template <std::ranges::input_range R, class T>
    auto Contains(R&& range, const T& value) -> bool
    {
        return std::ranges::contains(range, value);
    }
    
    export template <std::ranges::input_range R, class T>
    auto Find(R&& range, const T& value)
    {
        return std::ranges::find(range, value);
    }

    export template <std::ranges::input_range R, class Pred>
    auto FindIf(R&& range, Pred pred)
    {
        return std::ranges::find_if(range, pred);
    }
    
    export template <std::ranges::random_access_range R, class T>
    auto IndexOf(R&& range, const T& value) -> std::optional<size_t>
    {
        const auto it = std::ranges::find(range, value);

        if (it == std::ranges::end(range))
            return std::nullopt;

        return static_cast<size_t>(std::ranges::distance(std::ranges::begin(range), it));
    }
    
    export template <std::ranges::random_access_range R, class Pred>
    auto IndexOfIf(R&& range, Pred pred) -> std::optional<size_t>
    {
        const auto it = std::ranges::find_if(range, pred);

        if (it == std::ranges::end(range))
            return std::nullopt;

        return static_cast<size_t>(std::ranges::distance(std::ranges::begin(range), it));
    }
    
    export template <std::ranges::contiguous_range R, class T>
    auto FindPtr(R& range, const T& value) -> std::ranges::range_value_t<R>*
    {
        auto it = std::ranges::find(range, value);

        if (it == std::ranges::end(range))
            return nullptr;

        return std::addressof(*it);
    }
    
    export template <std::ranges::contiguous_range R, class Pred>
    auto FindIfPtr(R& range, Pred pred) -> std::ranges::range_value_t<R>*
    {
        auto it = std::ranges::find_if(range, pred);

        if (it == std::ranges::end(range))
            return nullptr;

        return std::addressof(*it);
    }
    
    export template <std::ranges::contiguous_range R, class Pred>
    auto FindIfPtr(const R& range, Pred pred) -> const std::ranges::range_value_t<R>*
    {
        auto it = std::ranges::find_if(range, pred);

        if (it == std::ranges::end(range))
            return nullptr;

        return std::addressof(*it);
    }
    
    export template <class Map, class Key>
    auto FindValuePtr(Map& map, const Key& key) -> typename Map::mapped_type*
    {
        auto it = map.find(key);

        if (it == map.end())
            return nullptr;

        return std::addressof(it->second);
    }

    export template <class Map, class Key>
    auto FindValuePtr(const Map& map, const Key& key) -> const typename Map::mapped_type*
    {
        auto it = map.find(key);

        if (it == map.end())
            return nullptr;

        return std::addressof(it->second);
    }
    
    export template <class T, class Alloc, class U>
    auto EraseFirst(std::vector<T, Alloc>& v, const U& value) -> bool
    {
        const auto it = std::ranges::find(v, value);

        if (it == v.end())
            return false;

        v.erase(it);
        return true;
    }
    
    export template <class T, class Alloc, class Pred>
    auto EraseFirstIf(std::vector<T, Alloc>& v, Pred pred) -> bool
    {
        const auto it = std::ranges::find_if(v, pred);

        if (it == v.end())
            return false;

        v.erase(it);
        return true;
    }
    
    export template <class T, class Alloc>
    auto EraseFast(std::vector<T, Alloc>& v, size_t index) -> bool
    {
        if (index >= v.size())
            return false;

        if (index + 1 != v.size())
            v[index] = std::move(v.back());

        v.pop_back();
        return true;
    }
    
    export template <class T, class Alloc, class U>
    auto EraseFastFirst(std::vector<T, Alloc>& v, const U& value) -> bool
    {
        const auto it = std::ranges::find(v, value);

        if (it == v.end())
            return false;

        const auto index = static_cast<size_t>(std::distance(v.begin(), it));
        return EraseFast(v, index);
    }
    
    export template <class T, class Alloc, class Pred>
    auto EraseFastFirstIf(std::vector<T, Alloc>& v, Pred pred) -> bool
    {
        const auto it = std::ranges::find_if(v, pred);

        if (it == v.end())
            return false;

        const auto index = static_cast<size_t>(std::distance(v.begin(), it));
        return EraseFast(v, index);
    }
    
    export template <class T, class Alloc, class U>
    auto PushUnique(std::vector<T, Alloc>& v, U&& value) -> bool
    {
        if (Contains(v, value))
            return false;

        v.emplace_back(std::forward<U>(value));
        return true;
    }
    
    export template <class T, class Alloc>
    auto PopBack(std::vector<T, Alloc>& v) -> std::optional<T>
    {
        if (v.empty())
            return std::nullopt;

        T value = std::move(v.back());
        v.pop_back();
        return value;
    }
    
    export template <class T, class Alloc>
    auto AtPtr(std::vector<T, Alloc>& v, size_t index) -> T*
    {
        if (index >= v.size())
            return nullptr;

        return &v[index];
    }

    export template <class T, class Alloc>
    auto AtPtr(const std::vector<T, Alloc>& v, size_t index) -> const T*
    {
        if (index >= v.size())
            return nullptr;

        return &v[index];
    }
    
    export template <class Map>
    auto Keys(const Map& map)
    {
        std::vector<typename Map::key_type> result;
        result.reserve(map.size());

        for (const auto& [key, value] : map)
            result.push_back(key);

        return result;
    }

    export template <class Map>
    auto Values(const Map& map)
    {
        std::vector<typename Map::mapped_type> result;
        result.reserve(map.size());

        for (const auto& [key, value] : map)
            result.push_back(value);

        return result;
    }
    
    export template <class Map, class Key>
    auto GetOrNull(Map& map, const Key& key) -> typename Map::mapped_type*
    {
        auto it = map.find(key);

        if (it == map.end())
            return nullptr;

        return std::addressof(it->second);
    }

    export template <class Map, class Key>
    auto GetOrNull(const Map& map, const Key& key) -> const typename Map::mapped_type*
    {
        auto it = map.find(key);

        if (it == map.end())
            return nullptr;

        return std::addressof(it->second);
    }
    
    export template <class Map, class Key, class Default>
    auto GetOrDefault(const Map& map, const Key& key, Default&& fallback)
    {
        auto it = map.find(key);

        if (it == map.end())
            return std::forward<Default>(fallback);

        return it->second;
    }
    
    export template <class Map, class Key, class... Args>
    auto GetOrEmplace(Map& map, Key&& key, Args&&... args) -> typename Map::mapped_type&
    {
        auto [it, inserted] = map.try_emplace(
            std::forward<Key>(key),
            std::forward<Args>(args)...
        );

        return it->second;
    }
    
    export template <std::ranges::input_range R, class T, class Proj>
    auto FindBy(R&& range, const T& value, Proj proj)
    {
        return std::ranges::find(range, value, proj);
    }
    
    export template <std::ranges::input_range R, class T, class Proj>
    auto ContainsBy(R&& range, const T& value, Proj proj) -> bool
    {
        return std::ranges::find(range, value, proj) != std::ranges::end(range);
    }
    
    export template <class T, class Alloc, class Pred>
    auto TakeFirstIf(std::vector<T, Alloc>& v, Pred pred) -> std::optional<T>
    {
        auto it = std::ranges::find_if(v, pred);

        if (it == v.end())
            return std::nullopt;

        T value = std::move(*it);
        v.erase(it);
        return value;
    }
    
    export template <class T, class Alloc, class Pred>
    auto EraseUnstableIf(std::vector<T, Alloc>& v, Pred pred) -> size_t
    {
        size_t removed = 0;

        for (size_t i = 0; i < v.size();) {
            if (std::invoke(pred, v[i])) {
                if (i + 1 != v.size())
                    v[i] = std::move(v.back());

                v.pop_back();
                ++removed;
            } else {
                ++i;
            }
        }

        return removed;
    }
    
    
    // opt in bitmask for enum classes
    /*
    enum class ActorFlags : uint32_t
    {
        None   = 0,
        Dead   = 1 << 0,
        Hidden = 1 << 1,
        Loaded = 1 << 2,
    };
    
    template <>
    inline constexpr bool EnableBitmaskOperators<ActorFlags> = true; 
     */
    export template <class E>
    inline constexpr bool EnableBitmaskOperators = false;

    export template <class E>
    concept BitmaskEnum =
        std::is_enum_v<E> && EnableBitmaskOperators<E>;

    export template <BitmaskEnum E>
    constexpr auto operator|(E lhs, E rhs) noexcept -> E
    {
        using U = std::underlying_type_t<E>;
        return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
    }

    export template <BitmaskEnum E>
    constexpr auto operator&(E lhs, E rhs) noexcept -> E
    {
        using U = std::underlying_type_t<E>;
        return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
    }

    export template <BitmaskEnum E>
    constexpr auto HasFlag(E value, E flag) noexcept -> bool
    {
        using U = std::underlying_type_t<E>;
        return (static_cast<U>(value) & static_cast<U>(flag)) == static_cast<U>(flag);
    }
    
    
    export template <std::integral T>
    constexpr auto ToBigEndian(T value) noexcept -> T
    {
        if constexpr (std::endian::native == std::endian::big) {
            return value;
        } else {
            return std::byteswap(value);
        }
    }

    export template <std::integral T>
    constexpr auto FromBigEndian(T value) noexcept -> T
    {
        return ToBigEndian(value);
    }

    export template <class T>
    concept ParseInteger =
        std::integral<T> &&
        !std::same_as<std::remove_cv_t<T>, bool>;

    export template <ParseInteger T>
    [[nodiscard]]
    auto ParseIntegral(std::string_view input, int base = 10) -> Result<T>
    {
        const auto original = input;
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return Internal::ParseFailure<T>("ParseIntegral", original, "empty input");

        if (base < 2 || base > 36)
            return Internal::ParseFailure<T>("ParseIntegral", original, "base must be in [2, 36]");

        T value{};

        const char* first = input.data();
        const char* last = input.data() + input.size();

        const auto [ptr, ec] = std::from_chars(first, last, value, base);

        if (ec == std::errc::invalid_argument)
            return Internal::ParseFailure<T>("ParseIntegral", original, "not an integer");

        if (ec == std::errc::result_out_of_range)
            return Internal::ParseFailure<T>("ParseIntegral", original, "integer out of range");

        if (ec != std::errc{})
            return Internal::ParseFailure<T>("ParseIntegral", original, "integer parse failed");

        if (ptr != last)
            return Internal::ParseFailure<T>("ParseIntegral", original, "trailing characters");

        return value;
    }

    export template <std::signed_integral T = int64_t>
    [[nodiscard]]
    auto ParseInt(std::string_view input, int base = 10) -> Result<T>
    {
        return ParseIntegral<T>(input, base);
    }

    export template <std::unsigned_integral T = uint64_t>
    [[nodiscard]]
    auto ParseUInt(std::string_view input, int base = 10) -> Result<T>
    {
        return ParseIntegral<T>(input, base);
    }

    export template <std::floating_point T>
    [[nodiscard]]
    auto ParseFloating(std::string_view input,
                       std::chars_format format = std::chars_format::general) -> Result<T>
    {
        const auto original = input;
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return Internal::ParseFailure<T>("ParseFloating", original, "empty input");

        T value{};

        const char* first = input.data();
        const char* last = input.data() + input.size();

        const auto [ptr, ec] = std::from_chars(first, last, value, format);

        if (ec == std::errc::invalid_argument)
            return Internal::ParseFailure<T>("ParseFloating", original, "not a floating-point number");

        if (ec == std::errc::result_out_of_range)
            return Internal::ParseFailure<T>("ParseFloating", original, "floating-point value out of range");

        if (ec != std::errc{})
            return Internal::ParseFailure<T>("ParseFloating", original, "floating-point parse failed");

        if (ptr != last)
            return Internal::ParseFailure<T>("ParseFloating", original, "trailing characters");

        return value;
    }

    export [[nodiscard]]
    inline auto ParseFloat(std::string_view input) -> Result<float>
    {
        return ParseFloating<float>(input);
    }

    export [[nodiscard]]
    inline auto ParseDouble(std::string_view input) -> Result<double>
    {
        return ParseFloating<double>(input);
    }

    export [[nodiscard]]
    inline auto ParseLongDouble(std::string_view input) -> Result<long double>
    {
        return ParseFloating<long double>(input);
    }

    export template <class T>
        requires ParseInteger<T> || std::floating_point<T>
    [[nodiscard]]
    auto ParseNumber(std::string_view input) -> Result<T>
    {
        if constexpr (ParseInteger<T>) {
            return ParseIntegral<T>(input);
        } else {
            return ParseFloating<T>(input);
        }
    }
    
    export template <ParseInteger T>
    [[nodiscard]]
    auto ParseHex(std::string_view input) -> Result<T>
    {
        const auto original = input;

        input = Internal::TrimAsciiView(input);

        if constexpr (std::signed_integral<T>) {
            bool negative = false;

            if (!input.empty() && input.front() == '-') {
                negative = true;
                input.remove_prefix(1);
            }

            input = Internal::StripHexPrefix(input);

            if (input.empty())
                return Internal::ParseFailure<T>("ParseHex", original, "empty hex value");

            if (negative) {
                std::string normalized;
                normalized.reserve(input.size() + 1);
                normalized += '-';
                normalized += input;

                return ParseIntegral<T>(normalized, 16);
            }

            return ParseIntegral<T>(input, 16);
        } else {
            if (!input.empty() && input.front() == '-')
                return Internal::ParseFailure<T>("ParseHex", original, "negative value for unsigned integer");

            if (!input.empty() && input.front() == '+')
                input.remove_prefix(1);

            input = Internal::StripHexPrefix(input);

            if (input.empty())
                return Internal::ParseFailure<T>("ParseHex", original, "empty hex value");

            return ParseIntegral<T>(input, 16);
        }
    }

    export [[nodiscard]]
    inline auto HexToInt64(std::string_view input) -> Result<int64_t>
    {
        return ParseHex<int64_t>(input);
    }

    export [[nodiscard]]
    inline auto HexToUInt32(std::string_view input) -> Result<uint32_t>
    {
        return ParseHex<uint32_t>(input);
    }

    export enum class BoolParseMode
    {
        Strict,  // true / false
        Relaxed  // true / false / 1 / 0 / yes / no / on / off
    };

    export [[nodiscard]]
    inline auto ParseBool(std::string_view input,
                          BoolParseMode mode = BoolParseMode::Relaxed) -> Result<bool>
    {
        const auto original = input;
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return Internal::ParseFailure<bool>("ParseBool", original, "empty input");

        if (Internal::IEqualsAscii(input, "true"))
            return true;

        if (Internal::IEqualsAscii(input, "false"))
            return false;

        if (mode == BoolParseMode::Strict)
            return Internal::ParseFailure<bool>("ParseBool", original, "expected `true` or `false`");

        if (input == "1" ||
            Internal::IEqualsAscii(input, "yes") ||
            Internal::IEqualsAscii(input, "on") ||
            Internal::IEqualsAscii(input, "enabled"))
            return true;

        if (input == "0" ||
            Internal::IEqualsAscii(input, "no") ||
            Internal::IEqualsAscii(input, "off") ||
            Internal::IEqualsAscii(input, "disabled"))
            return false;

        return Internal::ParseFailure<bool>("ParseBool", original, "not a boolean");
    }
    
    export template <class T>
        requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
    [[nodiscard]]
    auto Parse(std::string_view input) -> Result<T>
    {
        if constexpr (std::same_as<T, bool>) {
            return ParseBool(input);
        } else if constexpr (ParseInteger<T>) {
            return ParseIntegral<T>(input);
        } else {
            return ParseFloating<T>(input);
        }
    }
    
    export enum class CaseMode
    {
        Sensitive,
        InsensitiveAscii
    };

    export template <class E>
        requires std::is_enum_v<E>
    [[nodiscard]]
    auto ParseEnum(std::string_view input,
                   std::initializer_list<std::pair<std::string_view, E>> values,
                   CaseMode caseMode = CaseMode::InsensitiveAscii) -> Result<E>
    {
        const auto original = input;
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return Internal::ParseFailure<E>("ParseEnum", original, "empty input");

        for (const auto& [name, value] : values) {
            const bool match =
                caseMode == CaseMode::Sensitive
                    ? input == name
                    : Internal::IEqualsAscii(input, name);

            if (match)
                return value;
        }

        return Internal::ParseFailure<E>("ParseEnum", original, "unknown enum value");
    }
    
    export template <class T>
        requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
    [[nodiscard]]
    auto ParseOptional(std::string_view input) -> Result<std::optional<T>>
    {
        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return std::optional<T>{};

        auto parsed = Parse<T>(input);

        if (!parsed)
            return std::unexpected(parsed.error());

        return std::optional<T>{std::move(*parsed)};
    }
    
    export template <class T>
        requires ParseInteger<T> || std::floating_point<T> || std::same_as<T, bool>
    [[nodiscard]]
    auto ParseList(std::string_view input, char delimiter = ',') -> Result<std::vector<T>>
    {
        std::vector<T> result;

        input = Internal::TrimAsciiView(input);

        if (input.empty())
            return result;

        size_t pos = 0;

        while (true) {
            const size_t next = input.find(delimiter, pos);

            const std::string_view part =
                next == std::string_view::npos
                    ? input.substr(pos)
                    : input.substr(pos, next - pos);

            auto parsed = Parse<T>(part);

            if (!parsed)
                return std::unexpected(parsed.error());

            result.emplace_back(std::move(*parsed));

            if (next == std::string_view::npos)
                break;

            pos = next + 1;
        }

        return result;
    }
    
}
