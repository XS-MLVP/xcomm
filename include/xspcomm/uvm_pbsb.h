#ifndef __xspcomm_uvm_pbsb__
#define __xspcomm_uvm_pbsb__

#include "xspcomm/uvm_msg.h"
#include <functional>

namespace xspcomm {

class UVMSub
{
    void *ptr_sub = nullptr;
public:
    UVMSub(std::string channel);
    ~UVMSub();
    void SetHandler(std::function<void(const uvm_msg &)> handler);
    void Connect();
    std::string GetChannel();
};

class UVMPub
{
    void *ptr_pub = nullptr;
public:
    UVMPub(std::string channel);
    ~UVMPub();
    void SendMsg(uvm_msg &msg);
    void Connect();
    std::string GetChannel();
};

void uvm_pbsub_run(double time);

} // namespace xspcomm

#endif
