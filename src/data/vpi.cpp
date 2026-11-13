#include "xspcomm/xdata.h"
#include "data/vpi_storage.h"

namespace xspcomm {

bool XData::BindVPI(vpiHandle obj, func_vpi_get get, func_vpi_get_value get_value,
                    func_vpi_put_value put_value, std::string name) {
    if (vpi_obj_handle || !obj || !get || !get_value || !put_value) return false;
    const auto type = get(vpiType, obj);
    const bool packed_struct = (type == vpiStructVar || type == vpiStructNet) && get(vpiPacked, obj) == 1;
    if (!detail::VpiLeafType(type) && !packed_struct) return false;
    const auto bits = get(vpiSize, obj);
    if (bits <= 0) return false;
    const auto direction = get(vpiDirection, obj);
    const auto mode = direction == vpiOutput ? IOType::Output : IOType::Input;
    // Preserve a caller's explicit one-bit vector layout; the traditional
    // FromVPI entry point still normalizes a one-bit object to scalar XData.
    const uint32_t width = bits == 1 && mWidth != 1 ? 0 : uint32_t(bits);
    ReInit(width, mode, name);
    vpi_obj_handle = obj;
    vpi_get = get;
    vpi_get_value = get_value;
    vpi_put_value = put_value;
    backend_kind = XDataBackendKind::VPI;
    readonly_backend = mode == IOType::Output;
    if (!readonly_backend) AsImmWrite();
    update_read();
    return true;
}

void XData::_vpi_read() {
    if (vpi_owner) vpi_owner->context->Check();
    s_vpi_value value{};
    if (!mWidth) {
        value.format = vpi_data_type == VPI_XDATA_INTEGER ? vpiIntVal : vpiScalarVal;
        vpi_get_value(vpi_obj_handle, &value);
        mLogicData = xsvLogic(value.format == vpiIntVal ? value.value.integer : value.value.scalar);
    } else if (vpi_data_type == VPI_XDATA_INTEGER) {
        value.format = vpiIntVal;
        vpi_get_value(vpi_obj_handle, &value);
        _zero_sv();
        pVecData[0].aval = PLI_UINT32(value.value.integer);
    } else {
        value.format = vpiVectorVal;
        vpi_get_value(vpi_obj_handle, &value);
        if (!value.value.vector) throw std::runtime_error("VPI vector read returned null: " + mName);
        for (uint32_t i = 0; i < vecSize; ++i) {
            pVecData[i].aval = value.value.vector[i].aval;
            pVecData[i].bval = value.value.vector[i].bval;
        }
    }
}

void XData::_vpi_write() {
    if (vpi_owner) vpi_owner->context->Check();
    s_vpi_value value{};
    if (!mWidth) {
        value.format = vpi_data_type == VPI_XDATA_INTEGER ? vpiIntVal : vpiScalarVal;
        value.value.integer = mLogicData;
    } else if (vpi_data_type == VPI_XDATA_INTEGER) {
        value.format = vpiIntVal;
        value.value.integer = PLI_INT32(pVecData[0].aval);
    } else {
        value.format = vpiVectorVal;
        value.value.vector = reinterpret_cast<s_vpi_vecval *>(pVecData);
    }
    vpi_put_value(vpi_obj_handle, &value, nullptr, vpi_obj_wflage);
}

XData *XData::FromVPI(vpiHandle obj, func_vpi_get get, func_vpi_get_value get_value,
                       func_vpi_put_value put_value, std::string name) {
    auto data = std::make_unique<XData>(0, IOType::InOut, name);
    if (!data->BindVPI(obj, get, get_value, put_value, name)) return nullptr;
    return data.release();
}
} // namespace xspcomm
