#include "xspcomm/xdata.h"
#include "xspcomm/common/compare.h"

namespace xspcomm {

void XData::SetBits(u_int8_t *buffer, int count, u_int8_t *mask, int start)
{
    if (unlikely(!this->_check_writeable())) {
        Assert(false, "XData(%s) is not writeable, cannot set bits",
               this->mName.c_str());
    }
    Assert(this->mWidth > 0, "Only svVec support SetBits");
    int index = start;
    for (int i = 0; i < count; i++) {
        auto vec_idx = index / 4;
        auto vec_off = index % 4;
        if (vec_idx >= this->vecSize) break;
        uint8_t byte_val = buffer[i];
        if (mask) { // FIXME: need refine
            u_int8_t old =
                (u_int8_t)((this->pVecData[vec_idx].aval >> vec_off * 4)
                           & 0x000000FF);
            byte_val = (old & (~mask[i])) | (buffer[i] & mask[i]);
        }
        bit32_chr(this->pVecData[vec_idx].aval, vec_off, byte_val);
        bit32_chr(this->pVecData[vec_idx].bval, vec_off, 0);
        index += 1;
    }
    this->_dpi_write();
    this->_sv_to_local();
    this->_update_shadow();
}
void XData::SetBits(u_int32_t *buffer, u_int32_t count, u_int32_t *mask,
                    u_int32_t start)
{
    if (unlikely(!this->_check_writeable())) {
        Assert(false, "XData(%s) is not writeable, cannot set bits",
               this->mName.c_str());
    }
    Assert(this->mWidth > 0, "only svVec support SetBits");
    auto range = start < this->vecSize ? std::min(count, this->vecSize - start) : 0;
    for (int i = 0; i < range; i++) {
        if (mask == nullptr) {
            this->pVecData[i + start].aval = buffer[i];
        } else {
            u_int32_t aval = this->pVecData[i + start].aval & (~mask[i]);
            this->pVecData[i + start].aval = aval | (buffer[i] & mask[i]);
        }
        this->pVecData[i + start].bval = 0;
    }
    this->_dpi_write();
    this->_sv_to_local();
    this->_update_shadow();
}
bool XData::GetBits(u_int32_t *buffer, u_int32_t count)
{
    Assert(this->mWidth > 0, "only svVec support GetBits");
    this->update_read();
    auto range = std::min(count, this->vecSize);
    bool ret   = true;
    for (int i = 0; i < range; i++) {
        buffer[i] = this->pVecData[i].aval;
        if (this->pVecData[i].bval != 0) { ret = false; }
    }
    return ret;
}

bool XData::GetBits(u_int8_t *buffer, u_int32_t count)
{
    Assert(this->mWidth > 0, "only svVec support GetBits");
    this->update_read();
    auto range = std::min(count, 4 * this->vecSize);
    bool ret   = true;
    for (int i = 0; i < range; i++) {
        buffer[i] = ((unsigned char *)&this->pVecData[i / 4].aval)[i % 4];
        if (this->pVecData[i / 4].bval != 0) { ret = false; }
    }
    return ret;
}

void XData::SetVU8(std::vector<unsigned char> &buffer)
{
    if (unlikely(!this->_check_writeable())) {
        Assert(false, "XData(%s) is not writeable, cannot set bytes",
               this->mName.c_str());
    }
    Assert(this->mWidth > 0, "only svVec support SetVU8");
    int index = 0;
    for (int i = 0; i < this->vecSize; i++) {
        this->pVecData[i].aval = 0;
        this->pVecData[i].bval = 0;
    }
    for (auto &d : buffer) {
        auto vec_idx = index / 4;
        auto vec_off = index % 4;
        if (vec_idx >= this->vecSize) break;
        ((unsigned char *)&this->pVecData[vec_idx].aval)[vec_off] = d;
        ((unsigned char *)&this->pVecData[vec_idx].bval)[vec_off] = 0;
        index += 1;
    }
    this->_dpi_write();
    this->_sv_to_local();
    this->_update_shadow();
}

std::vector<unsigned char> XData::GetVU8()
{
    std::vector<unsigned char> ret;
    Assert(this->mWidth > 0, "only svVec support GetVU8");
    this->update_read();
    for (int i = 0; i < this->vecSize; i++) {
        ret.push_back(((unsigned char *)&this->pVecData[i].aval)[0]);
        ret.push_back(((unsigned char *)&this->pVecData[i].aval)[1]);
        ret.push_back(((unsigned char *)&this->pVecData[i].aval)[2]);
        ret.push_back(((unsigned char *)&this->pVecData[i].aval)[3]);
    }
    return ret;
}

std::vector<unsigned char> XData::GetBvalBytes()
{
    std::vector<unsigned char> ret;
    Assert(this->mWidth > 0, "only svVec support GetBvalBytes");
    this->update_read();
    for (int i = 0; i < this->vecSize; i++) {
        ret.push_back(((unsigned char *)&this->pVecData[i].bval)[0]);
        ret.push_back(((unsigned char *)&this->pVecData[i].bval)[1]);
        ret.push_back(((unsigned char *)&this->pVecData[i].bval)[2]);
        ret.push_back(((unsigned char *)&this->pVecData[i].bval)[3]);
    }
    return ret;
}

void XData::OnChange(xfunction<void, bool, XData *, u_int64_t, void *> func,
                     void *args, std::string desc)
{
    bool init_shadow = !this->has_on_change_cbs;
    if (init_shadow) {
        this->update_read();
    }
    XDataCallBack cb;
    cb.args = args;
    cb.fc   = func;
    cb.desc = desc;
    this->call_back_on_change.push_back(cb);
    this->has_on_change_cbs = true;
    if (init_shadow) {
        this->__mLogicData = this->mLogicData;
        if (this->mWidth > 0 && this->__pVecData) {
            memcpy(this->__pVecData, this->pVecData,
                   this->vecSize * sizeof(xsvLogicVecVal));
        }
    }
}

void XData::ClearOnChangeCbs(){
    this->call_back_on_change.clear();
    this->has_on_change_cbs = false;
}

uint32_t XData::W()
{
    return this->mWidth;
}
uint64_t XData::U()
{
    this->update_read();
    return static_cast<uint64_t>(this->udata);
}
uint64_t XData::XMask()
{
    this->update_read();
    return static_cast<uint64_t>(this->xdata);
}
int64_t XData::S()
{
    this->update_read();
    // A full native word already has its sign bit in the correct position.
    if (this->mWidth == 0 || this->mWidth >= 64) {
        return static_cast<int64_t>(this->udata);
    }

    auto value = this->udata;
    auto sign_bit = ((uint64_t)1) << (this->mWidth - 1);

    if ((value & sign_bit) != 0){
        auto mask = (((uint64_t)1) << this->mWidth) - 1;
        value |= ~mask;
    }
    return value;
}
bool XData::B()
{
    this->update_read();
    return static_cast<bool>(this->udata);
}
std::string XData::String()
{
    this->update_read();
    if (this->mWidth == 0) {
        switch (mLogicData) {
        case 0: return "0"; break;
        case 1: return "1"; break;
        case 2: return "Z"; break;
        case 3: return "X"; break;
        default:;
        }
        Assert(false, "Logic Data value error(%d), need range (0 -> 3)",
               this->mLogicData);
    }
    // svLogicVecVal
    std::string ret;
    u_int8_t abuffer[4]; // 32 bit buffer
    u_int8_t bbuffer[4]; // 32 bit buffer
    for (int i = this->vecSize - 1; i >= 0; i--) {
        *(u_int32_t *)abuffer = this->pVecData[i].aval;
        *(u_int32_t *)bbuffer = this->pVecData[i].bval;
        for (int j = 3; j >= 0; j--) {
            if (i * 32 + j * 8 > this->mWidth) continue;
            if (bbuffer[j] == 0) {
                ret += sFmt("%02x", abuffer[j]);
            } else {
                ret += "??";
            }
        }
    }
    return ret;
}

bool XData::operator==(u_int64_t data)
{
    this->update_read();
    if (this->mWidth > 64) {
        return compare::EqualWords(this->vecSize, 2,
            [this](size_t i) {
                const auto& word = this->pVecData[i];
                return static_cast<uint64_t>(word.aval) | (static_cast<uint64_t>(word.bval) << 32);
            },
            [data](size_t i) { return static_cast<uint64_t>(static_cast<uint32_t>(data >> (i * 32))); });
    }
    return this->udata == data && this->xdata == 0;
}

bool XData::operator==(const std::string &str)
{
    return sLower(this->String()) == sLower(str);
}

bool XData::operator==(std::string &str)
{
    return this->operator==((const std::string)str);
}

bool XData::operator==(char *str)
{
    return this->operator==((const char *)str);
}

bool XData::operator==(const char *str)
{
    return this->operator==(std::string(str));
}

XData &XData::operator=(u_int64_t data)
{
    if (unlikely(!this->_check_writeable())) {
        Assert(false, "XData(%s) is not writeable, cannot assign",
               this->mName.c_str());
    }

    this->udata = data;
    this->xdata = 0;
    this->update_write();
    return *this;
}

XData &XData::operator=(XData &data)
{
    data.update_read();
    if (unlikely(!this->_check_writeable())) {
        Assert(false, "XData(%s) is not writeable, cannot assign",
               this->mName.c_str());
    }
    Assert(this->mWidth == data.mWidth,
           "Need left.mWidth(%d) == right.mWidth(%d)", this->mWidth,
           data.mWidth);
    // raw
    for (int i = 0; i < this->vecSize; i++) {
        this->pVecData[i] = data.pVecData[i];
    }
    this->udata_is_valid = data.udata_is_valid;
    this->mLogicData     = data.mLogicData;
    // update
    this->_sv_to_local();
    this->_dpi_write();
    this->_update_shadow();
    return *this;
}
bool XData::Comp(XData &data, int opcode, int eq){
    // opcode 0: equal, 1: less, 2: greater
    if(opcode == 0)return *this == data;
    if(*this == data){
        if(eq){
            return true;
        }
        return false;
    }
    int order;
    if(this->mWidth < 64 && data.mWidth < 64){
        order = compare::Order(this->udata, data.udata);
    }else{
        Assert(this->mWidth == data.mWidth, "Need left.mWidth(%d) == right.mWidth(%d)", this->mWidth, data.mWidth);
        order = compare::CompareWords(this->vecSize,
                                     [this](size_t i) { return this->pVecData[i].aval; },
                                     [&data](size_t i) { return data.pVecData[i].aval; });
    }
    return opcode == 1 ? order < 0 : order > 0;
}

bool XData::operator==(XData &data)
{
    // update value
    this->update_read();
    data.update_read();
    if (this->mWidth < 64 && data.mWidth < 64) {
        return (this->udata == data.udata) && (this->xdata == data.xdata);
    }
    // logic compare
    auto lgc_cmp_vec = [](XData *lgc, XData *vec) -> bool {
        for (int i = 1; i < vec->vecSize; i++) {
            if (vec->pVecData[i].aval != 0) { return false; }
        }
        if (vec->pVecData[0].aval > 1 | vec->pVecData[0].bval > 1) {
            return false;
        }
        return (vec->pVecData->aval + 2 * vec->pVecData->bval)
               == lgc->mLogicData;
    };
    if (this->mWidth == 0) {
        if (data.mWidth == 0) { return this->mLogicData == data.mLogicData; }
        return lgc_cmp_vec(this, &data);
    } else {
        if (data.mWidth == 0) return lgc_cmp_vec(&data, this);
        return compare::EqualWords(this->vecSize, data.vecSize,
            [this](size_t i) {
                const auto& word = this->pVecData[i];
                return static_cast<uint64_t>(word.aval) | (static_cast<uint64_t>(word.bval) << 32);
            },
            [&data](size_t i) {
                const auto& word = data.pVecData[i];
                return static_cast<uint64_t>(word.aval) | (static_cast<uint64_t>(word.bval) << 32);
            });
    }
}

XData &XData::operator=(const char *str)
{
    std::string raw_str = str;
    return this->operator=(raw_str);
}
XData &XData::operator=(std::string &data)
{
    if (unlikely(!this->_check_writeable())) {
        Assert(false, "XData(%s) is not writeable, cannot assign",
               this->mName.c_str());
    }
    if (this->mWidth == 0) {
        if (sLower(data) == "z") return this->operator=(2);
        if (sLower(data) == "x") return this->operator=(3);
        Assert(false, "for string values, Logica Data only support set Z or X");
    } else {
        if (sLower(data) == "z") {
            for (int i = 0; i < this->vecSize; i++) {
                this->pVecData[i].aval = 0;
                this->pVecData[i].bval = 1;
            }
            return *this;
        }
        if (sLower(data) == "x") {
            for (int i = 0; i < this->vecSize; i++) {
                this->pVecData[i].aval = 1;
                this->pVecData[i].bval = 1;
            }
            return *this;
        }
    }

    auto prefix = data.substr(0, 2);
    Assert(data.length() > 2
               && contians(std::vector<std::string>{"0b", "0x", "::"}, prefix),
           "Input string needs start with: 0b (binary), 0x (hex) or :: (str)");
    this->_zero_sv();

    auto txt  = data.substr(2);
    int tsz   = txt.length();
    int index = 0;
    if (prefix == "0b") {
        txt = sLower(txt);
        for (int i = 0; i < tsz; i++) {
            if (txt[i] == '_') continue;
            Assert(txt[i] == '0' || txt[i] == '1' || txt[i] == 'z'
                       || txt[i] == 'x',
                   "find no 0/1 (%c) value: %s(%d) at %d", txt[i], txt.c_str(),
                   tsz, i);
            auto vec_idx = index / 32;
            auto vec_off = index % 32;
            if (vec_idx >= this->vecSize) break;
            if (txt[i] == '1') {
                bit32_set(this->pVecData[vec_idx].aval, vec_off);
            } else if (txt[i] == 'z') {
                bit32_set(this->pVecData[vec_idx].bval, vec_off);
            } else if (txt[i] == 'x') {
                bit32_set(this->pVecData[vec_idx].aval, vec_off);
                bit32_set(this->pVecData[vec_idx].bval, vec_off);
            }
            index += 1;
        }
    } else if (prefix == "0x") {
        txt = sLower(txt);
        for (int i = tsz - 1; i >= 0; i--) {
            if (txt[i] == '_') continue;
            u_int32_t hex_val = (u_int32_t)txt[i] - 48;
            if (hex_val > 9) { hex_val = (u_int32_t)txt[i] - 97 + 10; }
            Assert(hex_val >= 0 && hex_val <= 15 || hex_val == 0x23
                       || hex_val == 0x21,
                   "find no hex(%c: 0x%x) value: %s(%d) at %d", txt[i], hex_val,
                   txt.c_str(), tsz, i);
            auto vec_idx = index / 8;
            auto vec_off = index % 8;
            if (vec_idx >= this->vecSize) break;
            if (txt[i] == 'z') {
                bit32_hex(this->pVecData[vec_idx].aval, vec_off, 0x0);
                bit32_hex(this->pVecData[vec_idx].bval, vec_off, 0xf);
            } else if (txt[i] == 'x') {
                bit32_hex(this->pVecData[vec_idx].aval, vec_off, 0xf);
                bit32_hex(this->pVecData[vec_idx].bval, vec_off, 0xf);
            } else {
                bit32_hex(this->pVecData[vec_idx].aval, vec_off, hex_val);
            }
            index += 1;
        }
    } else if (prefix == "::") {
        for (int i = 0; i < tsz; i++) {
            if (txt[i] == '_') continue;
            u_int32_t chr_val = (u_int32_t)txt[i];
            auto vec_idx      = index / 4;
            auto vec_off      = index % 4;
            if (vec_idx >= this->vecSize) break;
            bit32_chr(this->pVecData[vec_idx].aval, vec_off, chr_val);
            index += 1;
        }
    }
    this->_dpi_write();
    this->_sv_to_local();
    this->_update_shadow();
    return *this;
}

void XData::ReadFresh(WriteMode m)
{
    if (this->mIOType != IOType::Output) {
        if (this->write_mode != m && m != WriteMode::Imme) return;
    }
    this->_dpi_check();
    if (this->bitRead) this->bitRead(&this->mLogicData);
    if (this->vecRead) this->vecRead(this->pVecData);
    this->_sv_to_local();
    this->_update_shadow();
}

XData &XData::FlipIOType()
{
    if (this->mIOType == IOType::Input) {
        this->mIOType = IOType::Output;
    } else if (this->mIOType == IOType::Output) {
        this->mIOType = IOType::Input;
    }
    return *this;
}

XData &XData::AsBiIO()
{
    this->mIOType = IOType::InOut;
    return *this;
}

XData &XData::AsInIO()
{
    this->mIOType = IOType::Input;
    return *this;
}

XData &XData::AsOutIO()
{
    this->mIOType = IOType::Output;
    return *this;
}

XData &XData::Invert()
{
    if (unlikely(!this->_check_writeable())) {
        Assert(false, "XData(%s) is not writeable, cannot invert",
               this->mName.c_str());
    }
    if (this->mWidth == 0) {
        this->mLogicData = this->mLogicData == 0 ? 1 : 0;
    } else {
        for (int i = 0; i < this->vecSize; i++) {
            this->pVecData[i].aval = ~this->pVecData[i].aval;
            this->pVecData[i].bval = 0;
        }
    }
    this->_dpi_write();
    this->_sv_to_local();
    this->_update_shadow();
    return *this;
}
XData::operator u_int64_t()
{
    this->update_read();
    return this->udata;
}
XData::operator std::string()
{
    return this->mName;
}
PinBind &XData::operator[](u_int32_t index)
{
    this->update_read();
    Assert(index < this->mWidth, "Index[%d] need range 0 t %d", index,
           this->mWidth);
    if (this->mWidth > 0) { return *this->pinbind_vec[index]; }
    return this->pinbind_bit;
}


} // namespace xspcomm
