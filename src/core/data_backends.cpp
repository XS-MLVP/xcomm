#include "xspcomm/xdata.h"
#include "xspcomm/xexpr.h"

#include "xspcomm/detail/data/native_memory.h"

namespace xspcomm {

void XData::BindDPIPtr(uint64_t read_ptr, uint64_t write_ptr)
{
    if (this->mWidth > 0) {
        this->vecRead  = (void (*)(void *))read_ptr;
        this->vecWrite = (void (*)(const void *))write_ptr;
    } else {
        this->bitRead  = (void (*)(void *))read_ptr;
        this->bitWrite = (void (*)(const unsigned char))write_ptr;
    }
    this->backend_kind = XDataBackendKind::DPI;
    this->readonly_backend = false;
    this->update_read();
}

void XData::BindDPIRW(xfunction<void, void *> read,
                      xfunction<void, void *> write)
{
    Assert(this->mWidth > 0, "XData(name=%s).mWidth(%d) need > 0",
           this->mName.c_str(), this->mWidth);
    this->vecRead  = read;
    this->vecWrite = write;
    this->backend_kind = XDataBackendKind::DPI;
    this->readonly_backend = false;
    this->update_read();
}
void XData::BindDPIRW(xfunction<void, void *> read,
                      xfunction<void, unsigned char> write)
{
    Assert(this->mWidth == 0, "XData(name=%s).mWidth(%d) need == 0",
           this->mName.c_str(), this->mWidth);
    this->bitRead  = read;
    this->bitWrite = write;
    this->backend_kind = XDataBackendKind::DPI;
    this->readonly_backend = false;
    this->update_read();
}
void XData::BindDPIRW(void (*read)(void *), void (*write)(const void *)) {
    Assert(this->mWidth > 0, "XData(name=%s).mWidth(%d) need > 0",
           this->mName.c_str(), this->mWidth);
    this->vecRead  = read;
    this->vecWrite = write;
    this->backend_kind = XDataBackendKind::DPI;
    this->readonly_backend = false;
    this->update_read();
}
void XData::BindDPIRW(void (*read)(void *), void (*write)(const unsigned char)) {
    Assert(this->mWidth == 0, "XData(name=%s).mWidth(%d) need == 0",
           this->mName.c_str(), this->mWidth);
    this->bitRead  = read;
    this->bitWrite = write;
    this->backend_kind = XDataBackendKind::DPI;
    this->readonly_backend = false;
    this->update_read();
}
// TODO: Define how native two-state reads handle an existing X/Z mask and how
// X/Z writes map to native storage. Preserve bval until that policy is settled;
// see docs/xdata_xclock_optimizations.md (BindNativeData X/Z semantics).
void XData::BindNativeData(uint64_t pdata){
    if (this->mWidth == 0){
        auto* native = reinterpret_cast<xsvLogic*>(pdata);
        this->bitRead = [native](xsvLogic* dst){
            *dst = *native;
        };
        this->bitWrite = [native](xsvLogic value){
            *native = value;
        };
    } else if (this->mWidth <= 8) {
        detail::VecMemoryIO<uint8_t, 1>::Bind(pdata, this->vecSize, this->vecRead, this->vecWrite);
    } else if (this->mWidth <= 16) {
        detail::VecMemoryIO<uint16_t, 1>::Bind(pdata, this->vecSize, this->vecRead, this->vecWrite);
    } else {
#define XCOMM_NATIVE_WORD_COUNTS(M) \
    M(1) M(2) M(3) M(4) M(5) M(6) M(7) M(8) \
    M(9) M(10) M(11) M(12) M(13) M(14) M(15) M(16)
#define XCOMM_BIND_NATIVE_CASE(N) \
    case N: \
        detail::VecMemoryIO<uint32_t, N>::Bind(pdata, this->vecSize, this->vecRead, this->vecWrite); \
        break;

        switch (this->vecSize) {
        XCOMM_NATIVE_WORD_COUNTS(XCOMM_BIND_NATIVE_CASE)
        default:
            detail::VecMemoryIO<uint32_t>::Bind(pdata, this->vecSize, this->vecRead, this->vecWrite);
            break;
        }
#undef XCOMM_BIND_NATIVE_CASE
#undef XCOMM_NATIVE_WORD_COUNTS
    }
    this->backend_kind = XDataBackendKind::MemDirect;
    this->readonly_backend = false;
    this->update_read();
}
void XData::BindMixFromUvs(uint64_t pdata)
{
    if (this->mWidth == 0) { // scalar and 1bit vector
        this->bitRead = [pdata](void *d) {
            *(xsvLogic *)d = *(uint8_t *)pdata;
        };
    } else {
        if (this->mWidth <= 8) {
            this->vecRead = [this, pdata](void *d) {
                ((xsvLogicVecVal *)d)->aval = *(uint8_t *)pdata;
                // ((xsvLogicVecVal *)d)->bval = 0;
            };
        } else if (this->mWidth <= 16) {
            this->vecRead = [this, pdata](void *d) {
                ((xsvLogicVecVal *)d)->aval = *(uint16_t *)pdata;
                // ((xsvLogicVecVal *)d)->bval = 0;
            };
        } else if (this->mWidth <= 32) {
            this->vecRead = [this, pdata](void *d) {
                ((xsvLogicVecVal *)d)->aval = *(uint32_t *)pdata;
                // ((xsvLogicVecVal *)d)->bval = 0;
            };
        } else if (this->mWidth <= 64) {
            this->vecRead = [this, pdata](void *d) {
                ((xsvLogicVecVal *)d)->aval = ((uint32_t *)pdata)[0];
                // ((xsvLogicVecVal *)d)->bval = 0;
                ((xsvLogicVecVal *)d)[1].aval = ((uint32_t *)pdata)[1];
                // ((xsvLogicVecVal *)d)[1].bval = 0;
            };
        } else { // wider
            this->vecRead = [this, pdata](void *d) {
                for (int i = 0; i < this->vecSize; i++) {
                    ((xsvLogicVecVal *)d)[i].aval = ((uint32_t *)pdata)[i];
                    // ((xsvLogicVecVal *)d)[i].bval = 0;
                }
            };
        }
    }
}
void XData::BindExpr(std::shared_ptr<ExprEngine> engine, int root_id){
    Assert(engine != nullptr, "BindExpr engine is null");
    if (this->mWidth == 0) {
        this->bitRead = [engine, root_id](void *d){
            auto value = engine->Eval(root_id);
            *(xsvLogic *)d = (xsvLogic)((value & 0x1) ? 1 : 0);
        };
        this->bitWrite = nullptr;
        this->vecRead = nullptr;
        this->vecWrite = nullptr;
    } else {
        this->vecRead = [this, engine, root_id](void *d){
            auto value = engine->Eval(root_id);
            auto *vec = (xsvLogicVecVal *)d;
            for (uint32_t i = 0; i < this->vecSize; i++) {
                vec[i].aval = 0;
                vec[i].bval = 0;
            }
            for (uint32_t i = 0; i < std::min<uint32_t>(2, this->vecSize); i++) {
                vec[i].aval = (uint32_t)((value >> (i * 32)) & 0xffffffffULL);
            }
        };
        this->vecWrite = nullptr;
        this->bitRead = nullptr;
        this->bitWrite = nullptr;
    }
    this->backend_kind = XDataBackendKind::Expr;
    this->readonly_backend = true;
    this->mIOType = IOType::Output;
    this->update_read();
}
void XData::BindConst(uint64_t value){
    if (this->mWidth == 0) {
        this->bitRead = [value](void *d){
            *(xsvLogic *)d = (xsvLogic)((value & 0x1) ? 1 : 0);
        };
        this->bitWrite = nullptr;
        this->vecRead = nullptr;
        this->vecWrite = nullptr;
    } else {
        this->vecRead = [this, value](void *d){
            auto *vec = (xsvLogicVecVal *)d;
            for (uint32_t i = 0; i < this->vecSize; i++) {
                vec[i].aval = 0;
                vec[i].bval = 0;
            }
            for (uint32_t i = 0; i < std::min<uint32_t>(2, this->vecSize); i++) {
                vec[i].aval = (uint32_t)((value >> (i * 32)) & 0xffffffffULL);
            }
        };
        this->vecWrite = nullptr;
        this->bitRead = nullptr;
        this->bitWrite = nullptr;
    }
    this->backend_kind = XDataBackendKind::Const;
    this->readonly_backend = true;
    this->mIOType = IOType::Output;
    this->update_read();
}
void XData::_TestBindDPIL()
{
    this->BindDPIRW(TEST_DPI_LR, TEST_DPI_LW);
}

void XData::_TestBindDPIV()
{
    this->BindDPIRW(TEST_DPI_VR, TEST_DPI_VW);
}

} // namespace xspcomm
