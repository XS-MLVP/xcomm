#include "xspcomm/xcomm.h"
#include "xspcomm/xinstance.h"
#include <algorithm>
#include <limits>
#include <utility>

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

static void test_native_storage()
{
    auto check = [](unsigned width, auto value) {
        auto storage = std::make_unique<decltype(value)>(value);
        XData signal(width, XData::InOut);
        signal.BindNativeData(reinterpret_cast<uint64_t>(storage.get()));
        Assert(signal.U() == value, "XData(%u) native read failed", width);
        signal.AsImmWrite();
        signal.Set(0x5A);
        const auto expected = width == 1 ? 0 : 0x5A;
        Assert(*storage == expected, "XData(%u) native write failed", width);
        *storage = value;
        Assert(signal.U() == value, "XData(%u) native refresh failed", width);
    };
    check(1, uint8_t(1));
    check(8, uint8_t(0xA5));
    check(9, uint16_t(0x1A5));
    check(16, uint16_t(0xBEEF));
    check(32, uint32_t(0xDEADBEEF));
    check(64, uint64_t(0x89ABCDEF01234567));
}

static void test_native_vector_boundaries()
{
    std::vector<unsigned> widths = {1, 8, 9, 16, 17, 31, 32, 33, 63, 64};
    for (unsigned words = 3; words <= 18; ++words) {
        widths.push_back(words * 32 - 1);
        widths.push_back(words * 32);
    }
    widths.push_back(1024);
    for (unsigned width : widths) {
        const unsigned words = (width + 31) / 32;
        const unsigned bytes = width <= 8 ? 1 : width <= 16 ? 2 : words * 4;
        // Leave exactly the native storage size after an unaligned address.
        auto storage = std::make_unique<unsigned char[]>(bytes + 1);
        storage[0] = 0xA5;
        auto* native = storage.get() + 1;
        std::vector<uint32_t> expected(words), actual(words);
        const uint32_t last_mask = width % 32 ? (uint32_t(1) << (width % 32)) - 1 : UINT32_MAX;
        for (unsigned i = 0; i < words; ++i) expected[i] = 0x9E3779B9U * (i + 1);
        expected.back() &= last_mask;
        auto store_native = [&](unsigned char* dst) {
            if (width <= 8) {
                *dst = static_cast<uint8_t>(expected[0]);
            } else if (width <= 16) {
                const auto value = static_cast<uint16_t>(expected[0]);
                std::memcpy(dst, &value, sizeof(value));
            } else {
                std::memcpy(dst, expected.data(), bytes);
            }
        };
        store_native(native);
        XData signal(width, XData::InOut);
        signal.BindNativeData(reinterpret_cast<uint64_t>(native));
        signal.AsImmWrite();
        Assert(signal.GetBits(actual.data(), words) && actual == expected,
               "XData(%u) native vector read failed", width);

        for (unsigned i = 0; i < words; ++i) {
            signal.pVecData[i].bval = 1;
            expected[i] = ~expected[i];
        }
        expected.back() &= last_mask;
        store_native(native);
        Assert(!signal.GetBits(actual.data(), words) && actual == expected,
               "XData(%u) native refresh changed the X/Z mask", width);
        for (unsigned i = 0; i < words; ++i)
            Assert(signal.pVecData[i].bval == 1, "native read changed bval[%u]", i);

        for (auto& word : expected) word ^= 0xA5A5A5A5U;
        expected.back() &= last_mask;
        signal.SetBits(expected.data(), words);
        std::vector<unsigned char> packed(bytes);
        store_native(packed.data());
        Assert(std::memcmp(native, packed.data(), bytes) == 0 && storage[0] == 0xA5,
               "XData(%u) native vector write crossed a boundary", width);
    }
}

