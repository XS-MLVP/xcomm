#ifndef XSPCOMM_COMMON_COMPARE_H
#define XSPCOMM_COMMON_COMPARE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xspcomm::compare {

// Operation enums supply EQ/NE/GT/GE/LT/LE; no dependency on their owners.
template <auto Operation, typename Left, typename Right>
bool Compare(Left&& lhs, Right&& rhs) {
    using Op = decltype(Operation);
    if constexpr (Operation == Op::EQ) return lhs == rhs;
    else if constexpr (Operation == Op::NE) return !(lhs == rhs);
    else if constexpr (Operation == Op::GT) return lhs > rhs;
    else if constexpr (Operation == Op::GE) return lhs >= rhs;
    else if constexpr (Operation == Op::LT) return lhs < rhs;
    else if constexpr (Operation == Op::LE) return lhs <= rhs;
    else return false;
}

template <typename Op, typename Left, typename Right>
bool Compare(Op operation, Left&& lhs, Right&& rhs) {
    switch (operation) {
    case Op::EQ: return Compare<Op::EQ>(lhs, rhs);
    case Op::NE: return Compare<Op::NE>(lhs, rhs);
    case Op::GT: return Compare<Op::GT>(lhs, rhs);
    case Op::GE: return Compare<Op::GE>(lhs, rhs);
    case Op::LT: return Compare<Op::LT>(lhs, rhs);
    case Op::LE: return Compare<Op::LE>(lhs, rhs);
    default: return false;
    }
}

template <size_t Bytes> struct UIntBytes;

template <typename T>
struct UIntWord {
    static uint64_t Load(const unsigned char* data) {
        T value;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }
};

template <size_t LowBytes, size_t HighBytes>
struct UIntParts {
    static uint64_t Load(const unsigned char* data) {
        return UIntBytes<LowBytes>::Load(data) |
               (UIntBytes<HighBytes>::Load(data + LowBytes) << (LowBytes * 8));
    }
};

// Fixed byte widths use native integers or consecutive low/high parts.
#define XSPCOMM_UINT_BYTES(Bytes, ...) \
    template <> struct UIntBytes<Bytes> : __VA_ARGS__ {};
XSPCOMM_UINT_BYTES(1, UIntWord<uint8_t>)
XSPCOMM_UINT_BYTES(2, UIntWord<uint16_t>)
XSPCOMM_UINT_BYTES(3, UIntParts<2, 1>)
XSPCOMM_UINT_BYTES(4, UIntWord<uint32_t>)
XSPCOMM_UINT_BYTES(5, UIntParts<4, 1>)
XSPCOMM_UINT_BYTES(6, UIntParts<4, 2>)
XSPCOMM_UINT_BYTES(7, UIntParts<6, 1>)
XSPCOMM_UINT_BYTES(8, UIntWord<uint64_t>)
#undef XSPCOMM_UINT_BYTES

// A non-owning reader for little-endian buffers, including a partial last word.
struct ByteWords {
    const unsigned char* data;
    size_t bytes;

    size_t Count() const { return bytes / 4 + (bytes % 4 != 0); }

    uint32_t operator()(size_t i) const {
        const size_t offset = i * 4;
        const size_t count = std::min(sizeof(uint32_t), bytes - offset);
        const auto* word = data + offset;
        if (count == sizeof(uint32_t)) return UIntBytes<4>::Load(word);
        switch (count) {
        case 1: return UIntBytes<1>::Load(word);
        case 2: return UIntBytes<2>::Load(word);
        default: return UIntBytes<3>::Load(word);
        }
    }
};

template <typename T>
int Order(T lhs, T rhs) {
    return lhs < rhs ? -1 : lhs > rhs ? 1 : 0;
}

// Readers provide words from least to most significant; ordering scans backward.
template <typename ReadA, typename ReadB>
inline int CompareWords(size_t words, ReadA a, ReadB b) {
    while (words != 0) {
        --words;
        const auto lhs = a(words);
        const auto rhs = b(words);
        if (lhs != rhs) return lhs < rhs ? -1 : 1;
    }
    return 0;
}

// Equality permits zero extension. A word may also contain a four-state mask.
template <typename ReadA, typename ReadB>
inline bool EqualWords(size_t awords, size_t bwords, ReadA a, ReadB b) {
    const size_t common = std::min(awords, bwords);
    for (size_t i = common; i < awords; ++i) if (a(i) != 0) return false;
    for (size_t i = common; i < bwords; ++i) if (b(i) != 0) return false;
    for (size_t i = 0; i < common; ++i) if (a(i) != b(i)) return false;
    return true;
}

// Equal-width two's-complement buffers; bytes must be positive.
inline int CompareSignedBytes(const unsigned char* a, const unsigned char* b, size_t bytes) {
    const bool anegative = (a[bytes - 1] & 0x80) != 0;
    const bool bnegative = (b[bytes - 1] & 0x80) != 0;
    if (anegative != bnegative) return anegative ? -1 : 1;
    // Within the same sign, two's-complement values retain unsigned ordering.
    const ByteWords lhs{a, bytes};
    const ByteWords rhs{b, bytes};
    return CompareWords(lhs.Count(), lhs, rhs);
}

inline bool WithinRange(uint64_t target, uint64_t center, int range) {
    if (range >= 0) {
        return target <= center && center - target <= static_cast<uint32_t>(range);
    }
    return target >= center && target - center <= static_cast<uint64_t>(-static_cast<int64_t>(range));
}

// Range checks subtract low words first; the tolerance fits in one 32-bit word.
template <typename ReadA, typename ReadB>
bool WithinRange(size_t words, int range, ReadA a, ReadB b) {
    const uint32_t limit = static_cast<uint32_t>(range >= 0 ? range : -static_cast<int64_t>(range));
    uint64_t borrow = 0;
    for (size_t i = 0; i < words; ++i) {
        const uint64_t lhs = range >= 0 ? b(i) : a(i);
        const uint64_t rhs = (range >= 0 ? a(i) : b(i)) + borrow;
        const uint32_t difference = static_cast<uint32_t>(lhs - rhs);
        borrow = lhs < rhs;
        if (difference > (i == 0 ? limit : 0U)) return false;
    }
    return borrow == 0;
}

} // namespace xspcomm::compare

#endif
