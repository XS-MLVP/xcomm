#include "xspcomm/xcomm.h"
#include "xspcomm/xinstance.h"

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

    // test callback
    int count;
    z.OnChange(
        [](bool v, XData *x, u_int32_t val, void *args) {
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
    test_assert(count = 5 && z.DataValid(), "test call back fail");

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
    b.OnChange([](bool v, XData *x, u_int32_t val, void *args) {});
    a.Connect(b);
    a = 1;
    b = 1;
    a = 2;
    test_assert(b == 2 && b.DataValid(),
                "test connect fail, b.mIOType=%s, a.mIOType=%s",
                IOTYPES(b.mIOType), IOTYPES(a.mIOType));

    a.Invert();
    test_assert(~uint32_t(2) == a, "~ check fail: %s", a.String().c_str());

    Info("test fails: %d, success: %d\n", fails, success);
    return fails;
}
} // namespace xspcomm
