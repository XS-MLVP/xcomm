#include "xspcomm/tlm_pbsb.h"
#include "xspcomm/_uvmc_pbsb.h"

namespace xspcomm {

TLMSub::TLMSub(std::string channel){
    this->ptr_sub = new UVMCSub(channel);
}

TLMSub::~TLMSub(){
    delete (UVMCSub*)this->ptr_sub;
}

void TLMSub::SetHandler(
    xfunction<void, const tlm_msg&> handler)
{
    ((UVMCSub*)this->ptr_sub)->SetHandler(handler);
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
    this->ptr_pub = new UVMCPub(channel);
}

TLMPub::~TLMPub(){
    delete (UVMCPub*)this->ptr_pub;
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
