
#include "xspcomm/xcomuse_base.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>


namespace xspcomm {

u_int64_t ComUseStepCb::GetCb(){
    return (u_int64_t)ComUseStepCb::Cb;
}

void ComUseStepCb::Cb(uint64_t c, void *self){
    ComUseStepCb *p = (ComUseStepCb*)self;
    if (unlikely(!p->cb_enable)) return;
    p->cycle = c;
    p->Call();
    if (unlikely(p->cb_maxcts > 0 && p->cb_counts >= p->cb_maxcts)) p->Disable();
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
    RemoveUint64Cond(this->cond_idx_uint64, this->cond_vec_uint64, unique_name);
    auto it = this->cond_idx_xdata.find(unique_name);
    if(it == this->cond_idx_xdata.end()){
        CondXDataEntry entry;
        entry.name = unique_name;
        entry.pin = pin;
        entry.val = val;
        entry.cmp = cmp;
        entry.valid = valid;
        entry.valid_value = valid_value;
        entry.func = func;
        entry.arg = arg;
        entry.valid_cmp = ComUseCondCmp::EQ;
        entry.cmp_fn = SelectXDataCmpFn(cmp);
        entry.valid_cmp_fn = SelectXDataCmpFn(entry.valid_cmp);
        this->cond_vec_xdata.push_back(std::move(entry));
        this->cond_idx_xdata[unique_name] = this->cond_vec_xdata.size() - 1;
    }else{
        auto &entry = this->cond_vec_xdata[it->second];
        entry.pin = pin;
        entry.val = val;
        entry.cmp = cmp;
        entry.valid = valid;
        entry.valid_value = valid_value;
        entry.func = func;
        entry.arg = arg;
        entry.cmp_fn = SelectXDataCmpFn(cmp);
        entry.valid_cmp_fn = SelectXDataCmpFn(entry.valid_cmp);
    }
};
void ComUseCondCheck::SetCondition(std::string unique_name, uint64_t pin_ptr, uint64_t val_ptr, ComUseCondCmp cmp, int bytes, uint64_t valid_ptr, uint64_t valid_value_ptr, int valid_bytes, xfunction<bool, uint64_t, uint64_t, uint64_t> func, uint64_t arg){
    RemoveXDataCond(this->cond_idx_xdata, this->cond_vec_xdata, unique_name);
    auto it = this->cond_idx_uint64.find(unique_name);
    if(it == this->cond_idx_uint64.end()){
        CondUint64Entry entry;
        entry.name = unique_name;
        entry.pin_ptr = pin_ptr;
        entry.val_ptr = val_ptr;
        entry.cmp = cmp;
        entry.bytes = bytes;
        entry.valid_ptr = valid_ptr;
        entry.valid_value_ptr = valid_value_ptr;
        entry.valid_bytes = valid_bytes;
        entry.func = func;
        entry.arg = arg;
        entry.valid_cmp = ComUseCondCmp::EQ;
        entry.cmp_fn = SelectPtrCmpFn(cmp);
        entry.valid_cmp_fn = SelectPtrCmpFn(entry.valid_cmp);
        this->cond_vec_uint64.push_back(std::move(entry));
        this->cond_idx_uint64[unique_name] = this->cond_vec_uint64.size() - 1;
    }else{
        auto &entry = this->cond_vec_uint64[it->second];
        entry.pin_ptr = pin_ptr;
        entry.val_ptr = val_ptr;
        entry.cmp = cmp;
        entry.bytes = bytes;
        entry.valid_ptr = valid_ptr;
        entry.valid_value_ptr = valid_value_ptr;
        entry.valid_bytes = valid_bytes;
        entry.func = func;
        entry.arg = arg;
        entry.cmp_fn = SelectPtrCmpFn(cmp);
        entry.valid_cmp_fn = SelectPtrCmpFn(entry.valid_cmp);
    }
};
void ComUseCondCheck::RemoveCondition(std::string unique_name){
    RemoveXDataCond(this->cond_idx_xdata, this->cond_vec_xdata, unique_name);
    RemoveUint64Cond(this->cond_idx_uint64, this->cond_vec_uint64, unique_name);
};
std::vector<std::string> ComUseCondCheck::GetTriggeredConditionKeys(){
    std::vector<std::string> ret;
    for(auto &e : this->cond_vec_xdata){
        if(e.triggered)ret.push_back(e.name);
    }
    for(auto &e : this->cond_vec_uint64){
        if(e.triggered)ret.push_back(e.name);
    }
    return ret;
}
std::map<std::string, bool> ComUseCondCheck::ListCondition(){
    std::map<std::string, bool> ret;
    for(auto &e : this->cond_vec_xdata){
        ret[e.name] = e.triggered ? true : false;
    }
    for(auto &e : this->cond_vec_uint64){
        ret[e.name] = e.triggered ? true : false;
    }
    return ret;
}
ComUseCondCmp ComUseCondCheck::GetValidCmpMode(std::string unique_name){
    auto it = this->cond_idx_xdata.find(unique_name);
    if(it != this->cond_idx_xdata.end()){
        return this->cond_vec_xdata[it->second].valid_cmp;
    }
    auto it2 = this->cond_idx_uint64.find(unique_name);
    if(it2 != this->cond_idx_uint64.end()){
        return this->cond_vec_uint64[it2->second].valid_cmp;
    }
    Error("Condition not found: %s", unique_name.c_str());
    return ComUseCondCmp::EQ; // default
}
void ComUseCondCheck::SetValidCmpMode(std::string unique_name, ComUseCondCmp cmp){
    auto it = this->cond_idx_xdata.find(unique_name);
    if(it != this->cond_idx_xdata.end()){
        auto &entry = this->cond_vec_xdata[it->second];
        entry.valid_cmp = cmp;
        entry.valid_cmp_fn = SelectXDataCmpFn(cmp);
        return;
    }
    auto it2 = this->cond_idx_uint64.find(unique_name);
    if(it2 != this->cond_idx_uint64.end()){
        auto &entry = this->cond_vec_uint64[it2->second];
        entry.valid_cmp = cmp;
        entry.valid_cmp_fn = SelectPtrCmpFn(cmp);
        return;
    }
    Error("Condition not found: %s", unique_name.c_str());
}
void ComUseCondCheck::ClearClock(){this->clk_list.clear();}
void ComUseCondCheck::ClearCondition(){
    this->cond_idx_xdata.clear();
    this->cond_idx_uint64.clear();
    this->cond_vec_xdata.clear();
    this->cond_vec_uint64.clear();
};
int  ComUseCondCheck::_valcmp(uint64_t a, u_int64_t b, int bytes){
    uint8_t *a_ptr = (uint8_t*)a;
    uint8_t *b_ptr = (uint8_t*)b;
    uint8_t a_sig = a_ptr[bytes-1] & 0x80;
    uint8_t b_sig = b_ptr[bytes-1] & 0x80;
    if (a_sig != b_sig)return a_sig ? -1: 1;
    for(int i=bytes-1; i>=0; i--){
        if(a_ptr[i] != b_ptr[i]){
            if(a_sig){
                return (a_ptr[i] < b_ptr[i]) ? 1 : -1;
            }
            return (a_ptr[i] < b_ptr[i]) ? -1 : 1;
        }
    }
    return 0;
}
xfunction<bool, XData*, XData*, uint64_t> ComUseCondCheck::AsXDataXFunc(uint64_t func){
    xfunction<bool, XData*, XData*, uint64_t> ret = (bool (*)(XData*, XData*, uint64_t))func;
    return ret;
}
xfunction<bool, uint64_t, uint64_t, uint64_t> ComUseCondCheck::AsPtrXFunc(uint64_t func){
    xfunction<bool, uint64_t, uint64_t, uint64_t> ret = (bool (*)(uint64_t, uint64_t, uint64_t))func;
    return ret;
}
void ComUseCondCheck::Call(){
    if (likely(this->cond_vec_xdata.empty() && this->cond_vec_uint64.empty())) {
        return;
    }
    // check XData condition
    bool is_triggered = false;
    for(auto &entry : this->cond_vec_xdata){
        auto pin = entry.pin;
        auto val = entry.val;
        auto valid = entry.valid;
        auto valid_value = entry.valid_value;
        auto func = entry.func;
        auto arg = entry.arg;
        auto cmp_fn = entry.cmp_fn;
        auto valid_cmp_fn = entry.valid_cmp_fn;
        // not triggered
        entry.triggered = 0;
        if (unlikely(valid != nullptr)){
            Assert(valid_value != nullptr, "valid_value is null");
            if(unlikely(!valid_cmp_fn || !valid_cmp_fn(valid, valid_value))) continue;
        }
        // check cmp
        if(unlikely(func)){
            if(func(pin, val, arg)){
                entry.triggered = 1;
            }
        }else{
            if(unlikely(!cmp_fn)){
                Assert(0, "cmp xdata type error");
            }else if(cmp_fn(pin, val)){
                entry.triggered = 1;
            }
        }
        if(unlikely(!is_triggered && entry.triggered)){
                for(auto &clk : this->clk_list){
                    clk->Disable();
                }
                is_triggered = true;
                this->IncCbCount();
        }
    }
    // check uint64_t condition
    for(auto &entry : this->cond_vec_uint64){
        auto pin_ptr = entry.pin_ptr;
        auto val_ptr = entry.val_ptr;
        auto bytes = entry.bytes;
        auto valid_ptr = entry.valid_ptr;
        auto valid_value_ptr = entry.valid_value_ptr;
        auto valid_bytes = entry.valid_bytes;
        auto func = entry.func;
        auto arg = entry.arg;
        auto cmp_fn = entry.cmp_fn;
        auto valid_cmp_fn = entry.valid_cmp_fn;
        // not triggered
        entry.triggered = 0;
        if (unlikely(valid_ptr != 0)){
            Assert(valid_value_ptr != 0, "valid_value is null");
            if(unlikely(!valid_cmp_fn || !valid_cmp_fn(this, valid_ptr, valid_value_ptr, valid_bytes))) continue;
        }
        // check cmp
        if(unlikely(func)){
            if(func(pin_ptr, val_ptr, arg)){
                entry.triggered = 1;
            }
        }else{
            if(unlikely(!cmp_fn)){
                Assert(0, "cmp ptr type error");
            }else if(cmp_fn(this, pin_ptr, val_ptr, bytes)){
                entry.triggered = 1;
            }
        }
        if(unlikely(!is_triggered && entry.triggered)){
                for(auto &clk : this->clk_list){
                    clk->Disable();
                }
                is_triggered = true;
                this->IncCbCount();
        }
    }
};
ComUseCondCheck::XDataCmpFn ComUseCondCheck::SelectXDataCmpFn(ComUseCondCmp cmp){
    switch (cmp) {
    case ComUseCondCmp::EQ: return &ComUseCondCheck::XDataCmpEq;
    case ComUseCondCmp::NE: return &ComUseCondCheck::XDataCmpNe;
    case ComUseCondCmp::GT: return &ComUseCondCheck::XDataCmpGt;
    case ComUseCondCmp::GE: return &ComUseCondCheck::XDataCmpGe;
    case ComUseCondCmp::LT: return &ComUseCondCheck::XDataCmpLt;
    case ComUseCondCmp::LE: return &ComUseCondCheck::XDataCmpLe;
    default: return nullptr;
    }
}

ComUseCondCheck::PtrCmpFn ComUseCondCheck::SelectPtrCmpFn(ComUseCondCmp cmp){
    switch (cmp) {
    case ComUseCondCmp::EQ: return &ComUseCondCheck::PtrCmpEq;
    case ComUseCondCmp::NE: return &ComUseCondCheck::PtrCmpNe;
    case ComUseCondCmp::GT: return &ComUseCondCheck::PtrCmpGt;
    case ComUseCondCmp::GE: return &ComUseCondCheck::PtrCmpGe;
    case ComUseCondCmp::LT: return &ComUseCondCheck::PtrCmpLt;
    case ComUseCondCmp::LE: return &ComUseCondCheck::PtrCmpLe;
    default: return nullptr;
    }
}

bool ComUseCondCheck::RemoveXDataCond(std::unordered_map<std::string, size_t> &idx,
                                      std::vector<CondXDataEntry> &vec,
                                      const std::string &name){
    auto it = idx.find(name);
    if(it == idx.end()) return false;
    size_t at = it->second;
    size_t last = vec.size() - 1;
    if(at != last){
        vec[at] = std::move(vec[last]);
        idx[vec[at].name] = at;
    }
    vec.pop_back();
    idx.erase(it);
    return true;
}

bool ComUseCondCheck::RemoveUint64Cond(std::unordered_map<std::string, size_t> &idx,
                                       std::vector<CondUint64Entry> &vec,
                                       const std::string &name){
    auto it = idx.find(name);
    if(it == idx.end()) return false;
    size_t at = it->second;
    size_t last = vec.size() - 1;
    if(at != last){
        vec[at] = std::move(vec[last]);
        idx[vec[at].name] = at;
    }
    vec.pop_back();
    idx.erase(it);
    return true;
}

} // namespace xspcomm
