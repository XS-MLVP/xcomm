
#include "xspcomm/xcomuse.h"


namespace xspcomm {

u_int64_t ComUseStepCb::GetCb(){
    return (u_int64_t)ComUseStepCb::Cb;
}

void ComUseStepCb::Cb(uint64_t c, void *self){
    ComUseStepCb *p = (ComUseStepCb*)self;
    p->cycle = c;
    p->Call();
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

uint64_t U64PtrAsU64(unsigned long long *p){return (uint64_t)p;}
uint64_t U32PtrAsU64(unsigned int *p){return (uint64_t)p;}
uint64_t  U8PtrAsU64(unsigned char *p){return (uint64_t)p;}

} // namespace xspcomm