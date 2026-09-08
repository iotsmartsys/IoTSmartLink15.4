#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace issp
{

enum class IsspValueType : std::uint8_t
{
    Int32 = 0,
    Float32 = 1,
    Invalid = 255,
};

/// Fixed-storage value. Bits are the canonical numeric content, never a native
/// wire struct. Invalid conversions remain invalid until admission rejects them.
struct IsspValue
{
    IsspValueType type{IsspValueType::Int32};
    std::uint32_t bits{0};

    constexpr IsspValue() = default;

    template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    constexpr IsspValue(T integer)
    {
        if constexpr (std::is_signed_v<T>)
        {
            if (integer < INT32_MIN || integer > INT32_MAX)
            {
                type = IsspValueType::Invalid;
                return;
            }
        }
        else if (integer > static_cast<std::uint32_t>(INT32_MAX))
        {
            type = IsspValueType::Invalid;
            return;
        }
        bits = static_cast<std::uint32_t>(integer);
    }

    static IsspValue floating(float number)
    {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                      "ISSP requires IEEE 754 binary32");
        IsspValue value;
        value.type = IsspValueType::Float32;
        std::memcpy(&value.bits, &number, sizeof(number));
        return value;
    }

    constexpr bool isValid() const
    {
        return type == IsspValueType::Int32 ||
               (type == IsspValueType::Float32 &&
                (bits & 0x7f800000U) != 0x7f800000U);
    }

    constexpr bool isCanonical() const
    {
        return isValid() && !(type == IsspValueType::Float32 && bits == 0x80000000U);
    }

    void normalizeZero()
    {
        if (type == IsspValueType::Float32 && bits == 0x80000000U)
        {
            bits = 0;
        }
    }

    constexpr std::int32_t integer() const
    {
        return bits <= static_cast<std::uint32_t>(INT32_MAX)
                   ? static_cast<std::int32_t>(bits)
                   : -1 - static_cast<std::int32_t>(~bits);
    }

    friend constexpr bool operator==(const IsspValue &left, const IsspValue &right)
    {
        return left.type == right.type && left.bits == right.bits;
    }
};

} // namespace issp
