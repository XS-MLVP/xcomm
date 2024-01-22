#ifndef __xspcomm___uvmc_pbsb__
#define __xspcomm___uvmc_pbsb__

#include <tlm.h>
#include <systemc.h>

#include "uvm_msg.h"
#include <cstdint>
#include <string>
#include <functional>

#include "simple_target_socket.h"


namespace xspcomm {

class UVMCSub
{
    tlm_utils::simple_target_socket<UVMCSub> in;
    tlm::tlm_analysis_port<tlm::tlm_generic_payload> ap;

public:
    std::string channel;
    UVMCSub(std::string channel) : channel(channel), in("in"), ap("ap")
    {
        this->in.register_b_transport(this, &UVMCSub::b_transport);
    }
    ~UVMCSub() {}
    virtual void b_transport(tlm::tlm_generic_payload &gp, sc_core::sc_time &t)
    {
        auto address = gp.get_data_prt();
        std::vector<uint8_t> data(address, address + gp.get_data_length());
        uvm_msg msg(address, address + gp.get_data_length());
        msg.cmd         = gp.get_command();
        msg.resp_status = gp.get_response_status();
        this->Handler(msg);
        sc_core::wait(t);
        ap.write(gp);
    }
    virtual void Handler(const uvm_msg &msg)
    {
        printf("[warn] raw UVMCSub::handler called, with datasize: %ld\n",
               msg.data.size());
    }
    virtual void Connect(){
        umvc_connect(this->in, this->channel.c_str());
    }
};

class UVMCPub
{
    tlm_utils::simple_initiator_socket<UVMCPub> out;
    tlm::tlm_analysis_port<tlm::tlm_generic_payload> ap;
    std::vector<*uvm_msg> data_to_send;
    bool exit = false;
    sc_core::sc_event not_empty;

public:
    std::string channel;
    UVMCPub(std::string channel) :
        channel(channel), out("out"), ap("ap")
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
            tlm::tlm_command cmd                 = data->cmd;
            tlm::tlm_response_status resp_status = data->resp_status;
            auto vdata                           = data->data;
            tlm::tlm_generic_payload gp;
            gp.set_data_length(vdata.size());
            gp.set_data_ptr(vdata.data());
            gp.set_command(cmd);
            gp.set_response_status(resp_status);
            out->b_transport(gp, sc_core::sc_time(0, sc_core::SC_NS));
            ap.write(gp);
            delete data;
        }
    }
    virtual void SendMsg(uvm_msg &msg)
    {
        this->data_to_send.push_back(&msg);
        this->not_empty.notify();
    }

    virtual void Connect(){
        umvc_connect(this->out, this->channel.c_str());
    }
};

void inline sc_run(uint64_t time)
{
    sc_core::sc_start(time, sc_core::SC_NS);
}

} // namespace xspcomm

#endif
