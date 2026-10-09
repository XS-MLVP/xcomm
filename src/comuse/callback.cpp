#include "xspcomm/xcomuse/callback.h"
#include "xspcomm/xclock.h"

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

} // namespace xspcomm
