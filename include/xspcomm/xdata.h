#ifndef __xspcomm_xdata__
#define __xspcomm_xdata__

#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include "xspcomm/xcallback.h"

namespace xspcomm {

typedef unsigned char xsvLogic; /* scalar */

typedef struct {
    uint32_t aval;
    uint32_t bval;
} xsvLogicVecVal;

void TEST_DPI_LR(void *v);
void TEST_DPI_LW(unsigned char v);
void TEST_DPI_VR(void *v);
void TEST_DPI_VW(void *v);

enum class IOType {
    Input,
    Output,
    InOut,
};

enum class WriteMode {
    Imme,
    Rise,
    Fall,
};

#define bit32_one(tar, msk) tar = msk | tar
#define bit32_zro(tar, msk) tar = (~msk) & tar
#define bit32_val(tar, msk) (msk & tar) == 0 ? 0 : 1
#define bit32_msk(ones) (1 << ones) - 1
#define bit32_set(tar, idx) tar = (tar) | (1 << idx)
#define bit32_hex(tar, idx, val)                                               \
    tar = ((~(0xF << idx * 4)) & (tar)) | (val << idx * 4)
#define bit32_chr(tar, idx, val)                                               \
    tar = ((~(0xFF << idx * 8)) & (tar)) | (val << idx * 8)

class PinBind
{
private:
    int index            = -1;
    uint32_t mask        = 0;
    xsvLogicVecVal *pVec = nullptr;
    xsvLogic *pLgc       = nullptr;

public:
    std::function<void()> write_fc = nullptr;

    PinBind(xsvLogicVecVal *p, int index);
    PinBind(xsvLogic *p);
    PinBind &operator=(const u_int8_t &v);
    PinBind &operator=(const std::string &v);
    PinBind &operator=(std::string &v);
    operator u_int8_t();
    /*************************************************************** */
    //                  Start of Stable public user APIs
    /*************************************************************** */
    PinBind &Set(int v);
    PinBind &Set(std::string &v){ return this->operator=(v); }
    int Get() {return this->AsInt32();}
    int AsInt32();
    std::string AsString();
    /*************************************************************** */
    //                  End of Stable public user APIs
    /*************************************************************** */

    // template functions
    PinBind &operator=(const char *v)
    {
        return this->operator=(std::string(v));
    }
    template <typename T>
    bool operator==(const T data)
    {
        return (T) * this == data;
    }
    template <typename T>
    PinBind &operator=(const T &v)
    {
        return this->operator=((u_int8_t)v);
    }
    template <typename T>
    operator T()
    {
        return (T)this->operator u_int8_t();
    }
};

class XData;
struct XDataCallBack {
    std::string desc;
    xfunction<void, bool, XData *, u_int32_t, void *> fc = nullptr;
    void *args                                           = nullptr;
};
class XData
{
public:
    static const IOType In      = IOType::Input;
    static const IOType Out     = IOType::Output;
    static const IOType InOut   = IOType::InOut;
    static const WriteMode Imme = WriteMode::Imme;
    static const WriteMode Rise = WriteMode::Rise;
    static const WriteMode Fall = WriteMode::Fall;
    XData & value;
private:
    std::vector<XDataCallBack> call_back_on_change;
    xfunction<void, xsvLogic *> bitRead        = nullptr;
    xfunction<void, xsvLogic> bitWrite         = nullptr;
    xfunction<void, xsvLogicVecVal *> vecRead  = nullptr;
    xfunction<void, xsvLogicVecVal *> vecWrite = nullptr;
    PinBind pinbind_bit;
    PinBind **pinbind_vec = nullptr;
    uint32_t ubuff[2];
    uint32_t xbuff[2];
    volatile uint64_t xdata    = 0;
    volatile uint64_t udata    = 0;
    uint32_t vecSize           = 0;
    uint32_t zero_mask         = -1;
    xsvLogicVecVal *__pVecData = nullptr; // 01ZX  -> 00,01,10,11
    xsvLogic __mLogicData      = 0;       // 01ZX  -> 00,01,10,11
    bool igore_callback        = false;
    bool udata_is_valid        = false;
    WriteMode write_mode       = WriteMode::Rise;

