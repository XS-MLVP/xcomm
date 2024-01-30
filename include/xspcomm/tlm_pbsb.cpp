#include "xspcomm/tlm_pbsb.h"
#include "xspcomm/_uvmc_pbsb.h"
#include <map>
#include <string>

namespace xspcomm {

std::map<std::string, TLMSub*> subs;
std::map<std::string, TLMPub*> pubs;

TLMSub::TLMSub(std::string channel){
    if(!subs.count(channel)){
        subs[channel] = new UVMCSub(channel);
    }
    this->ptr_sub = subs[channel];
}

TLMSub::~TLMSub(){
    auto ptr = ((UVMCSub*))this->ptr_sub;
    ptr.DelHandler(this);
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
    ((UVMCSub*)this->ptr_sub)->channel;
}

TLMPub::TLMPub(std::string channel){
    if(!pubs.count(channel)){
        pubs[channel] = new UVMCPub(channel);
    }
    this->ptr_pub = pubs[channel];
}

TLMPub::~TLMPub(){
    auto ptr = ((UVMCSub*))this->ptr_pub;
    ptr.DelSender(this);
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
    return ((UVMCPub*)this->ptr_pub)->channel;
}

#ifdef USE_VCS
void tlm_pbsb_run(double time)
{
    sc_run(time);
}

void tlm_vcs_init(int argc, char **argv){
    VcsMain(argc, argv);
    VcsInit();
}

void tlm_vcs_step(uint64_t delay){
    VcsSimUntil(&delay);
}
#endif

} // namespace xspcomm
