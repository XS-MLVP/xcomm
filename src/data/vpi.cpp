#include "xspcomm/xdata.h"

namespace xspcomm {

bool XData::BindVPI(vpiHandle obj, func_vpi_get get,
               func_vpi_get_value get_value, func_vpi_put_value put_value, std::string name){
    if(this->vpi_obj_handle != nullptr){
        Warn("BindVPI: already bind, ignore bind");
        return false;
    }
    if(obj == nullptr){
        Warn("BindVPI: obj is nullptr, ignore bind");
        return false;
    }
    if(get == nullptr || get_value == nullptr || put_value == nullptr){
        Warn("BindVPI: get/get_value/put_value is nullptr, ignore bind");
        return false;
    }
    PLI_INT32 type = get(vpiType, obj);
    if(type != vpiNet && type != vpiReg){
        Warn("BindVPI: obj type is not vpiNet or vpiReg, ignore bind");
        return false;
    }
    this->vpi_obj_handle = obj;
    this->vpi_get = get;
    this->vpi_get_value = get_value;
    this->vpi_put_value = put_value;
    // Get data width
    PLI_INT32 bit_size = get(vpiSize, obj);
    if(bit_size == 1){
        bit_size = 0;
    }
    PLI_INT32 direction = vpi_get(vpiDirection, obj);
    bool writeable = (direction != vpiOutput);
    auto mode = IOType::Input;
    if (!writeable) {
        mode = IOType::Output;
    }
    // Reinit self
    this->ReInit(bit_size, mode, name);
    // Define and bind fake DPIRW
    if(bit_size == 0){ // As logic
        auto fake_dpir = [this](void *data){
            t_vpi_value value;
            value.format = vpiIntVal;
            if(this->vpi_data_type == VPI_XDATA_SCALAR){
                value.format = vpiScalarVal;
                this->vpi_get_value(this->vpi_obj_handle, &value);
                *(xsvLogic *)data = (xsvLogic)value.value.scalar;
            }else{
                this->vpi_get_value(this->vpi_obj_handle, &value);
                *(xsvLogic *)data = (xsvLogic)value.value.integer;
            }
        };
        auto fake_dpiw = [this](unsigned char data){
            t_vpi_value value;
            value.format = vpiIntVal;
            value.value.integer = data;
            if(this->vpi_data_type == VPI_XDATA_SCALAR){
                value.format = vpiScalarVal;
                value.value.scalar = data;
            }
            this->vpi_put_value(this->vpi_obj_handle, &value, nullptr, this->vpi_obj_wflage);
        };
        if(writeable){
            this->BindDPIRW(fake_dpir, fake_dpiw);
        }else{
            this->BindDPIRW(fake_dpir, (void(*)(unsigned char))nullptr);
        }
    }else{ // As vec
        auto fake_dpir = [this](void *data){
            t_vpi_value value;
            if((this->W() <= 32 && this->vpi_data_type == VPI_XDATA_AUTOCHECK) || this->vpi_data_type == VPI_XDATA_INTEGER){
                value.format = vpiIntVal;
                this->vpi_get_value(this->vpi_obj_handle, &value);
                this->pVecData[0].aval = value.value.integer;
                this->pVecData[0].bval = 0;
            }else{
                value.format = vpiVectorVal;
                value.value.vector = nullptr;
                this->vpi_get_value(this->vpi_obj_handle, &value);
                if(value.value.vector == nullptr){
                    Error("VPI vector read returned nullptr for XData(%s)", this->mName.c_str());
                    this->_zero_sv();
                    return;
                }
                auto dst = (xsvLogicVecVal *)data;
                for(uint32_t i = 0; i < this->vecSize; i++){
                    dst[i].aval = value.value.vector[i].aval;
                    dst[i].bval = value.value.vector[i].bval;
                }
            }
        };
        auto fake_dpiw = [this](void *data){
            t_vpi_value value;
            value.format = vpiVectorVal;
            value.value.vector = (s_vpi_vecval *)data;
            if((this->W() <= 32 && this->vpi_data_type == VPI_XDATA_AUTOCHECK) || this->vpi_data_type == VPI_XDATA_INTEGER){
                value.format = vpiIntVal;
                value.value.integer = this->pVecData[0].aval;
            }
            this->vpi_put_value(this->vpi_obj_handle, &value, nullptr, this->vpi_obj_wflage);
        };
        if(writeable){
            this->BindDPIRW(fake_dpir, fake_dpiw);
        }else{
            this->BindDPIRW(fake_dpir, (void(*)(void *))nullptr);
        }
    }
    if(writeable){
        this->AsImmWrite();
    }
    this->backend_kind = XDataBackendKind::VPI;
    this->readonly_backend = !writeable;
    return true;
}

XData * XData::FromVPI(vpiHandle obj, func_vpi_get get, func_vpi_get_value get_value, func_vpi_put_value put_value, std::string name){
    auto xdata = new XData(0, IOType::InOut, name);
    if(xdata->BindVPI(obj, get, get_value, put_value, name)){
        return xdata;
    }
    delete xdata;
    return nullptr;
}

} // namespace xspcomm