    xsvLogicVecVal *last_pVecData = nullptr;
    xsvLogic last_mLogicData      = 0;
    bool last_is_write            = false;
    bool ignore_same_write        = true;
    u_int32_t sub_offset          = 0;       // for sub data
    xsvLogicVecVal * sub_pVecRef  = nullptr; // for sub data

private:
    void update_read();
    void update_write();
    void _sv_to_local();
    void _zero_sv();
    void _trunc_sv();
    void _local_to_sv();
    void _dpi_read();
    void _dpi_write();
    void _update_shadow();
    void _dpi_check();
    bool _need_write();
    void _update_last_write();
    void _sub_data_fake_dpirw(void *data, bool is_read);
    void _sub_data_fake_dpir(void * data);
    void _sub_data_fake_dpiw(void * data);

public:
    // basic
    std::string mName;
    IOType mIOType;
    uint32_t mWidth;
    // DPI                                         ba
    xsvLogicVecVal *pVecData = nullptr; // 01ZX  -> 00,01,10,11
    xsvLogic mLogicData      = 0;       // 01ZX  -> 00,01,10,11
public:
    /*************************************************************** */
    //                  Start of Stable public user APIs
    /*************************************************************** */
    XData();
    XData(uint32_t width, IOType itype, std::string name = "");
    XData(XData &t);
    ~XData();
    void ReInit(uint32_t width, IOType itype, std::string name = "");
    XData *SubDataRef(std::string name, uint32_t start, uint32_t width);
    WriteMode GetWriteMode();
    bool SetWriteMode(WriteMode mode);
    bool DataValid();
    void BindDPIRW(xfunction<void, void *> read, xfunction<void, void *> write);
    void BindDPIRW(xfunction<void, void *> read,
                   xfunction<void, unsigned char> write);
    void BindDPIRW(void (*read)(void *), void (*write)(const void *));
    void BindDPIRW(void (*read)(void *), void (*write)(const unsigned char));
    void SetVU8(std::vector<unsigned char> &buffer);
    std::vector<unsigned char> GetVU8();
    uint32_t W();
    uint64_t U();
    int64_t S();
    bool B();
    std::string String();
    bool Connect(XData &xdata);
    bool Equal(XData &xdata) { return this->operator==(xdata); }
    XData &Set(XData &data);
    XData &Set(const char *data);
    XData &Set(std::string &data);
    XData &Set(int data);
    XData &Set(long data);
    XData &Set(long long data);
    XData &Set(unsigned long long data);
    bool IsInIO(){ return this->mIOType == IOType::Input; }
    bool IsOutIO(){ return this->mIOType == IOType::Output; }
    bool IsBiIO(){ return this->mIOType == IOType::InOut; }
    bool IsImmWrite(){ return this->write_mode == WriteMode::Imme && this->mIOType != IOType::Output; }
    bool IsRiseWrite(){ return this->write_mode == WriteMode::Rise && this->mIOType != IOType::Output; }
    bool IsFallWrite(){ return this->write_mode == WriteMode::Fall && this->mIOType != IOType::Output; }
    XData &AsImmWrite(){ this->SetWriteMode(WriteMode::Imme); return *this; }
    XData &AsRiseWrite(){ this->SetWriteMode(WriteMode::Rise); return *this; }
    XData &AsFallWrite(){ this->SetWriteMode(WriteMode::Fall); return *this; }
    XData &AsBiIO();
    XData &AsInIO();
    XData &AsOutIO();
    XData &Flip();
    XData &Invert();
    PinBind &At(int index);
    std::string AsBinaryString();
    long long AsInt64();
    int AsInt32();
    /*************************************************************** */
    //                  End of Stable public user APIs
    /*************************************************************** */

    // C++ dependent APIs
    void WriteOnRise();
    void WriteOnFall();
    void WriteDirect();
    void SetIgnoreSameDataWrite(bool w) { this->ignore_same_write = w; }
    void _TestBindDPIL();
    void _TestBindDPIV();
    void SetBits(u_int8_t *buffer, int count, u_int8_t *mask = nullptr,
                 int start = 0);
    void SetBits(u_int32_t *buffer, u_int32_t count, u_int32_t *mask = nullptr,
                 u_int32_t start = 0);
    bool GetBits(u_int32_t *buffer, u_int32_t count);
    bool GetBits(u_int8_t *buffer, u_int32_t count);
    void OnChange(xfunction<void, bool, XData *, u_int64_t, void *> func,
                  void *args = nullptr, std::string desc = "");
    void ReadFresh(WriteMode m);
    PinBind &operator[](u_int32_t index);
    bool operator==(XData &data);
    bool operator==(u_int64_t data);
    bool operator==(std::string &str);
    bool operator==(const std::string &str);
    bool operator==(const char *str);
    bool operator==(char *str);
    XData &operator=(XData &data);
    XData &operator=(const char *str);
    XData &operator=(std::string &data);
    XData &operator=(u_int64_t data);
    operator std::string();
    operator u_int64_t();
    // template functions
    template <typename T>
    XData(T value) : XData(sizeof(T) * 8, IOType::Input)
    {
        this->operator=(value);
    }
    template <typename T>
    bool operator==(T data)
    {
        return this->operator==((u_int64_t)data);
    }
    template <typename T>
    XData &operator=(T data)
    {
        return this->operator=((u_int64_t)data);
    }
    template <typename T>
    operator T()
    {
        return (T)(this->operator u_int64_t());
    }
};
} // namespace xspcomm
#endif
