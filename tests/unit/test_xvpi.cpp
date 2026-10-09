#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xdata.h"

#include <array>
#include <memory>

using namespace xspcomm;

namespace {

struct MockVpiObject {
    PLI_INT32 width = 64;
    PLI_INT32 direction = vpiOutput;
    std::array<s_vpi_vecval, 2> value{};
};

MockVpiObject *as_mock(vpiHandle handle){
    return reinterpret_cast<MockVpiObject *>(handle);
}

PLI_INT32 mock_vpi_get(PLI_INT32 property, vpiHandle handle){
    auto obj = as_mock(handle);
    if(property == vpiType) return vpiReg;
    if(property == vpiSize) return obj->width;
    if(property == vpiDirection) return obj->direction;
    return 0;
}

void mock_vpi_get_value(vpiHandle handle, p_vpi_value value){
    auto obj = as_mock(handle);
    if(value->format == vpiIntVal){
        value->value.integer = (PLI_INT32)obj->value[0].aval;
        return;
    }
    if(value->format == vpiVectorVal){
        // VPI returns simulator-owned vector storage through this pointer.
        value->value.vector = obj->value.data();
    }
}

vpiHandle mock_vpi_put_value(vpiHandle handle, p_vpi_value value,
                            p_vpi_time, PLI_INT32){
    auto obj = as_mock(handle);
    if(value->format == vpiIntVal){
        obj->value[0].aval = (PLI_UINT32)value->value.integer;
        obj->value[0].bval = 0;
    }else if(value->format == vpiVectorVal){
        for(size_t i = 0; i < obj->value.size(); i++){
            obj->value[i] = value->value.vector[i];
        }
    }
    return handle;
}

vpiHandle as_handle(MockVpiObject &obj){
    return reinterpret_cast<vpiHandle>(&obj);
}

} // namespace

TEST_CASE("VPI reads 64-bit vectors from returned storage", "[xvpi]") {
    MockVpiObject obj;
    obj.value[0] = {0x89abcdefU, 0};
    obj.value[1] = {0x01234567U, 0x00000010U};

    std::unique_ptr<XData> data(XData::FromVPI(
        as_handle(obj), mock_vpi_get, mock_vpi_get_value, mock_vpi_put_value,
        "vpi_read64"));

    REQUIRE(data != nullptr);
    REQUIRE(data->W() == 64);
    REQUIRE(data->U() == 0x0123456789abcdefULL);
    REQUIRE_FALSE(data->DataValid());
}

TEST_CASE("VPI writes 64-bit vectors as two words", "[xvpi]") {
    MockVpiObject obj;
    obj.direction = vpiInput;

    std::unique_ptr<XData> data(XData::FromVPI(
        as_handle(obj), mock_vpi_get, mock_vpi_get_value, mock_vpi_put_value,
        "vpi_write64"));

    REQUIRE(data != nullptr);
    *data = 0xfedcba9876543210ULL;
    REQUIRE(obj.value[0].aval == 0x76543210U);
    REQUIRE(obj.value[1].aval == 0xfedcba98U);
    REQUIRE(obj.value[0].bval == 0);
    REQUIRE(obj.value[1].bval == 0);
}
