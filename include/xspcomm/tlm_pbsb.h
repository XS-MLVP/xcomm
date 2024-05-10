#ifndef __xspcomm_tlm_pbsb__
#define __xspcomm_tlm_pbsb__

#include "xspcomm/tlm_msg.h"
#include "xspcomm/xcallback.h"
#include <functional>

#ifdef USE_VCS
extern "C" {
int VcsMain(int argc, char **argv);
void VcsInit();
void VcsSimUntil(uint64_t *);
}
#endif

namespace xspcomm {

class TLMSub
{
    void *ptr_sub = nullptr;
public:
    TLMSub(std::string channel);
    ~TLMSub();
    std::string MakeChannel(std::string channel){
        return channel + ".sub";
    }
    std::string UMakeChannel(std::string channel){
        return removeSuffix(channel, ".sub");
    }
    void SetHandler(xfunction<void, const tlm_msg &> handler);
    void Connect();
    std::string GetChannel();
};

class TLMPub
{
    void *ptr_pub = nullptr;
public:
    TLMPub(std::string channel);
    ~TLMPub();
    std::string MakeChannel(std::string channel){
        return channel + ".pub";
    }
    std::string UMakeChannel(std::string channel){
        return removeSuffix(channel, ".pub");
    }
    void SendMsg(tlm_msg &msg);
    void Connect();
    std::string GetChannel();
};

void tlm_pbsb_run(double time);

void step(double time, double scale = 1.0f);

#ifdef USE_VCS
void tlm_vcs_init(int argc, char **argv);
void tlm_vcs_step(uint64_t delay);
#endif

} // namespace xspcomm

#endif
