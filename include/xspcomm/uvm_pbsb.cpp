#include "xspcomm/uvm_pbsb.h"
#include "xspcomm/_uvmc_pbsb.h"

namespace xspcomm {

UVMSub::UVMSub(std::string channel){
    this->ptr_sub = new UVMCSub(channel);
}

UVMSub::~UVMSub(){
    delete (UVMCSub*)this->ptr_sub;
}

void UVMSub::SetHandler(
    Xfunction<void, const uvm_msg&> handler)
{
    ((UVMCSub*)this->ptr_sub)->SetHandler(handler);
}

void UVMSub::Connect()
{
    ((UVMCSub*)this->ptr_sub)->Connect();
}

std::string UVMSub::GetChannel()
{
    ((UVMCSub*)this->ptr_sub)->channel;
}

UVMPub::UVMPub(std::string channel){
    this->ptr_pub = new UVMCPub(channel);
}

UVMPub::~UVMPub(){
    delete (UVMCPub*)this->ptr_pub;
}

void UVMPub::SendMsg(uvm_msg &msg)
{
    ((UVMCPub*)this->ptr_pub)->SendMsg(msg);
}

void UVMPub::Connect()
{
    ((UVMCPub*)this->ptr_pub)->Connect();
}

std::string UVMPub::GetChannel()
{
    return ((UVMCPub*)this->ptr_pub)->channel;
}

void uvm_pbsb_run(double time)
{
    sc_run(time);
}

} // namespace xspcomm
