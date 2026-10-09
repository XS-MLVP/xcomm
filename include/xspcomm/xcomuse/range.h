#ifndef XSPCOMM_XCOMUSE_RANGE_H
#define XSPCOMM_XCOMUSE_RANGE_H

#include "xspcomm/xcallback.h"
#include "xspcomm/xutil.h"
#include <cstring>

namespace xspcomm {

    class XData;

    class ComUseRangeCheck {
        int bytes;
        int range;

        template <size_t Bytes>
        static bool Compare(uint64_t a, uint64_t b, uint64_t self);
        static bool ArrayWideCmp(uint64_t a, uint64_t b, uint64_t self);
        using ArrayCompare = bool (*)(uint64_t, uint64_t, uint64_t);
        static const ArrayCompare comparisons[8];
        static ArrayCompare Select(int bytes) {
            return bytes <= 8 ? comparisons[bytes - 1] : ArrayWideCmp;
        }
    public:
        ComUseRangeCheck(int range, int bytes) : bytes(bytes), range(range) {
            Assert(bytes >= 1, "Need bytes >= 1");
        }

        static bool cmp(uint64_t target, uint64_t center, int range) {
            if (range >= 0) {
                return target <= center && center - target <= static_cast<uint32_t>(range);
            }
            return target >= center && target - center <= static_cast<uint64_t>(-static_cast<int64_t>(range));
        }

        static bool ArrayCmp(uint64_t a, uint64_t b, uint64_t self) {
            const auto* check = reinterpret_cast<const ComUseRangeCheck*>(self);
            if (check->bytes == sizeof(uint64_t)) {
                uint64_t target, center;
                std::memcpy(&target, reinterpret_cast<const void*>(a), sizeof(target));
                std::memcpy(&center, reinterpret_cast<const void*>(b), sizeof(center));
                return cmp(target, center, check->range);
            }
            return Select(check->bytes)(a, b, self);
        }
        static bool XDataCmp(XData* a, XData* b, uint64_t self);

        uint64_t CSelf() {
            return reinterpret_cast<uint64_t>(this);
        }

        xfunction<bool, uint64_t, uint64_t, uint64_t> GetArrayCmp() {
            // Select once when creating the callback, outside the hot path.
            return Select(this->bytes);
        }
        xfunction<bool, XData*, XData*, uint64_t> GetXDataCmp() {
            return XDataCmp;
        }
    };

} // namespace xspcomm

#endif
