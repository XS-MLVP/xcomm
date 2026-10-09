#include "xspcomm/xcomuse/range.h"
#include "xspcomm/xdata.h"

#include <algorithm>
#include <cstring>

namespace xspcomm {

namespace {
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

template <typename ReadA, typename ReadB>
bool range_compare(size_t words, int range, ReadA a, ReadB b) {
    const uint32_t limit = static_cast<uint32_t>(range >= 0 ? range : -static_cast<int64_t>(range));
    uint64_t borrow = 0;
    for (size_t i = 0; i < words; ++i) {
        const uint64_t lhs = range >= 0 ? b(i) : a(i);
        const uint64_t rhs = (range >= 0 ? a(i) : b(i)) + borrow;
        const uint32_t difference = static_cast<uint32_t>(lhs - rhs);
        borrow = lhs < rhs;
        // The tolerance fits in the low word; all higher difference words must be zero.
        if (difference > (i == 0 ? limit : 0U)) return false;
    }
    return borrow == 0;
}
}

template <size_t Bytes>
bool ComUseRangeCheck::Compare(uint64_t a, uint64_t b, uint64_t self) {
    const auto* check = reinterpret_cast<const ComUseRangeCheck*>(self);
    const auto target = UIntBytes<Bytes>::Load(reinterpret_cast<const unsigned char*>(a));
    const auto center = UIntBytes<Bytes>::Load(reinterpret_cast<const unsigned char*>(b));
    return cmp(target, center, check->range);
}

const ComUseRangeCheck::ArrayCompare ComUseRangeCheck::comparisons[] = {
    Compare<1>, Compare<2>, Compare<3>, Compare<4>,
    Compare<5>, Compare<6>, Compare<7>, Compare<8>,
};

bool ComUseRangeCheck::ArrayWideCmp(uint64_t a, uint64_t b, uint64_t self) {
    const auto* check = reinterpret_cast<const ComUseRangeCheck*>(self);
    const size_t bytes = check->bytes;
    auto word = [bytes](uint64_t address, size_t i) {
        const size_t offset = i * 4;
        const size_t count = std::min(sizeof(uint32_t), bytes - offset);
        const auto* data = reinterpret_cast<const unsigned char*>(address) + offset;
        if (count == sizeof(uint32_t)) return UIntBytes<4>::Load(data);
        switch (count) {
        case 1: return UIntBytes<1>::Load(data);
        case 2: return UIntBytes<2>::Load(data);
        default: return UIntBytes<3>::Load(data);
        }
    };
    return range_compare((bytes + 3) / 4, check->range,
                         [=](size_t i) { return word(a, i); },
                         [=](size_t i) { return word(b, i); });
}

bool ComUseRangeCheck::XDataCmp(XData* a, XData* b, uint64_t self) {
    // Refresh each signal once; subsequent word reads use its existing cache.
    const uint64_t av = a->U();
    const uint64_t bv = b->U();
    if (!a->DataValid() || !b->DataValid()) return false;
    const int range = reinterpret_cast<const ComUseRangeCheck*>(self)->range;
    if (a->W() <= 64 && b->W() <= 64) return cmp(av, bv, range);
    const size_t awords = (static_cast<uint64_t>(a->W()) + 31) / 32;
    const size_t bwords = (static_cast<uint64_t>(b->W()) + 31) / 32;
    return range_compare(std::max(awords, bwords), range,
                         [=](size_t i) {
                             if (i == 0) return static_cast<uint32_t>(av);
                             return i < awords ? a->pVecData[i].aval : 0U;
                         },
                         [=](size_t i) {
                             if (i == 0) return static_cast<uint32_t>(bv);
                             return i < bwords ? b->pVecData[i].aval : 0U;
                         });
}

} // namespace xspcomm