static void test_native_reinit()
{
    for (auto widths : {std::make_pair(128U, 32U), std::make_pair(32U, 544U),
                        std::make_pair(544U, 64U), std::make_pair(512U, 513U)}) {
        std::vector<uint32_t> native(18, 0x12345678);
        XData signal(widths.first, XData::InOut);
        signal.BindNativeData(reinterpret_cast<uint64_t>(native.data()));
        signal.ReInit(widths.second, XData::InOut);
        signal.AsImmWrite();
        const unsigned words = (widths.second + 31) / 32;
        const uint32_t last_mask = widths.second % 32 ?
            (uint32_t(1) << (widths.second % 32)) - 1 : UINT32_MAX;
        std::vector<uint32_t> actual(words), expected(words, 0x12345678);
        expected.back() &= last_mask;
        Assert(signal.GetBits(actual.data(), words) && actual == expected,
               "native read used the old width after ReInit");
        expected.assign(words, 0x89ABCDEF);
        expected.back() &= last_mask;
        signal.SetBits(expected.data(), words);
        Assert(std::equal(expected.begin(), expected.end(), native.begin()) &&
               native[words] == 0x12345678,
               "native write used the old width after ReInit");
    }
}

static void test_signed_values_and_slices()
{
    XData full(64, XData::InOut);
    for (int64_t value : {INT64_MIN, INT64_MIN + 1, int64_t(-123),
                          int64_t(-1), int64_t(0), INT64_MAX}) {
        full.Set(value);
        Assert(full.S() == value, "64-bit signed round-trip failed");
    }
    for (unsigned width : {8U, 31U, 32U, 63U}) {
        XData signal(width, XData::InOut);
        signal.Set(-123);
        Assert(signal.S() == -123, "XData(%u) sign extension failed", width);
    }
    full.Set(uint64_t(0x8000000000000001));
    auto slice = full.SubDataRef(1, 62);
    slice->Set(UINT64_MAX);
    Assert(full.U() == UINT64_MAX, "slice ending at bit 63 lost bits");
    slice->Set(0);
    Assert(full.U() == UINT64_C(0x8000000000000001), "slice overwrote adjacent bits");
}

static void test_bit_buffer_boundaries()
{
    XData signal(32, XData::InOut);
    uint32_t values[] = {0xA5, 0x5A};
    uint32_t mask = 0xFFFF;
    signal.Set(0x12345678);
    for (uint32_t start : {uint32_t(1), uint32_t(2), UINT32_MAX}) {
        signal.SetBits(values, 1, nullptr, start);
        signal.SetBits(values, 1, &mask, start);
        Assert(signal.U() == 0x12345678, "out-of-range SetBits changed the signal");
    }
    signal.SetBits(static_cast<uint32_t *>(nullptr), uint32_t(0), nullptr, UINT32_MAX);
    signal.SetBits(values, 1, &mask);
    Assert(signal.U() == 0x123400A5, "masked SetBits failed");
    signal.SetBits(values, 2);
    Assert(signal.U() == 0xA5, "SetBits did not clip to the signal size");

    const uint64_t initial = UINT64_C(0x89ABCDEF01234567);
    for (int shift : {0, 1, 31, 32, 33, 63, 64, 65, 95, 96,
                      -1, -31, -32, -33, -63, -64, -65, -95, -96,
                      std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        int words[] = {int(uint32_t(initial)), int(uint32_t(initial >> 32))};
        big_shift(words, 2, shift);
        const uint64_t actual = uint32_t(words[0]) | (uint64_t(uint32_t(words[1])) << 32);
        uint64_t expected = 0;
        if (shift >= 0 && shift < 64) expected = initial >> shift;
        if (shift < 0 && shift > -64) expected = initial << -shift;
        Assert(actual == expected, "big_shift failed for shift %d", shift);
    }
    big_shift(nullptr, 0, std::numeric_limits<int>::min());
}

int main(int argsc, const char **argsv)
{
    Debug("version: %s", version().c_str());
    checkVersion();
    test_optimized_signal_buffers();
    test_native_storage();
    test_native_vector_boundaries();
    test_native_reinit();
    test_signed_values_and_slices();
    test_bit_buffer_boundaries();
    test_xdata();
    return 0;
}
