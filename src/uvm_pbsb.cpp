#include "xspcomm/uvm_pubsb.h"

namespace xspcomm {

void UVMSub::SetHandler(
    std::function<void(const uvm_msg&)> handler)
{
    this->handler = handler;
}

virtual void UVMSub::Handler(const uvm_msg &data) override
{
    if (this->handler) {
        this->handler(data);
    } else {
        printf("[warn] called with datasize: %ld to empty handler\n",
               data.size());
    }
}

virtual void UVMSub::Connect() override
{
    return UVMCPub.Connect();
}

std::string UVMSub::GetChannel()
{
    return this->channel;
}

virtual void UVMPub::SendMsg(uvm_msg &msg) override
{
    return UVMCPub.SendMsg(msg);
}

virtual void UVMPub::Connect() override
{
    return UVMCPub.Connect();
}

std::string UVMPub::GetChannel()
{
    return this->channel;
}

void uvm_pbsub_run(uint64_t time)
{
    sc_run(time);
}

} // namespace xspcomm
