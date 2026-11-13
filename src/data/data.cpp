
#include "xspcomm/xdata.h"
#include "xspcomm/common/compare.h"
#include "xspcomm/xutil.h"
#include "data/array_storage.h"
#include "data/vpi_storage.h"
#include <utility>
#include <stdexcept>

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
    if (this->vpi_owner) this->vpi_owner->context->Check();
    if (this->sub_parent) {
        this->_slice_rw(true);
        return;
    }
    if (unlikely(this->mIOType == IOType::Input
                 && this->write_mode != WriteMode::Imme)) {
        return;
    }
    this->_dpi_check();
    if (this->backend_kind == XDataBackendKind::VPI) { this->_vpi_read(); return; }
    if (this->array_storage) {
        this->array_storage->Read(this->array_location, this->mWidth == 0 ?
            static_cast<void *>(&this->mLogicData) : static_cast<void *>(this->pVecData));
        return;
    }
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
    if (this->sub_parent) {
        this->_slice_rw(false);
        return;
    }
    if (this->backend_kind == XDataBackendKind::VPI) { this->_vpi_write(); return; }
    if (this->array_storage) {
        this->array_storage->Write(this->array_location, this->mWidth == 0 ?
            static_cast<const void *>(&this->mLogicData) : static_cast<const void *>(this->pVecData));
        return;
    }
    if (likely(this->bitWrite)) this->bitWrite(this->mLogicData);
    if (likely(this->vecWrite)) this->vecWrite(this->pVecData);
}

void XData::WriteOnRise()
{
    if (unlikely(this->mIOType == IOType::Output)) return;
    if (unlikely(this->write_mode != WriteMode::Rise)) return;
    if (this->_need_write()) {
        this->WriteDirect();
    }
}

void XData::WriteOnFall()
{
    if (unlikely(this->mIOType == IOType::Output)) return;
    if (unlikely(this->write_mode != WriteMode::Fall)) return;
    if (this->_need_write()) {
        this->WriteDirect();
    }
}
void XData::_dpi_write()
{
    if (unlikely(this->mIOType == IOType::Output)) return;
    if (unlikely(this->write_mode != WriteMode::Imme)) return;
    this->_dpi_check();
    // The parent's refreshed full value, not the slice's last written value,
    // determines whether an immediate backend write can be suppressed.
    if (this->sub_parent) {
        this->WriteDirect();
        return;
    }
    if (this->_need_write()) {
        this->WriteDirect();
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
    if (this->vpi_owner) this->vpi_owner->context->Check();
    if (this->sub_parent) {
        if (this->sub_lifetime.expired()) throw std::runtime_error("Parent XData has been destroyed");
        return this->sub_parent->_check_writeable();
    }
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
    if (this->mWidth != width) this->view_lifetime.Renew();
    this->sub_parent = nullptr;
    this->sub_lifetime.reset();
    this->vpi_owner.reset();
    this->vpi_obj_handle = nullptr;
    this->array_storage.reset(); // Array element widths/layouts are immutable.
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
    this->sub_parent = t.sub_parent;
    this->sub_lifetime = t.sub_lifetime;
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
    if(this->sub_parent != nullptr){
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
    const uint64_t bits = width ? width : 1;
    if (uint64_t(start) + bits > (this->mWidth ? this->mWidth : 1))
        throw std::out_of_range("SubDataRef bit range exceeds parent XData");
    auto *parent = this;
    if (this->sub_parent) {
        if (this->sub_lifetime.expired()) throw std::runtime_error("Parent XData has been destroyed");
        start += this->sub_offset;
        parent = this->sub_parent;
    }
    auto sub = std::make_unique<XData>(width, this->mIOType, name);
    if(sub->mIOType != IOType::Output)sub->SetWriteMode(WriteMode::Imme);
    sub->sub_offset = start;
    sub->sub_parent = parent;
    sub->sub_lifetime = parent->ObserveLifetime();
    sub->backend_kind = parent->backend_kind;
    sub->readonly_backend = parent->readonly_backend;
    sub->update_read();
    return sub.release();
}

std::shared_ptr<XData> XData::SubDataRef(uint32_t start, uint32_t width, std::string name){
    std::shared_ptr<XData> sub(this->SubDataRefRaw(start, width, name));
    return sub;
}

void XData::_slice_rw(bool is_read){
    if (this->sub_lifetime.expired()) throw std::runtime_error("Parent XData has been destroyed");
    auto &parent = *this->sub_parent;
    const uint64_t bits = this->mWidth ? this->mWidth : 1;
    if (uint64_t(this->sub_offset) + bits > (parent.mWidth ? parent.mWidth : 1))
        throw std::out_of_range("Parent XData width changed beneath SubDataRef");
    // Refresh before read-modify-write so an immediate write cannot overwrite
    // sibling fields changed by the simulator. Scheduled input values remain
    // local because the parent's existing read policy skips backend reads.
    if (is_read) parent.update_read();
    else {
        // Notify after the merged write, as a direct parent assignment does.
        // A pre-write callback that reads this slice would replace its pending
        // value with the old parent value before the merge.
        parent._dpi_read();
        parent._sv_to_local();
    }
    if (this->sub_lifetime.expired()) throw std::runtime_error("Parent XData storage is no longer valid");
    bool changed = false;
    for (uint64_t bit = 0; bit < bits;) {
        const auto source_bit = uint64_t(this->sub_offset) + bit;
        const uint32_t parent_shift = source_bit % 32, child_shift = bit % 32;
        const uint32_t count = uint32_t(std::min<uint64_t>(bits - bit, std::min(32 - parent_shift, 32 - child_shift)));
        const uint32_t mask = count == 32 ? UINT32_MAX : (uint32_t(1) << count) - 1;
        xsvLogicVecVal parent_scalar{uint32_t(parent.mLogicData & 1), uint32_t((parent.mLogicData >> 1) & 1)};
        xsvLogicVecVal child_scalar{uint32_t(this->mLogicData & 1), uint32_t((this->mLogicData >> 1) & 1)};
        auto &parent_word = parent.mWidth ? parent.pVecData[source_bit / 32] : parent_scalar;
        auto &child_word = this->mWidth ? this->pVecData[bit / 32] : child_scalar;
        auto &source = is_read ? parent_word : child_word;
        auto &target = is_read ? child_word : parent_word;
        const auto from = is_read ? parent_shift : child_shift;
        const auto to = is_read ? child_shift : parent_shift;
        const auto aval = ((source.aval >> from) & mask) << to;
        const auto bval = ((source.bval >> from) & mask) << to;
        changed |= ((target.aval ^ aval) & (mask << to)) || ((target.bval ^ bval) & (mask << to));
        target.aval = (target.aval & ~(mask << to)) | aval;
        target.bval = (target.bval & ~(mask << to)) | bval;
        if (!this->mWidth && is_read) this->mLogicData = xsvLogic(target.aval | (target.bval << 1));
        if (!parent.mWidth && !is_read) parent.mLogicData = xsvLogic(target.aval | (target.bval << 1));
        bit += count;
    }
    if (!is_read) {
        parent._sv_to_local();
        // The simulator may have changed the field since the last immediate
        // write, even when the requested new value equals that earlier write.
        if (changed && parent.write_mode == WriteMode::Imme) parent.last_is_write = false;
        parent._dpi_write();
        parent._update_shadow();
    }
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
