#include "xspcomm/xdata.h"

namespace xspcomm {

// class PinBind
PinBind::PinBind(xsvLogicVecVal *p, int index)
{
    this->pVec  = p;
    this->index = index / 32;
    this->mask  = 1 << index % 32;
}
PinBind::PinBind(xsvLogic *p)
{
    this->pLgc  = p;
    this->index = -1;
}

PinBind &PinBind::Set(int v)
{
    return this->operator=(v);
};

int PinBind::AsInt32()
{
    return (int)*this;
}

PinBind &PinBind::operator=(const u_int8_t &v)
{
    auto val = (u_int8_t)v;
    Assert(val >= 0 && val <= 3,
           "Set value[%d(to uint8)] must in rang [0,1,2,3] 01ZX", val);
    if (this->pLgc) {
        *this->pLgc = val;
    } else if (this->pVec) {
        switch (val) {
        case 0:
            bit32_zro(this->pVec[this->index].aval, this->mask);
            bit32_zro(this->pVec[this->index].bval, this->mask);
            break;
        case 1:
            bit32_one(this->pVec[this->index].aval, this->mask);
            bit32_zro(this->pVec[this->index].bval, this->mask);
            break;
        case 2:
            bit32_zro(this->pVec[this->index].aval, this->mask);
            bit32_one(this->pVec[this->index].bval, this->mask);
            break;
        case 3:
            bit32_one(this->pVec[this->index].aval, this->mask);
            bit32_one(this->pVec[this->index].bval, this->mask);
            break;
        default:
            Assert(false, "Set value[%d] must in rang [0,1,2,3] 01ZX", val);
            break;
        }
    } else {
        Assert(false, "Pin is not bind to any svLogic data");
    }
    // write to dpi
    if (this->write_fc) this->write_fc();
    return *this;
}

std::string PinBind::AsString(){
    int v = this->AsInt32();
    switch (v)
    {
    case 0:
        return "0";
    case 1:
        return "1";
    case 2:
        return "z";
    case 3:
        return "x";
    }
    Assert(false, "Error! PinBind::AsString() return invalid value[%d], need in [0,1,2,3]", v);
}

PinBind &PinBind::operator=(const std::string &v)
{
    if (sLower(v) == "z") { return this->operator=(2); }
    if (sLower(v) == "x") { return this->operator=(3); }
    Assert(false, "Only support set Z or X");
    return *this;
}

PinBind &PinBind::operator=(std::string &v)
{
    return this->operator=((const std::string &)v);
}

PinBind::operator u_int8_t()
{
    if (this->pLgc) {
        return static_cast<u_int8_t>(*this->pLgc);
    } else if (this->pVec) {
        uint8_t aval = bit32_val(this->pVec[this->index].aval, this->mask);
        uint8_t bval = bit32_val(this->pVec[this->index].bval, this->mask);
        return bval * 2 + aval;
    } else {
        Assert(false, "PinType error");
    }
}

} // namespace xspcomm
