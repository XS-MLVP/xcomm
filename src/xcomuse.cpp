
#include "xspcomm/xcomuse.h"


namespace xspcomm {

u_int64_t ComUseStepCb::GetCb(){
    return (u_int64_t)ComUseStepCb::Cb;
}

void ComUseStepCb::Cb(uint64_t c, void *self){
    ComUseStepCb *p = (ComUseStepCb*)self;
    if(!p->cb_enable)return;
    p->cycle = c;
    p->Call();
    if(p->cb_maxcts > 0 && p->cb_counts >= p->cb_maxcts)p->Disable();
}

void ComUseStepCb::Disable(){
    this->cb_enable = false;
}
void ComUseStepCb::Enable(){
    this->cb_enable = true;
}
bool ComUseStepCb::IsDisable(){
    return !this->cb_enable;
}
int ComUseStepCb::GetCbCount(){
    return this->cb_counts;
}
int ComUseStepCb::IncCbCount(){
    this->cb_counts += 1;
    return this->cb_counts;
}
int ComUseStepCb::DecCbCount(){
    this->cb_counts -= 1;
    return this->cb_counts;
}
void ComUseStepCb::SetMaxCbs(int c){
    this->cb_maxcts = c;
}
void ComUseStepCb::Reset(){
    this->cb_enable = true;
    this->cb_counts = 0;
}

void ComUseStepCb::Call(){
    fprintf(stderr, "Error, This is a virtual Call!\n");
}

void ComUseEcho::Call(){
    //convert; // 0 (char), 1 (int), 2 (float), 3 (double), 4 (string)
    Assert(this->valid != NULL, "Pin[valid] is Null");
    Assert(this->data  != NULL, "Pin[data] is Null");
    auto o = this->stderr_echo ? stderr: stdout;
    // Echo
    if((*this->valid) != 0){
        switch (this->convert)
        {
        case 0:fprintf(o, this->fmt.c_str(), (char)(*this->data));break;
        case 1:fprintf(o, this->fmt.c_str(), (int64_t)(*this->data));break;
        case 2:fprintf(o, this->fmt.c_str(), (float)(*this->data));break;
        case 3:fprintf(o, this->fmt.c_str(), (double)(*this->data));break;
        case 4:fprintf(o, this->fmt.c_str(), this->data->String().c_str());break;
        default: Assert(0, "convert type error: %d", this->convert);
            break;
        }
    }
}

uint64_t GetFromU64Array(uint64_t address, int index){
    return ((uint64_t *)address)[index];
}
uint32_t GetFromU32Array(uint64_t address, int index){
    return ((uint32_t *)address)[index];
}
uint8_t  GetFromU8Array(uint64_t address,  int index){
    return ((uint8_t *)address)[index];
}

uint64_t GetFromU64Array(unsigned long long * address, int index){
    return address[index];
}
uint32_t GetFromU32Array(unsigned int * address, int index){
    return address[index];
}
uint8_t   GetFromU8Array(unsigned char * address, int index){
    return address[index];
}

// Get Array Item
void SetU64Array(uint64_t address, int index, uint64_t data){((uint64_t*)address)[index]=data;}
void SetU32Array(uint64_t address, int index, uint32_t data){((uint32_t*)address)[index]=data;}
void  SetU8Array(uint64_t address, int index, uint8_t  data){((uint8_t*)address)[index] =data;}

void SetU64Array(unsigned long long *address, int index, uint64_t data){address[index]=data;}
void SetU32Array(unsigned int       *address, int index, uint32_t data){address[index]=data;}
void  SetU8Array(unsigned char      *address, int index, uint8_t  data){address[index]=data;}

uint64_t U64PtrAsU64(unsigned long long *p){return (uint64_t)p;}
uint64_t U32PtrAsU64(unsigned int *p){return (uint64_t)p;}
uint64_t  U8PtrAsU64(unsigned char *p){return (uint64_t)p;}

unsigned long long *U64AsU64Ptr(uint64_t p){return (unsigned long long *)p;}
unsigned int       *U64AsU32Ptr(uint64_t p){return (unsigned int *)p;}
unsigned char      *U64AsU8Ptr(uint64_t p){return (unsigned char *)p;}

void ComUseCondCheck::BindXClock(XClock *clk){
    this->clk_list.push_back(clk);
}
void ComUseCondCheck::SetCondition(std::string unique_name, XData* pin, XData* val, ComUseCondCmp cmp, XData *valid, XData *valid_value, xfunction<bool, XData*, XData*, uint64_t> func, uint64_t arg){
    Assert(func == nullptr, "func type error: %d", func.func == nullptr);
    std::tuple<XData*, XData*, ComUseCondCmp, XData*, XData*, xfunction<bool, XData*, XData*, uint64_t>, int, uint64_t> t(pin, val, cmp, valid, valid_value, func, 0, arg);
    this->cond_map_xdata[unique_name] = t;
    this->cond_map_uint64.erase(unique_name);
};
void ComUseCondCheck::SetCondition(std::string unique_name, uint64_t pin_ptr, uint64_t val_ptr, ComUseCondCmp cmp, int bytes, uint64_t valid_ptr, uint64_t valid_value_ptr, int valid_bytes, xfunction<bool, uint64_t, uint64_t, uint64_t> func, uint64_t arg){
    std::tuple<uint64_t, uint64_t, ComUseCondCmp, int, uint64_t, uint64_t, int, xfunction<bool, uint64_t, uint64_t, uint64_t>, int, uint64_t> t(pin_ptr, val_ptr, cmp, bytes, valid_ptr, valid_value_ptr, valid_bytes, func, 0, arg);
    this->cond_map_uint64[unique_name] = t;
    this->cond_map_xdata.erase(unique_name);
};
void ComUseCondCheck::RemoveCondition(std::string unique_name){
    this->cond_map_xdata.erase(unique_name);
    this->cond_map_uint64.erase(unique_name);
};
std::vector<std::string> ComUseCondCheck::GetTriggeredConditionKeys(){
    std::vector<std::string> ret;
    for(auto &e : this->cond_map_xdata){
        auto [_1, _2, _3, _4, _5, _6, valid, arg] = e.second;
        if(valid)ret.push_back(e.first);
    }
    for(auto &e : this->cond_map_uint64){
        auto [_1, _2, _3, _4, _5, _6, _7, _8, valid, arg] = e.second;
        if(valid)ret.push_back(e.first);
    }
    return ret;
}
std::map<std::string, bool> ComUseCondCheck::ListCondition(){
    std::map<std::string, bool> ret;
    for(auto &e : this->cond_map_xdata){
        auto [_1, _2, _3, _4, _5, _6, valid, arg] = e.second;
        if(valid){
            ret[e.first] = true;
        }else{
            ret[e.first] = false;
        }
    }
    for(auto &e : this->cond_map_uint64){
        auto [_1, _2, _3, _4, _5, _6, _7, _8, valid, arg] = e.second;
        if(valid){
            ret[e.first] = true;
        }else{
            ret[e.first] = false;
        }
    }
    return ret;
}
void ComUseCondCheck::ClearClock(){this->clk_list.clear();}
void ComUseCondCheck::ClearCondition(){this->cond_map_xdata.clear(); this->cond_map_uint64.clear();};
void ComUseCondCheck::Call(){
    // check XData condition
    bool is_triggered = false;
    for(auto &e : this->cond_map_xdata){
        auto [pin, val, cmp, valid, valid_value, func, _, arg] = e.second;
        // not triggered
        std::get<6>(e.second) = 0;
        if (valid != nullptr){
            Assert(valid_value != nullptr, "valid_value is null");
            if(*valid != *valid_value)continue;
        }
        // check cmp
        if(func){
            if(func(pin, val, arg)){
                std::get<6>(e.second) = 1;
            }
        }else{
            switch (cmp)
            {
            case ComUseCondCmp::EQ: // EQ
                if(*pin == *val){
                    std::get<6>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::NE: // NE
                if(*pin != *val){
                    std::get<6>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::GT: // GT
                if(*pin > *val){
                    std::get<6>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::GE: // GE
                if(*pin >= *val){
                    std::get<6>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::LT: // LT
                if(*pin < *val){
                    std::get<6>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::LE: // LE
                if(*pin <= *val){
                    std::get<6>(e.second) = 1;
                }
                break;
            default:
                Assert(0, "cmp xdata type error: %d", (int)cmp);
                break;
            }
        }
        if(!is_triggered){
            if(std::get<6>(e.second)){
                for(auto &clk : this->clk_list){
                    clk->Disable();
                }
                is_triggered = true;
                this->IncCbCount();
            }
        }
    }
    // check uint64_t condition
    for(auto &e : this->cond_map_uint64){
        auto [pin_ptr, val_ptr, cmp, bytes, valid_ptr, valid_value_ptr, valid_bytes, func, _, arg] = e.second;
        // not triggered
        std::get<8>(e.second) = 0;
        if (valid_ptr != 0){
            Assert(valid_value_ptr != 0, "valid_value is null");
            if(memcmp((void*)valid_ptr, (void*)valid_value_ptr, valid_bytes) != 0)continue;
        }
        // check cmp
        if(func){
            if(func(pin_ptr, val_ptr, arg)){
                std::get<8>(e.second) = 1;
            }
        }else{
            switch (cmp)
            {
            case ComUseCondCmp::EQ: // EQ
                if(memcmp((void*)pin_ptr, (void*)val_ptr, bytes) == 0){
                    std::get<8>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::NE: // NE
                if(memcmp((void*)pin_ptr, (void*)val_ptr, bytes) != 0){
                    std::get<8>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::GT: // GT
                if(memcmp((void*)pin_ptr, (void*)val_ptr, bytes) > 0){
                    std::get<8>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::GE: // GE
                if(memcmp((void*)pin_ptr, (void*)val_ptr, bytes) >= 0){
                    std::get<8>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::LT: // LT
                if(memcmp((void*)pin_ptr, (void*)val_ptr, bytes) < 0){
                    std::get<8>(e.second) = 1;
                }
                break;
            case ComUseCondCmp::LE: // LE
                if(memcmp((void*)pin_ptr, (void*)val_ptr, bytes) <= 0){
                    std::get<8>(e.second) = 1;
                }
                break;
            default:
                Assert(0, "cmp ptr type error: %d", (int)cmp);
                break;
            }
        }
        if(!is_triggered){
            if(std::get<8>(e.second)){
                for(auto &clk : this->clk_list){
                    clk->Disable();
                }
                is_triggered = true;
                this->IncCbCount();
            }
        }
    }
};

} // namespace xspcomm
