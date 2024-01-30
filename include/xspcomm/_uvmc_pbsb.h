#ifndef __xspcomm___uvmc_pbsb__
#define __xspcomm___uvmc_pbsb__

#include <tlm.h>
#include <systemc.h>
#include "uvmc.h"
#include "tlm_msg.h"
#include <cstdint>
#include <string>
#include "xspcomm/xcallback.h"

#include "simple_target_socket.h"
#include "simple_initiator_socket.h"

namespace xspcomm {

class UVMCSub: public sc_core::sc_module
{
    tlm_utils::simple_target_socket<UVMCSub> in;
    tlm::tlm_analysis_port<tlm::tlm_generic_payload> ap;
    xfunction<void, const tlm_msg&> handler = nullptr;
public:
    std::string channel;
    UVMCSub(std::string channel) : channel(channel), in("in"), ap("ap"), sc_core::sc_module(channel)
    {
        this->in.register_b_transport(this, &UVMCSub::b_transport);
    }
    ~UVMCSub() {}
    void SetHandler(xfunction<void, const tlm_msg&> cb){
        this->handler = cb;
    }
    virtual void b_transport(tlm::tlm_generic_payload &gp, sc_core::sc_time &t)
    {
        auto address = gp.get_data_ptr();
        std::vector<uint8_t> data(address, address + gp.get_data_length());
        tlm_msg msg(address, address + gp.get_data_length());
        msg.cmd         = (tlm_command)gp.get_command();
        msg.resp_status = (tlm_response_status)gp.get_response_status();
        msg.option      = (tlm_gp_option)gp.get_gp_option();
        this->Handler(msg);
        sc_core::wait(t);
        ap.write(gp);
    }
    virtual void Handler(const tlm_msg &msg)
    {
        if(this->handler != nullptr){
            this->handler(msg);
        }else{
            printf("[warn] raw UVMCSub::handler called, with datasize: %ld\n",
               msg.data.size());
        }
    }
    virtual void Connect(){
        uvmc::uvmc_connect(this->in, this->channel.c_str());
    }
};

class UVMCPub: public sc_core::sc_module
{
    tlm_utils::simple_initiator_socket<UVMCPub> out;
    tlm::tlm_analysis_port<tlm::tlm_generic_payload> ap;
    std::vector<tlm_msg*> data_to_send;
    bool exit = false;
    sc_core::sc_event not_empty;

public:
    std::string channel;
    UVMCPub(std::string channel) :
        channel(channel), out("out"), ap("ap"), sc_core::sc_module(channel)
    {
        SC_THREAD(__run__);
    }
    SC_HAS_PROCESS(UVMCPub);
    virtual void Exit()
    {
        this->exit = true;
        this->not_empty.notify();
    }
    void __run__()
    {
        while (!this->exit) {
            if (this->data_to_send.empty()) {
                sc_core::wait(this->not_empty);
                continue;
            }
            auto data = this->data_to_send.back();
            this->data_to_send.pop_back();
            tlm::tlm_command cmd                 = (tlm::tlm_command)data->cmd;
            tlm::tlm_response_status resp_status = (tlm::tlm_response_status)data->resp_status;
            tlm::tlm_gp_option option            = (tlm::tlm_gp_option)data->option;
            auto vdata                           = data->data;
            tlm::tlm_generic_payload gp;
            gp.set_data_length(vdata.size());
            gp.set_data_ptr(vdata.data());
            gp.set_command(cmd);
            gp.set_response_status(resp_status);
            gp.set_gp_option(option);
            auto dtime = sc_core::sc_time(0, sc_core::SC_NS);
            out->b_transport(gp, dtime);
            ap.write(gp);
        }
    }
    virtual void SendMsg(tlm_msg &msg)
    {
        this->data_to_send.push_back(&msg);
        this->not_empty.notify();
    }

    virtual void Connect(){
        uvmc::uvmc_connect(this->out, this->channel.c_str());
    }
};

void inline sc_run(double time)
{
    sc_core::sc_start(time, sc_core::SC_NS);
}

} // namespace xspcomm

#endif
