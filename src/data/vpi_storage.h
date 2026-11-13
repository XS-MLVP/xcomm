#ifndef XSPCOMM_DATA_VPI_STORAGE_H
#define XSPCOMM_DATA_VPI_STORAGE_H

#include "xspcomm/xvpi.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace xspcomm::detail {
inline bool VpiLeafType(PLI_INT32 type) {
    switch (type) {
    case vpiNet: case vpiReg: case vpiMemoryWord: case vpiBitVar:
    case vpiIntVar: case vpiShortIntVar: case vpiLongIntVar: case vpiByteVar:
    case vpiIntegerVar: case vpiTimeVar: case vpiEnumVar:
    case vpiPackedArrayVar: case vpiPackedArrayNet: return true;
    default: return false;
    }
}
struct VpiOwnedHandle;
struct VpiContext : std::enable_shared_from_this<VpiContext> {
    func_vpi_get get = nullptr;
    func_vpi_get_value get_value = nullptr;
    func_vpi_put_value put_value = nullptr;
    vpiHandle (*by_name)(char *, vpiHandle) = nullptr;
    vpiHandle (*by_index)(vpiHandle, PLI_INT32) = nullptr;
    vpiHandle (*handle)(PLI_INT32, vpiHandle) = nullptr;
    vpiHandle (*iterate)(PLI_INT32, vpiHandle) = nullptr;
    vpiHandle (*scan)(vpiHandle) = nullptr;
    func_vpi_get_str get_str = nullptr;
    PLI_INT32 (*release)(vpiHandle) = nullptr;
    std::string scope;
    bool open = true;
    std::vector<std::weak_ptr<VpiOwnedHandle>> handles;
    void Check() const {
        if (!open) throw std::runtime_error("VPI session is closed; the DUT must remain alive");
    }
    std::shared_ptr<VpiOwnedHandle> Own(vpiHandle value);
    void Close();
};
struct VpiOwnedHandle {
    std::shared_ptr<VpiContext> context;
    vpiHandle value;
    VpiOwnedHandle(std::shared_ptr<VpiContext> context, vpiHandle value)
        : context(std::move(context)), value(value) {}
    void Release() {
        if (value) { context->release(value); value = nullptr; }
    }
    ~VpiOwnedHandle() { if (context->open) Release(); }
};
inline std::shared_ptr<VpiOwnedHandle> VpiContext::Own(vpiHandle value) {
    Check();
    if (!value) throw std::runtime_error("Simulator did not expose the requested VPI member or index");
    auto result = std::make_shared<VpiOwnedHandle>(shared_from_this(), value);
    handles.emplace_back(result);
    return result;
}
inline void VpiContext::Close() {
    if (!open) return;
    for (auto &weak : handles) if (auto owned = weak.lock()) owned->Release();
    handles.clear();
    open = false;
}
} // namespace xspcomm::detail
#endif
