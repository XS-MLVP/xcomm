#include "xspcomm/xcomm.h"
#include "xspcomm/xmonitor.h"

namespace xspcomm {

#define test_assert(c, fmt, ...)      \
    {                                 \
        if (!(c))                     \
        {                             \
            fails++;                  \
            Error(fmt, ##__VA_ARGS__) \
        }                             \
        else                          \
        {                             \
            success++;                \
        }                             \
    }

int test_xdata()
{
    int fails = 0, success = 0;
    // norm types convert
    XData x1 = (u_int8_t)0xf1;
    XData x2 = (u_int16_t)0xf2;
    XData x3 = (u_int32_t)0xf3;
    XData x4 = (u_int64_t)0xf4;
    test_assert(x1 == 0xf1 && x1.mWidth == 8 && x1.DataValid(),
                "check u_int8   fail");
    test_assert(x2 == 0xf2 && x2.mWidth == 16 && x2.DataValid(),
                "check u_int16  fail");
    test_assert(x3 == 0xf3 && x3.mWidth == 32 && x3.DataValid(),
                "check u_int32  fail");
    test_assert(x4 == 0xf4 && x4.mWidth == 64 && x4.DataValid(),
                "check u_int64  fail");
    XData w4(4, XData::In);
    w4 = -1;
    test_assert(w4.S() == -1, "w4 need be -1 (%lld)", w4.S());
    XData w0(0, XData::In);
    XData w1(1, XData::In);
    w0 =  1;
    w1 = -1;
    test_assert(w0.S() == 1 && w1.S() == -1, "w0 need be 1(%lld) and w1 need be -1(%lld)", w0.S(), w1.S());

    XData zx_z;
    XData zx_x(32, XData::In);
    zx_z = "z";
    zx_x = "0xf123faxz";
    test_assert(zx_z == "z", "xz=%s", zx_z.String().c_str());
    zx_z = "x";
    test_assert(zx_z == "x", "xz=%s", zx_z.String().c_str());
    test_assert(zx_x == "f123fa??", "xx=%s", zx_x.String().c_str());
    zx_x.value = 0xff;
    test_assert(zx_x == 0xff, "set value(0xff) fail zx_x: %s", zx_x.String().c_str());
    test_assert(zx_x.value == 0xff, "read value(0xff) fail zx_x: %s", zx_x.String().c_str());
    test_assert(zx_x.value == zx_x, "value == *self fail");
    uint32_t value = zx_x.value;
    test_assert(value == zx_x, "value == *self fail");

    XData x5 = (int8_t)0xf1;
    XData x6 = (int16_t)0xf2;
    XData x7 = (int32_t)0xf3;
    XData x8 = (int64_t)0xf4;
    test_assert((int8_t)x5 == (int8_t)0xf1 && x5.mWidth == 8 && x5.DataValid(),
                "check int8(0x%s=%d) width=%d   fail", x5.String().c_str(),
                (int8_t)x5, x5.mWidth);
    test_assert(x6 == 0xf2 && x6.mWidth == 16 && x6.DataValid(),
                "check int16(0x%x)  fail", (int16_t)x6);
    test_assert(x7 == 0xf3 && x7.mWidth == 32 && x7.DataValid(),
                "check int32  fail");
    test_assert(x8 == 0xf4 && x8.mWidth == 64 && x8.DataValid(),
                "check int64  fail");

    // test width miss match
    XData y1(8 - 2, XData::In), y2(8 + 1, XData::In), y3(32 + 1, XData::In),
        y4(32 - 3, XData::In);
    y1 = 0xff;
    y2 = 0xff;
    y3 = 0xffffffff;
    y4 = 0xffffffff;
    test_assert(y1 == (0xff & ((1 << 6) - 1)) && y1.DataValid(),
                "check 0x%s width = %d fail", y1.String().c_str(), y1.mWidth);

    test_assert(y2 == (0xff & ((1 << 9) - 1)) && y2.DataValid(),
                "check width = %d fail", y2.mWidth);
    test_assert(y3 == (0xffffffff & (((u_int64_t)1 << 33) - 1))
                    && y3.DataValid(),
                "check width = %d fail", y3.mWidth);
    test_assert(y4 == (0xffffffff & ((1 << 29) - 1)) && y4.DataValid(),
                "check width = %d fail", y4.mWidth);
    test_assert(y2 == XData(0xff) && y2.DataValid(),
                "check %d [%d] = %d [%d], fail", (int)y1, y1.mWidth, (int)y2,
                y2.mWidth);

    // test set/get bit
    test_assert(y1[2] == 1 && y1.DataValid(), "check (w=%d)getbits fail",
                y1.mWidth);
    test_assert(y3[32] == 0 && y3.DataValid(), "check (w=%d)getbits fail",
                y1.mWidth);
    y1[2]  = 0;
    y3[32] = 1;
    test_assert(y1[2] == 0 && y1.DataValid(), "check (w=%d)setbits fail",
                y1.mWidth);
    test_assert(y3[32] == 1 && y3.DataValid(), "check (w=%d)setbits fail",
                y1.mWidth);

    // big width set
    XData z(128, XData::In);
    test_assert(z.DataValid(), "check DataValid fail");

    z                = 0x123456789ABCDEF;
    u_int32_t bts[4] = {0, 0, 0xff123456, 0xabcdefab};
    u_int32_t msk[4] = {0, 0, 0xffffffff, 0xffffffff};
    test_assert(z == 0x123456789ABCDEF, "xdata[%d] = 0x%s, origin", z.mWidth,
                z.String().c_str());
    test_assert(
        "00000000000000000123456789abcdef" == z.String() && z.DataValid(),
        "xdata[%d] = 0x%s, direct assign", z.mWidth, z.String().c_str());

    z.SetBits(bts, 4);
    test_assert(
        "abcdefabff1234560000000000000000" == z.String() && z.DataValid(),
        "xdata[%d] = 0x%s, direct assign", z.mWidth, z.String().c_str());

    z = 0x123456789ABCDEF;
    z.SetBits(bts, 4, msk);
    test_assert("abcdefabff1234560123456789abcdef" == z.String()
                    && z.DataValid(),
                "xdata[%d] = 0x%s, with mask", z.mWidth, z.String().c_str());

    u_int8_t ubts[3] = {0x12, 0x34, 0x56};
    u_int8_t umsk[3] = {0xf0, 0x1f, 0x0f};
    z                = 0;
    z.SetBits(ubts, 3, nullptr, 2);
    test_assert("00000000000000000000005634120000" == z.String()
                    && z.DataValid(),
                "z=  %s", z.String().c_str());

    z = 0;
    z.SetBits(ubts, 3, umsk, 2);
    test_assert("00000000000000000000000614100000" == z.String()
                    && z.DataValid(),
                "z=  %s", z.String().c_str());

    // A masked partial write must take the masked-out bits from the *same*
    // byte it is about to overwrite. The previous cases all start from zero,
    // so an offset error read from the wrong byte goes unnoticed.
    {
        XData masked(16, IOType::InOut, "masked_bits");
        masked.Set(int(0x1234));
        u_int8_t clear_low[1] = {0x00};
        u_int8_t low_nibble[1] = {0x0f};
        masked.SetBits(clear_low, 1, low_nibble, 1); // byte 1, keep its high nibble
        test_assert((masked.U() & 0xffff) == 0x1034,
                    "masked SetBits must keep byte 1 high nibble, got 0x%llx",
                    (unsigned long long)masked.U());
    }

    // Byte-addressed writes span the 64-bit boundary; only the callback value
    // is limited to 64 bits (see the on-change case above).
    {
        XData wide(128, IOType::InOut, "wide_bits");
        std::string initial = "0x112233445566778899aabbccddeeff00";
        wide.Set(initial);
        u_int8_t clear_low[1]  = {0x00};
        u_int8_t low_nibble[1] = {0x0f};
        wide.SetBits(clear_low, 1, low_nibble, 8); // byte 8 == bits [71:64]
        test_assert(wide.String() == "112233445566778099aabbccddeeff00",
                    "masked write above bit 64 must keep byte 8 high nibble, got %s",
                    wide.String().c_str());
    }

    // String() prints one hex byte per byte that starts below mWidth. A byte
    // whose first bit is exactly mWidth is entirely outside the signal and must
    // not be printed; a byte that starts below mWidth is only partially valid
    // but must still be printed. Widths that are multiples of 32 hide this,
    // because the phantom byte would start past the last 32-bit word.
    {
        auto show = [](int width) {
            XData d(width, IOType::InOut, "string_width");
            d.Set("0xab");
            return d.String();
        };
        test_assert(show(8) == "ab", "W=8 String=%s, need ab", show(8).c_str());
        test_assert(show(16) == "00ab", "W=16 String=%s, need 00ab",
                    show(16).c_str());
        test_assert(show(24) == "0000ab", "W=24 String=%s, need 0000ab",
                    show(24).c_str());
        test_assert(show(32) == "000000ab", "W=32 String=%s, need 000000ab",
                    show(32).c_str());
        test_assert(show(40) == "00000000ab", "W=40 String=%s, need 00000000ab",
                    show(40).c_str());
        test_assert(show(48) == "0000000000ab",
                    "W=48 String=%s, need 0000000000ab", show(48).c_str());
        test_assert(show(56) == "000000000000ab",
                    "W=56 String=%s, need 000000000000ab", show(56).c_str());
        test_assert(show(64) == "00000000000000ab",
                    "W=64 String=%s, need 00000000000000ab", show(64).c_str());
        test_assert(show(128) == "000000000000000000000000000000ab",
                    "W=128 String=%s, need 32 hex digits", show(128).c_str());

        // The partial high byte of a non-byte-aligned width stays visible.
        test_assert(show(116) == "0000000000000000000000000000ab",
                    "W=116 String=%s, need 15 bytes", show(116).c_str());
        XData partial(116, IOType::InOut, "string_partial");
        partial.Set("0xa0000000000000000000000000000"); // bit 115..112 only
        test_assert(partial.String() == "0a0000000000000000000000000000",
                    "W=116 partial high byte lost, String=%s",
                    partial.String().c_str());
        XData w93(93, IOType::InOut, "string_w93");
        w93.Set("0xf1010101010101010202020f");
        test_assert(w93.String() == "11010101010101010202020f",
                    "W=93 String=%s", w93.String().c_str());
        XData w9(9, IOType::InOut, "string_w9");
        w9.Set("0x1ab");
        test_assert(w9.String() == "01ab", "W=9 String=%s", w9.String().c_str());

        // Widths below one byte still print that single partial byte, and 127
        // bits keeps the seven-bit top byte (it starts at bit 120 < 127).
        XData w1(1, IOType::InOut, "string_w1");
        w1.Set("0x1");
        test_assert(w1.String() == "01", "W=1 String=%s", w1.String().c_str());
        XData w4(4, IOType::InOut, "string_w4");
        w4.Set("0xb");
        test_assert(w4.String() == "0b", "W=4 String=%s", w4.String().c_str());
        XData w7(7, IOType::InOut, "string_w7");
        w7.Set("0x7f");
        test_assert(w7.String() == "7f", "W=7 String=%s", w7.String().c_str());
        test_assert(show(127) == "000000000000000000000000000000ab",
                    "W=127 String=%s, need 32 hex digits", show(127).c_str());

        // operator==(const std::string&) compares this display text, so the
        // extra byte used to make short signals fail to compare with "ab".
        XData c8(8, IOType::InOut, "string_eq8");
        c8.Set("0xab");
        test_assert(c8 == "ab" && c8 == "AB", "W=8 == \"ab\" fail: %s",
                    c8.String().c_str());
        test_assert(!(c8 == "00ab"), "W=8 == \"00ab\" must fail: %s",
                    c8.String().c_str());
        XData c16(16, IOType::InOut, "string_eq16");
        c16.Set("0xab");
        test_assert(c16 == "00ab" && !(c16 == "0000ab"),
                    "W=16 == \"00ab\" fail: %s", c16.String().c_str());
        XData c116(116, IOType::InOut, "string_eq116");
        c116.Set("0xab");
        test_assert(c116 == "0000000000000000000000000000ab",
                    "W=116 compare fail: %s", c116.String().c_str());
    }

    // read from string
    z = "0b1111_0011_0111_0000"; // 0xcf0e
    test_assert(
        "00000000000000000000000000000ecf" == z.String() && z.DataValid(),
        "xdata[%d] = 0x%s, (from bin str)", z.mWidth, z.String().c_str());

    z = "0x1234_5678_9abc_def0";
    test_assert(
        "0000000000000000123456789abcdef0" == z.String() && z.DataValid(),
        "xdata[%d] = 0x%s, (from hex str)", z.mWidth, z.String().c_str());

    z = "0x12ffaaee";
    test_assert(0x12ffaaee == z.AsInt32() && z.DataValid(),
                "xdata[%d] = 0x%s, (from hex str)", z.mWidth,
                z.String().c_str());

    z = "::ABCD_EFGH_IJKL_MNOP_Q";
    test_assert(
        "504f4e4d4c4b4a494847464544434241" == z.String() && z.DataValid(),
        "xdata[%d] = 0x%s, (from txt str)", z.mWidth, z.String().c_str());

    z = "0b1111_1111_0000_1111";
    z[5] = "x";
    z[8] = "z";
    test_assert("1111000z11x11111" == z.AsBinaryString() && !z.DataValid(),
                "xdata[%d] = 0x%s, (from bin str)", z.mWidth, z.String().c_str());

    // test callback
    int count = 0;
    z.OnChange(
        [](bool v, XData *x, u_int64_t val, void *args) {
            // Debug("callback: data is vali: %d, x= %s, d: %d, args: %p", v,
            // x->String().c_str(), val, args);
            *(int *)args += 1;
        },
        &count);
    z = 0; // change 1
    z = 1; // change 2
    z = 1;
    z = 2; // change 3
    z = 2;
    z = 1; // change 4
    z = 1;
    z = 2; // change 5
    test_assert(count == 5 && z.DataValid(), "test call back fail, count=%d", count);

    // The callback signature carries a 64-bit value. A narrower internal
    // signature silently drops the high bits of any signal wider than 32 bits.
    {
        XData wide(64, IOType::InOut, "wide_change");
        u_int64_t seen = 0;
        wide.OnChange([&seen](bool, XData *, u_int64_t value, void *) { seen = value; });
        wide.Set(uint64_t(0x123456789abcdef0ull));
        test_assert(seen == 0x123456789abcdef0ull,
                    "on-change must carry the full 64-bit value, got 0x%llx",
                    (unsigned long long)seen);
    }

    // Documents the width limit: the cached callback value is the low 64 bits
    // of a wider signal. Callers that need more must read inside the callback
    // through the XData argument.
    {
        XData wider(128, IOType::InOut, "wider_change");
        std::string initial = "0x112233445566778899aabbccddeeff00";
        wider.Set(initial);
        u_int64_t seen = 0;
        int calls      = 0;
        wider.OnChange([&](bool, XData *, u_int64_t value, void *) {
            ++calls;
            seen = value;
        });
        std::string next = "0xfedcba98765432100123456789abcdef";
        wider.Set(next);
        test_assert(calls == 1 && seen == 0x0123456789abcdefull,
                    "on-change above 64 bits carries the low 64 bits, got 0x%llx (calls=%d)",
                    (unsigned long long)seen, calls);
    }

    std::vector<unsigned char> v = {0x12, 0x34, 0x56, 0x78};
    z.SetVU8(v);
    test_assert(
        "00000000000000000000000078563412" == z.String() && z.DataValid(),
        "xdata[%d] = 0x%s, (from vector)", z.mWidth, z.String().c_str());
    std::string s;
    for (auto &i : z.GetVU8()) { s += sFmt("%02x", i); }
    test_assert("12345678000000000000000000000000" == s,
                "xdata[%d] = 0x%s, (to vector)", z.mWidth, z.String().c_str());

// test connect
#define IOTYPES(iotype)                                                        \
    (iotype == IOType::Input ?                                                 \
         "IOType::Input" :                                                     \
         (iotype == IOType::Output ? "IOType::Output" : "IOType::InOut"))

    XData a(32, IOType::InOut, "A");
    XData b(32, IOType::Input, "B");
    b.OnChange([](bool v, XData *x, u_int64_t val, void *args) {});
    a.Connect(b);
    a = 1;
    b = 1;
    a = 2;
    test_assert(b == 2 && b.DataValid(),
                "test connect fail, b.mIOType=%s, a.mIOType=%s",
                IOTYPES(b.mIOType), IOTYPES(a.mIOType));

    a.Invert();
    test_assert(~uint32_t(2) == a, "~ check fail: %s", a.String().c_str());

    // test sub data
    XData full(128, IOType::InOut, "full");
    full = "0xffffffffffffffffffffffffffffffff";

    auto x30_64 = full.SubDataRef(30, 64);
    auto x30_93 = full.SubDataRef(30, 93);
    *x30_64 = "0xababa12ba98babab";

    test_assert("ffffffffeaeae84aea62eaeaffffffff" == full.String() && full.DataValid(), "data(0x%s) need be: 0xffffffffeaeae84aea62eaeaffffffff", full.String().c_str());
    test_assert("ababa12ba98babab" == x30_64->String() && x30_64->DataValid(), "data(0x%s) need be: ababa12ba98babab", x30_64->String().c_str());

    *x30_93 = "0xf1010101010101010202020f";

    test_assert("fc4040404040404040808083ffffffff" == full.String() && full.DataValid(), "data(0x%s) need be: fc4040404040404040808083ffffffff", full.String().c_str());
    test_assert("11010101010101010202020f" == x30_93->String() && x30_93->DataValid(), "data(0x%s) need be: 11010101010101010202020f", x30_93->String().c_str());

    auto x0_1 = full.SubDataRef(0, 1);
    auto x1_0 = full.SubDataRef(1, 0, "x1_0");
    *x0_1 = 0;
    *x1_0 = 0;
    test_assert("fc4040404040404040808083fffffffc" == full.String() && full.DataValid(), "data(0x%s) need be: fc4040404040404040808083fffffffc", full.String().c_str());

    // test XPort
    XPort p1("A_");
    XPort p2("B_");
    XData d1(32, IOType::InOut, "d1");
    XData d2(32, IOType::InOut, "d2");
    XPort p3("C_");
    p1.Add("x", d1);
    p2.Add("x", d2);
    p1.Connect(p2);
    d1 = 1;
    test_assert(d1 == d2, "connect fail, d1=%s, d2=%s", d1.String().c_str(), d2.String().c_str());
    d1 = 2;
    test_assert(d1 == d2, "connect fail, d1=%s, d2=%s", d1.String().c_str(), d2.String().c_str());
    d2 = 3;
    test_assert(d1 < d2, "d1=%s, d2=%s", d1.String().c_str(), d2.String().c_str());
    test_assert(d1 <= d2, "d1=%s, d2=%s", d1.String().c_str(), d2.String().c_str());
    test_assert(!(d1 > d2), "d1=%s, d2=%s", d1.String().c_str(), d2.String().c_str());
    test_assert(!(d1 >= d2), "d1=%s, d2=%s", d1.String().c_str(), d2.String().c_str());

    p3 = p1;
    test_assert(p1.String() == p3.String(), "copy fail, p1=%s, p3=%s", p1.String().c_str(), p3.String().c_str());
    p1.Add("y", d2);
    test_assert(p1.SelectPins({"x", "y"})["y"] == p1.SelectPins({"y"})["y"], "select fail, p1=%s, p3=%s", p1.String().c_str(), p3.String().c_str());

    // test XClock
    XClock clk1([](bool x){ return 0; });
    XClock clk2([](bool x){ return 0; });
    clk1.StepFal([](u_int64_t clk, void *args){
        //printf("------------------ %ld -F\n", 2*clk - 1);
    }, nullptr, "Fal-lambda");
    clk1.StepRis([](u_int64_t clk, void *args){
        //printf("------------------ %ld -R\n", 2*clk);
    }, nullptr, "Ris-lambda");

    clk2.StepRis([&clk1](u_int64_t clk, void *args){
        //printf("clk2-R: %ld\n",clk);
    }, nullptr, "Ris-lambda");
    clk2.StepFal([&clk1](u_int64_t clk, void *args){
        //printf("clk2-F: %ld\n",clk);
    }, nullptr, "Fal-lambda");
    clk1.FreqDivWith(5, clk2);
    clk1.Step(20);
    test_assert(clk1.clk == 20, "clk1: %lld", clk1.clk);
    test_assert(clk2.clk == 4, "clk2: %lld", clk2.clk);
    // test big operations
    u_int64_t big = 0x123456789abcdef0;
    u_int64_t big2 = 0xfedcba9876543210;
    u_int64_t big3 = 0x123456789abcdef0;
    int int_size = sizeof(u_int64_t)/sizeof(int);
    big_mask((int*)&big, int_size, 23, 60);
    std::string big_s = big_binstr((int*)&big, int_size);
    test_assert(big_s == "0000111111111111111111111111111111111111100000000000000000000000", "big_mask fail: %s", big_s.c_str());
    big_shift((int*)&big, int_size, 33);
    big_s = big_binstr((int*)&big, int_size);
    test_assert(big_s == "0000000000000000000000000000000000000111111111111111111111111111", "big_shift fail: %s", big_s.c_str());
    big_shift((int*)&big, int_size, -33);
    big_s = big_binstr((int*)&big, int_size);
    test_assert(big_s == "0000111111111111111111111111111000000000000000000000000000000000", "big_shift fail: %s", big_s.c_str());
    sync_data_to((int*)&big2, int_size, (int*)&(big), (int*)&big3);
    big_s = big_binstr((int*)&big3, int_size).c_str();
    test_assert(big_s == "0001111011011100101110101001100010011010101111001101111011110000", "sync_data_to fail: %s", big_s.c_str());

    // test xdata bind native data
    uint64_t value_lgc;
    uint64_t value_l64;
    uint64_t value_b64[] = {0, 1};
    XData xlc(0, XData::In, "xlc");
    XData x64(32, XData::In, "x64");
    XData x72(72, XData::In, "x72");
    x64.BindNativeData((uint64_t)&value_l64);
    xlc.BindNativeData((uint64_t)&value_lgc);
    x72.BindNativeData((uint64_t)value_b64);
    x64.AsImmWrite(); xlc.AsImmWrite(); x72.AsImmWrite();

    x64 = 0x1234; xlc = 1; x72 = 0x4321;
    test_assert(x64 == value_l64, "bind native data fail: %llx", value_l64);
    test_assert(xlc == value_lgc, "bind native data fail: %llx", value_lgc);
    test_assert(x72 == value_b64[0], "bind native data fail: %llx", value_b64[0]);
    value_l64 = 0x5678; value_lgc = 0; value_b64[0] = 0x789;
    test_assert(x64 == 0x5678, "bind native data fail: %llx", value_l64);
    test_assert(xlc == 0, "bind native data fail: %llx", value_lgc);
    test_assert(x72 == 0x789, "bind native data fail: %llx", value_b64[0]);

    // test xsignal cfg
    uint64_t cfg_base[] = {0,1,2,3,4,0xffffffffffffffff,6,7,8,9};
    std::string cfg_str = R"(variables:
  - name: Cache_top.clock
    type: CData
    mem_bytes: 1
    rtl_width: 1
    offset: 8
  - name: Cache_top.reset
    type: CData
    mem_bytes: 1
    rtl_width: 8
    offset: 46)";
    XSignalCFG cfg(cfg_str, (uint64_t)cfg_base);
    auto clk = cfg.NewXData("Cache_top.clock");
    auto rst = cfg.NewXData("Cache_top.reset");
    test_assert(clk->U() == *(uint8_t*)((uint64_t)cfg_base+8), "xsignal cfg fail: %d  <- %d", clk->AsInt32(), *(uint8_t*)((uint64_t)cfg_base+8));
    test_assert(rst->U() == *(uint8_t*)((uint64_t)cfg_base+46), "xsignal cfg fail: %d  <- %d", rst->AsInt32(), *(uint8_t*)((uint64_t)cfg_base+46));

    auto clk_old = clk1.clk;
    clk1.Disable();
    clk1.Step(10);
    test_assert(clk1.clk == clk_old, "clk1 disable fail: %lld", clk1.clk);
    clk1.Enable();
    clk1.Step(10);
    test_assert(clk1.clk == clk_old+10, "clk1 enable fail: %lld", clk1.clk);

    // XConditionCheck
    XConditionCheck check(&clk1);
    clk1.StepRis(check.GetCb(), check.CSelf(),"XConditionCheck");
    check.SetCondition("key_EQ", &x64, &x64, CompareOp::EQ);
    check.SetCondition("key_GT", &x64, &d1, CompareOp::GT);
    int px1[1] = {2};
    int px2[1] = {2};
    check.SetCondition("key_EQ_1", (uint64_t)px1, (uint64_t)px2, CompareOp::EQ, 4);
    check.SetCondition("key_GT_2", (uint64_t)px1, (uint64_t)px2, CompareOp::GT, 4);
    check.SetValidCmpMode("key_EQ_1", CompareOp::NE);

    clk_old = clk1.clk;
    clk1.Step(10);
    test_assert(check.GetTriggeredConditionKeys().size() == 3, "check fail: %ld", check.GetTriggeredConditionKeys().size());
    test_assert(clk1.clk == clk_old + 1, "clk1 disable fail: %lld", clk1.clk);

    test_assert(1 == clk1.RemoveStepFalCbByDesc("Fal-lambda"), "check remove step cb fail");
    px2[0] = 6;
    check.ClearCondition();
    XRangeCheck rcheck(6, 4);
    check.SetCondition("key_RG", (uint64_t)px1, (uint64_t)px2, CompareOp::GT, 4,
                       0, 0, 0, rcheck.GetArrayCmp(), rcheck.CSelf());
    clk1.Enable();
    clk1.Step(10);
    test_assert(clk1.IsDisable(), "XRangeCheck check fail");
    Info("test fails: %d, success: %d\n", fails, success);
    return fails;
}
} // namespace xspcomm
