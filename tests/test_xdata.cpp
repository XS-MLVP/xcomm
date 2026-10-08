#include "xspcomm/xcomm.h"
#include "xspcomm/xinstance.h"

using namespace xspcomm;

static void test_optimized_signal_buffers()
{
    for (unsigned width : {1U, 8U, 16U, 31U, 32U, 33U, 63U, 64U}) {
        XData signal(width, XData::InOut);
        const uint64_t mask = width == 64 ? UINT64_MAX : (uint64_t(1) << width) - 1;
        for (uint64_t value : {uint64_t(0), uint64_t(1), uint64_t(0xA5),
                               uint64_t(0x89ABCDEF01234567), UINT64_MAX}) {
            signal.Set(value);
            const uint64_t actual = signal.U();
            Assert(actual == (value & mask),
                   "XData(%u) round-trip: expected 0x%llx, got 0x%llx", width,
                   static_cast<unsigned long long>(value & mask),
                   static_cast<unsigned long long>(actual));
            Assert(signal.XMask() == 0 && signal.DataValid(),
                   "XData(%u) integer assignment retained unknown bits", width);
        }

        signal.pVecData[0].bval = 1;
        const uint64_t unknown = width > 32 ? (uint64_t(1) << 32) | 1 : 1;
        if (width > 32) signal.pVecData[1].bval = 1;
        Assert(signal.XMask() == unknown && !signal.DataValid(),
               "XData(%u) lost its unknown-bit mask", width);
    }
}

int main(int argsc, const char **argsv)
{
    Debug("version: %s", version().c_str());
    checkVersion();
    test_optimized_signal_buffers();
    test_xdata();
    return 0;
}
