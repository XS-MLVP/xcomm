
#include "xspcomm/xdata.h"
#include "xspcomm/common/compare.h"
#include "xspcomm/xutil.h"
#include <utility>

namespace xspcomm {



const IOType XData::In;
const IOType XData::Out;
const IOType XData::InOut;
const WriteMode XData::Imme;
const WriteMode XData::Rise;
const WriteMode XData::Fall;

void TEST_DPI_LR(void *v)
{
    Debug("test DPI Logic Read");
}
void TEST_DPI_LW(unsigned char v)
{
    Debug("test DPI Logic Write");
}
void TEST_DPI_VR(void *v)
{
    Debug("test DPI Vec Read");
}
void TEST_DPI_VW(void *v)
{
    Debug("test DPI Vec Write");
}

// class XData::
void XData::update_read()
{
    this->_dpi_read();
    this->_sv_to_local();
    this->_update_shadow();
}
void XData::update_write()
{
    this->_local_to_sv();
    this->_dpi_write();
    this->_update_shadow();
}
bool XData::DataValid()
{
    return this->udata_is_valid;
}
void XData::_sv_to_local()
{
    this->_trunc_sv();
    if (this->mWidth == 0) {
        Assert(this->mLogicData >= 0 && this->mLogicData < 4,
               "Logic data[%d] need in range [0,1,2,3]", this->mLogicData);
        this->udata          = this->mLogicData;
        this->xdata          = this->udata > 1 ? 1 : 0;
        this->udata_is_valid = this->udata > 1 ? false : true;
    } else {
        // reset buff
        this->ubuff[0] = 0;
        this->ubuff[1] = 0;
        this->xbuff[0] = 0;
        this->xbuff[1] = 0;
        auto range     = std::min(
            2, (int)this->vecSize); // max xdata.length is 64 (2 vecData)
        for (int i = 0; i < range; i++) {
            this->ubuff[i] = this->pVecData[i].aval;
            this->xbuff[i] = this->pVecData[i].bval;
        }
        this->udata = *(uint64_t *)this->ubuff;
        this->xdata = *(uint64_t *)this->xbuff;
        this->udata_is_valid = true;
        for (int i = 0; i < this->vecSize; i++) {
            if (this->pVecData[i].bval != 0) {
                this->udata_is_valid = false;
                break;
            }
        }
    }
}
void XData::_zero_sv()
{
    for (int i = 0; i < this->vecSize; i++) {
        this->pVecData[i].aval = 0;
        this->pVecData[i].bval = 0;
    }
}
void XData::_trunc_sv()
{
    if (this->mWidth == 0) return;
    this->pVecData[this->vecSize - 1].aval &= this->zero_mask;
    this->pVecData[this->vecSize - 1].bval &= this->zero_mask;
}
void XData::_local_to_sv()
{
    if (this->mWidth == 0) {
        Assert(this->udata >= 0 && this->udata < 4,
               "Logic data to write need in range [0,1,2,3]");
        this->mLogicData = this->udata;
    } else {
        // reset buff
        *(uint64_t *)this->ubuff = this->udata;
        auto range               = std::min(
            2, (int)this->vecSize); // max xdata.length is 64 (2 vecData)
        for (int i = 0; i < this->vecSize; i++) {
            // assign lower 64 bits
            if (i < 2) {
                this->pVecData[i].aval = this->ubuff[i];
                // assign higher bits zero
            } else {
                this->pVecData[i].aval = 0;
            }
            this->pVecData[i].bval = 0;
        }
    }
    this->_sv_to_local();
}
void XData::_dpi_read()
{
    if (unlikely(this->mIOType == IOType::Input
                 && this->write_mode != WriteMode::Imme)) {
        return;
    }
    this->_dpi_check();
    if (likely(this->bitRead)) this->bitRead(&this->mLogicData);
    if (likely(this->vecRead)) this->vecRead(this->pVecData);
}
WriteMode XData::GetWriteMode()
{
    return this->write_mode;
}
bool XData::SetWriteMode(WriteMode mode)
{
    if (unlikely(!this->_check_writeable())) {
        Assert(false, "XData(%s) is not writeable, cannot set write mode",
               this->mName.c_str());
    }
    this->write_mode = mode;
    return true;
};

void XData::WriteDirect(){
    if (likely(this->bitWrite)) this->bitWrite(this->mLogicData);
    if (likely(this->vecWrite)) this->vecWrite(this->pVecData);
}

void XData::WriteOnRise()
{
    if (unlikely(this->mIOType == IOType::Output)) return;
    if (unlikely(this->write_mode != WriteMode::Rise)) return;
    if (this->_need_write()) {
        if (likely(this->bitWrite)) this->bitWrite(this->mLogicData);
        if (likely(this->vecWrite)) this->vecWrite(this->pVecData);
    }
}

void XData::WriteOnFall()
{
    if (unlikely(this->mIOType == IOType::Output)) return;
    if (unlikely(this->write_mode != WriteMode::Fall)) return;
    if (this->_need_write()) {
        if (likely(this->bitWrite)) this->bitWrite(this->mLogicData);
        if (likely(this->vecWrite)) this->vecWrite(this->pVecData);
    }
}
void XData::_dpi_write()
{
    if (unlikely(this->mIOType == IOType::Output)) return;
    if (unlikely(this->write_mode != WriteMode::Imme)) return;
    this->_dpi_check();
    if (this->_need_write()) {
        if (likely(this->bitWrite)) this->bitWrite(this->mLogicData);
        if (likely(this->vecWrite)) this->vecWrite(this->pVecData);
    }
}

bool XData::_need_write()
{
    if (unlikely(!this->ignore_same_write)) return true;
    if (unlikely(!this->last_is_write)) {
        this->_update_last_write();
        return true;
    };
    if (this->mWidth == 0) {
        if (this->last_mLogicData != this->mLogicData) {
            this->_update_last_write();
            return true;
        }
        return false;
    } else {
        if (memcmp(this->last_pVecData, this->pVecData,
                   this->vecSize * sizeof(xsvLogicVecVal)) != 0) {
            this->_update_last_write();
            return true;
        }
    }
    return false;
}

void XData::_update_last_write()
{
    this->last_is_write   = true;
    this->last_mLogicData = this->mLogicData;
    if (this->mWidth > 0) {
        if (this->last_pVecData == nullptr) {
            this->last_pVecData =
                (xsvLogicVecVal *)calloc(this->vecSize, sizeof(xsvLogicVecVal));
        }
        memcpy(this->last_pVecData, this->pVecData,
               this->vecSize * sizeof(xsvLogicVecVal));
    }
}

void XData::_update_shadow()
{
    if (unlikely(this->igore_callback)) return;
    if (unlikely(!this->has_on_change_cbs)) return;
    bool need_call = false;
    bool validate  = true;
    if (this->mWidth == 0) {
        need_call = (this->__mLogicData != this->mLogicData);
        validate  = this->mLogicData < 2;
    } else {
        for (int i = 0; i < this->vecSize; i++) {
            if (this->__pVecData[i].aval != this->pVecData[i].aval) {
                need_call = true;
            }
            if (this->__pVecData[i].bval != this->pVecData[i].bval) {
                need_call = true;
            }
            if (unlikely(this->pVecData[i].bval)) { validate = false; }
            this->__pVecData[i] = this->pVecData[i];
        }
    }
    this->__mLogicData   = this->mLogicData;
    this->igore_callback = true;
    if (need_call) {
        for (auto fc : this->call_back_on_change) {
            fc.fc(validate, this, this->udata, fc.args);
        }
    }
    this->igore_callback = false;
}
void XData::_dpi_check(){
    // TBD
};

bool XData::_check_writeable() const
{
    return !this->readonly_backend && this->mIOType != IOType::Output;
}

XData::XData() : XData(0, IOType::InOut){};
XData::XData(uint32_t width, IOType itype, std::string name) :
    mWidth(width), mIOType(itype), pinbind_bit(&this->mLogicData), mName(name), value(*this)
{
    this->ReInit(width, itype, name);
}
void XData::ReInit(uint32_t width, IOType itype, std::string name)
{
    if (this->pVecData) {
        for (int i = 0; i < this->mWidth; i++) { delete this->pinbind_vec[i]; }
        free(this->pinbind_vec);
        free(this->pVecData);
        free(this->__pVecData);
        this->pinbind_vec = nullptr;
        this->pVecData = nullptr;
        this->__pVecData = nullptr;
    }
    this->mWidth  = width;
    this->mIOType = itype;
    this->mName   = name;
    this->backend_kind = XDataBackendKind::Unknown;
    this->readonly_backend = false;
    this->last_is_write = false;
    this->last_mLogicData = 0;
    if (this->last_pVecData) {
        free(this->last_pVecData);
        this->last_pVecData = nullptr;
    }

    auto write_fc = [this]() {
        this->_dpi_write();
        this->_sv_to_local();
        this->_update_shadow();
    };
    if (width >= 1) {
        // svDPI Vec is 32 bits
        this->vecSize = width / 32 + ((width % 32 != 0) ? 1 : 0);
        this->pVecData =
            (xsvLogicVecVal *)calloc(this->vecSize, sizeof(xsvLogicVecVal));
        this->__pVecData =// [shadow data][3 * tmp data]
            (xsvLogicVecVal *)calloc(this->vecSize*4, sizeof(xsvLogicVecVal));
        this->pinbind_vec = (PinBind **)calloc(this->mWidth, sizeof(PinBind *));
        for (int i = 0; i < this->mWidth; i++) {
            this->pinbind_vec[i]           = new PinBind(this->pVecData, i);
            this->pinbind_vec[i]->write_fc = write_fc;
        }
        this->zero_mask =
            bit32_msk(width % 32); // eg: [bin] ,|1111,1000,0000,0000| ...
        if (this->zero_mask == 0) {
            this->zero_mask = -1; // set: 0xFFFFFFFF
        }
    } else {
        this->pinbind_bit.write_fc = write_fc;
        this->vecSize              = 0;
    }
    this->mLogicData     = 0;
    this->udata_is_valid = true;
}
XData::XData(XData &t) :
    mWidth(t.mWidth),
    mIOType(t.mIOType),
    pinbind_bit(&this->mLogicData),
    mLogicData(t.mLogicData),
    value(*this)
{
    auto write_fc = [this]() {
        this->_dpi_write();
        this->_sv_to_local();
        this->_update_shadow();
    };
    t.update_read();
    // members
    this->xdata     = t.xdata;
    this->udata     = t.udata;
    this->vecSize   = t.vecSize;
    this->zero_mask = t.zero_mask;
    this->write_mode = t.write_mode;
    this->sub_offset = t.sub_offset;
    this->sub_pVecRef = t.sub_pVecRef;
    this->sub_parent = t.sub_parent;
    // pointers
    if (this->vecSize > 0) {
        this->pVecData =
            (xsvLogicVecVal *)calloc(this->vecSize, sizeof(xsvLogicVecVal));
        this->__pVecData =    // [shadow data][3 * tmp data]
            (xsvLogicVecVal *)calloc(this->vecSize*4, sizeof(xsvLogicVecVal));
        this->pinbind_vec = (PinBind **)calloc(this->mWidth, sizeof(PinBind *));
        for (int i = 0; i < this->mWidth; i++) {
            this->pinbind_vec[i]           = new PinBind(this->pVecData, i);
            this->pinbind_vec[i]->write_fc = write_fc;
        }
    }
    for (int i = 0; i < this->vecSize; i++) {
        this->pVecData[i] = t.pVecData[i];
    }
    this->pinbind_bit.write_fc = write_fc;
    this->_sv_to_local();
    if(this->sub_pVecRef != nullptr){
        Warn("You are copying a ref xdata. bind _sub_data_fake_dpirw ...");
        this->vecRead = [this](void*data){return this->_sub_data_fake_dpir(data);};
        if(this->mIOType != IOType::Output)this->vecWrite = [this](void*data){return this->_sub_data_fake_dpiw(data);};
        this->update_read();
    }
};
XData::~XData()
{
    if (this->pVecData) {
        for (int i = 0; i < this->mWidth; i++) { delete this->pinbind_vec[i]; }
        free(this->pinbind_vec);
        free(this->pVecData);
        free(this->__pVecData);
        this->pVecData = nullptr;
    }
    if (this->last_pVecData) {
        free(this->last_pVecData);
        this->last_pVecData = nullptr;
    }
}

XData* XData::SubDataRefRaw(uint32_t start, uint32_t width, std::string name){
    Assert(this->mWidth > 0, "Only svVec support SubDataRef, need mWidth > 0");
    Assert(start + width <= this->mWidth, "SubDataRef out of range (start + witdth=%d > mWidth=%d)", start + width, this->mWidth);
    auto sub = new XData(width, this->mIOType, name);
    if(sub->mIOType != IOType::Output)sub->SetWriteMode(WriteMode::Imme);
    sub->sub_offset = start;
    sub->sub_pVecRef = this->pVecData;
    sub->sub_parent = this;
    sub->vecRead = [sub](void*data){return sub->_sub_data_fake_dpir(data);};
    if(sub->mIOType != IOType::Output)sub->vecWrite = [sub](void*data){return sub->_sub_data_fake_dpiw(data);};
    sub->update_read();
    return sub;
}

std::shared_ptr<XData> XData::SubDataRef(uint32_t start, uint32_t width, std::string name){
    std::shared_ptr<XData> sub(this->SubDataRefRaw(start, width, name));
    return sub;
}

void XData::_sub_data_fake_dpirw(void *data, bool is_read){
    DebugC(false, "_sub_data_fake_dpirw: %s", is_read ? "Read": "Write");
    if (is_read && this->sub_parent != nullptr) {
        this->sub_parent->update_read();
    }
    // read data from ref
    uint32_t start_p = this->sub_offset / 32;
    uint32_t start_f = this->sub_offset % 32;
    uint32_t end_p = (this->sub_offset + this->mWidth - 1) / 32;
    uint32_t end_f = (this->sub_offset + this->mWidth - 1) % 32 + 1;
    if (this->vecSize == 0 && this->mWidth == 0) {
        if (is_read) {
            auto aval = (this->sub_pVecRef[start_p].aval >> start_f) & 1;
            auto bval = (this->sub_pVecRef[start_p].bval >> start_f) & 1;
            this->mLogicData = bval * 2 + aval;
        } else {
            uint32_t mask = 1 << start_f;
            uint32_t maskb = ~mask;
            this->sub_pVecRef[start_p].aval = (maskb & this->sub_pVecRef[start_p].aval) | ((this->mLogicData & 1) << start_f);
            this->sub_pVecRef[start_p].bval = (maskb & this->sub_pVecRef[start_p].bval) | (((this->mLogicData >> 1) & 1) << start_f);
            if (this->sub_parent != nullptr) {
                this->sub_parent->_sv_to_local();
                this->sub_parent->_dpi_write();
                this->sub_parent->_update_shadow();
            }
        }
        return;
    }
    xsvLogicVecVal * p = this->sub_pVecRef;
    // copy/write data
    // from/dist:      |_|_|_|_|_|_|
    //                 /     /
    // to/from:        |_|_|_|
    // [shadow data][mask][aval][bval]
    int secs = end_p - start_p + 1;
    int * temp_mask = (int *)this->__pVecData + this->vecSize * 2;
    int * temp_aval = temp_mask + secs;
    int * temp_bval = temp_aval + secs;
    Assert(secs > 0, "Error! SubDataRef: secs <= 0");
    Assert(secs <= (this->vecSize + 1), "Error! SubDataRef: secs(%d) > vecSize(%d) + 1", secs, this->vecSize);
    if(is_read) {
        // Logic for reading data
        for (int i = 0; i < secs; i++) {
            temp_aval[i] = p[start_p + i].aval;
            temp_bval[i] = p[start_p + i].bval;
        }
        if (start_f > 0) {
            big_shift(temp_aval, secs, start_f);
            big_shift(temp_bval, secs, start_f);
        }
        for (int i = 0; i < this->vecSize; i++){
            this->pVecData[i].aval = temp_aval[i];
            this->pVecData[i].bval = temp_bval[i];
        }
    } else {
        // Logic for writing data
        for (int i = 0; i < this->vecSize; i++) {
            temp_aval[i] = this->pVecData[i].aval;
            temp_bval[i] = this->pVecData[i].bval;
        }
        if (start_f > 0) {
            big_shift(temp_aval, secs, -start_f);
            big_shift(temp_bval, secs, -start_f);
        }
        // Calculate bit mask
        // Use a more precise method to calculate the mask
        memset(temp_mask, 0, secs * sizeof(int));
        // Create mask for the first 32-bit block
        uint32_t first_mask_bits = 32 - start_f;
        if (this->mWidth < first_mask_bits) {
            first_mask_bits = this->mWidth;
        }
        uint32_t first_mask = first_mask_bits == 32
                                  ? 0xFFFFFFFFu
                                  : (uint32_t(1) << first_mask_bits) - 1;
        temp_mask[0] = first_mask << start_f;
        // Create mask for the middle blocks (if any)
        for (int i = 1; i < secs - 1; i++) {
            temp_mask[i] = 0xFFFFFFFF;  // Set all bits to 1
        }
        // Create mask for the last block (if different from the first block)
        if (secs > 1) {
            uint32_t last_bits = (this->sub_offset + this->mWidth) % 32;
            if (last_bits == 0) {
                // If exactly on the boundary, set all bits
                temp_mask[secs - 1] = 0xFFFFFFFF;
            } else {
                // Otherwise, set only the required bits
                temp_mask[secs - 1] = bit32_msk(last_bits);
            }
        }
        // Apply the mask and update the data
        for (int i = 0; i < secs; i++) {
            p[start_p + i].aval = (p[start_p + i].aval & ~temp_mask[i]) | (temp_aval[i] & temp_mask[i]);
            p[start_p + i].bval = (p[start_p + i].bval & ~temp_mask[i]) | (temp_bval[i] & temp_mask[i]);
        }
        if (this->sub_parent != nullptr) {
            this->sub_parent->_sv_to_local();
            this->sub_parent->_dpi_write();
            this->sub_parent->_update_shadow();
        }
    }
    // Update local data
    this->_sv_to_local();
    this->_update_shadow();
}

void XData::_sub_data_fake_dpir(void *data){
    return this->_sub_data_fake_dpirw(data, true);
}

void XData::_sub_data_fake_dpiw(void *data){
    return this->_sub_data_fake_dpirw(data, false);
}


XData &XData::Set(XData &data)
{
    return this->operator=(data);
}

XData &XData::Set(const char *data)
{
    return this->operator=(data);
}

XData &XData::Set(std::string &data)
{
    return this->operator=(data);
}

XData &XData::Set(int data)
{
    return this->operator=(data);
}

XData &XData::Set(unsigned int data)
{
    return this->operator=(data);
}

XData &XData::Set(int64_t data)
{
    return this->operator=(data);
}

XData &XData::Set(uint64_t data)
{
    return this->operator=(data);
}

namespace {
template <typename T>
XData &imm_set_once(XData &self, T &&data)
{
    auto mode = self.GetWriteMode();
    self.AsImmWrite();
    self.Set(std::forward<T>(data));
    self.SetWriteMode(mode);
    return self;
}
} // namespace

XData &XData::ImmSet(XData &data)
{
    return imm_set_once(*this, data);
}

XData &XData::ImmSet(const char *data)
{
    return imm_set_once(*this, data);
}

XData &XData::ImmSet(std::string &data)
{
    return imm_set_once(*this, data);
}

XData &XData::ImmSet(int data)
{
    return imm_set_once(*this, data);
}

XData &XData::ImmSet(unsigned int data)
{
    return imm_set_once(*this, data);
}

XData &XData::ImmSet(int64_t data)
{
    return imm_set_once(*this, data);
}

XData &XData::ImmSet(uint64_t data)
{
    return imm_set_once(*this, data);
}

PinBind &XData::At(int index)
{
    return this->operator[](index);
}

std::string XData::AsBinaryString()
{
    this->update_read();
    std::string ret;
    if (this->mWidth == 0) {
        switch (this->mLogicData) {
        case 0: return "0"; break;
        case 1: return "1"; break;
        case 2: return "z"; break;
        case 3: return "x"; break;
        default:;
        }
        Assert(false, "Logic Data value error(%d), need range (0 -> 3)",
               this->mLogicData);
    }
    for (int i = this->vecSize - 1; i >= 0; i--) {
        for (int j = 31; j >= 0; j--) {
            if(i*32 + j >= this->mWidth)continue;
            auto b = (this->pVecData[i].bval & (1 << j)) >> j;
            auto a = (this->pVecData[i].aval & (1 << j)) >> j;
            auto v = b * 2 + a;
            switch (v)
            {
            case 0: if(!ret.empty())ret += "0"; break;
            case 1: ret += "1"; break;
            case 2: ret += "z"; break;
            case 3: ret += "x"; break;
            default: Assert(false, "Logic Data value error(%d), need range (0 -> 3)", v);
            }
        }
    }
    if(ret.empty()){
        ret = "0";
    }
    return ret;
}

int64_t XData::AsInt64()
{
    return this->S();
}

int XData::AsInt32()
{
    return (int)this->S();
}

bool XData::Connect(XData &xdata)
{
    auto a_drive_b = [](XData &a, XData &b) -> bool {
        if (a.mIOType == IOType::Input) {
            Error("Master IO type cannot be IOType::Input");
            return false;
        }
        if (b.mIOType == IOType::Output) {
            Error("Slave IO type cannot be IOType::Output");
            return false;
        }
        std::string desc = "connet_" + a.mName + "_to_" + b.mName;
        a.OnChange(
            [](bool valid, XData *x, u_int32_t val, void *args) {
                if (valid) {
                    auto y        = ((XData *)args);
                    y->mLogicData = x->mLogicData;
                    for (int i = 0; i < x->vecSize; i++) {
                        y->pVecData[i] = x->pVecData[i];
                    }
                    y->_sv_to_local();
                    y->_dpi_write();
                    y->_update_shadow();
                }
            },
            &b, desc);
        return true;
    };
    // same type
    // if(this->mIOType == IOType::InOut || xdata.mIOType == IOType::InOut)
    // Warn("connect InOut PINs, it may cause error!");
    if (this->mIOType == xdata.mIOType) {
        if (this->mIOType == IOType::InOut) {
            xdata = *this;
            return a_drive_b(*this, xdata);
        }
        Error("Can not connect same IOType[%s] %s <-> %s",
              this->mIOType == IOType::Input ? "IOType::Input" :
                                               "IOType::Output",
              this->mName.c_str(), xdata.mName.c_str());
        return false;
    }
    // diff type
    switch (this->mIOType) {
    case IOType::Input:
        *this = xdata;
        return a_drive_b(xdata, *this);
        break;
    case IOType::Output: xdata = *this; return a_drive_b(*this, xdata);
    case IOType::InOut:
        if (xdata.mIOType == IOType::Input) {
            xdata = *this;
            return a_drive_b(*this, xdata);
        } else {
            *this = xdata;
            return a_drive_b(xdata, *this);
        };
        break;
    default:
        Assert(false, "Connect %s -> %s fail!", this->mName.c_str(),
               xdata.mName.c_str());
    }
}

} // namespace xspcomm
