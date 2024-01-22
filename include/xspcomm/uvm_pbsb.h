#ifndef __xspcomm_uvm_pbsb__
#define __xspcomm_uvm_pbsb__

#include "xspcomm/uvm_msg.h"
#include "xspcomm/_uvmc_pbsb.h"
#include <functional>

namespace xspcomm {

class UVMSub : public UVMCSub
{
    std::function<void(const std::vector<uint8_t> &)> handler = nullptr;
public:
    UVMSub(std::string channel) : UVMCSubBase(channel) {}
    ~UVMCSub() {}
    void SetHandler(std::function<void(const uvm_msg &)> handler);
    virtual void Handler(const uvm_msg &data) override;
    virtual void Connect() override;
    std::string GetChannel();
};

class UVMPub: public UVMCPub
{
public:
    UVMPub(std::string channel): UVMCPub(channel) {}
    ~UVMPub() {}
    virtual void SendMsg(uvm_msg &msg) override;
    virtual void Connect() override;
    std::string GetChannel();
};

void uvm_pbsub_run(uint64_t time);

} // namespace xspcomm

#endif
