#include "xspcomm/tlm_pbsb.h"
#include "xspcomm/_uvmc_pbsb.h"
#include <map>
#include <string>

namespace xspcomm {

std::map<std::string, UVMCSub*> subs;
std::map<std::string, UVMCPub*> pubs;

TLMSub::TLMSub(std::string channel){
    auto ch = this->MakeChannel(channel);
    if(!subs.count(ch)){
        subs[ch] = new UVMCSub(ch);
    }
    this->ptr_sub = subs[ch];
}

TLMSub::~TLMSub(){
    UVMCSub* ptr = (UVMCSub*)this->ptr_sub;
    ptr->DelHandler(this);
    if(ptr->IsHandlerEmpty()){
        subs.erase(ptr->channel);
        delete ptr;
    }
}

void TLMSub::SetHandler(
    xfunction<void, const tlm_msg&> handler)
{
    ((UVMCSub*)this->ptr_sub)->SetHandler(this, handler);
}

void TLMSub::Connect()
{
    ((UVMCSub*)this->ptr_sub)->Connect();
}

std::string TLMSub::GetChannel()
{
    return this->UMakeChannel(((UVMCSub*)this->ptr_sub)->channel);
}

TLMPub::TLMPub(std::string channel){
    auto ch = this->MakeChannel(channel);
    if(!pubs.count(ch)){
        pubs[ch] = new UVMCPub(ch);
    }
    this->ptr_pub = pubs[ch];
}

TLMPub::~TLMPub(){
    UVMCPub* ptr = (UVMCPub*)this->ptr_pub;
    ptr->DelSender(this);
    if(ptr->IsSenderEmpty()){
        pubs.erase(ptr->channel);
        delete ptr;
    }
}

void TLMPub::SendMsg(tlm_msg &msg)
{
    ((UVMCPub*)this->ptr_pub)->SendMsg(msg);
}

void TLMPub::Connect()
{
    ((UVMCPub*)this->ptr_pub)->Connect();
}

std::string TLMPub::GetChannel()
{
    return this->UMakeChannel(((UVMCPub*)this->ptr_pub)->channel);
}

void step(double time, double scale, std::string unit){
    sc_run(time, unit);
#ifdef USE_VCS
    tlm_vcs_step((uint64_t)(time * scale));
#endif
}

#ifdef USE_VCS
void tlm_pbsb_run(double time, std::string unit)
{
    sc_run(time, unit);
}

void tlm_vcs_init(int argc, char **argv){
    VcsMain(argc, argv);
    VcsInit();
}

uint64_t _vcs_time = 0;
void tlm_vcs_step(uint64_t delay){
    _vcs_time += delay;
    VcsSimUntil(&_vcs_time);
}
#endif

} // namespace xspcomm
