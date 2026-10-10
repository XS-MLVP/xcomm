#include "xspcomm/xcomuse/range.h"
#include "xspcomm/xdata.h"

#include <algorithm>

namespace xspcomm {

template <size_t Bytes>
bool ComUseRangeCheck::Compare(uint64_t a, uint64_t b, uint64_t self) {
    const auto* check = reinterpret_cast<const ComUseRangeCheck*>(self);
    const auto target = memory::UIntBytes<Bytes>::Load(reinterpret_cast<const unsigned char*>(a));
    const auto center = memory::UIntBytes<Bytes>::Load(reinterpret_cast<const unsigned char*>(b));
    return cmp(target, center, check->range);
}

const ComUseRangeCheck::ArrayCompare ComUseRangeCheck::comparisons[] = {
    Compare<1>, Compare<2>, Compare<3>, Compare<4>,
    Compare<5>, Compare<6>, Compare<7>, Compare<8>,
};

bool ComUseRangeCheck::ArrayWideCmp(uint64_t a, uint64_t b, uint64_t self) {
    const auto* check = reinterpret_cast<const ComUseRangeCheck*>(self);
    const size_t bytes = check->bytes;
    const memory::ByteWords target{reinterpret_cast<const unsigned char*>(a), bytes};
    const memory::ByteWords center{reinterpret_cast<const unsigned char*>(b), bytes};
    return compare::WithinRange(target.Count(), check->range, target, center);
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
    return compare::WithinRange(std::max(awords, bwords), range,
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
