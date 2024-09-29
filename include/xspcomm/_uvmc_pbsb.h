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
#include <map>

namespace xspcomm {

class UVMCSub: public sc_core::sc_module
{
    tlm_utils::simple_target_socket<UVMCSub> in;
    tlm::tlm_analysis_port<tlm::tlm_generic_payload> ap;
    std::map<void*, xfunction<void, const tlm_msg&>> handler;
    bool is_connected = false;
public:
    std::string channel;
    UVMCSub(std::string channel) : channel(channel), in("in"), ap("ap"), sc_core::sc_module(channel)
    {
        this->in.register_b_transport(this, &UVMCSub::b_transport);
    }
    ~UVMCSub() {}
    void SetHandler(void* key, xfunction<void, const tlm_msg&> cb){
        this->handler[key] = cb;
    }
    void DelHandler(void* key){
        if(this->handler.count(key)){
            this->handler.erase(key);
        }
    }
    bool IsHandlerEmpty(){
        return this->handler.empty();
    }
    virtual void b_transport(tlm::tlm_generic_payload &gp, sc_core::sc_time &t)
    {
        auto address = gp.get_data_ptr();
        std::vector<uint8_t> data(address, address + gp.get_data_length());
        tlm_msg msg(address, address + gp.get_data_length());
        msg.cmd         = (tlm_command)gp.get_command();
        msg.resp_status = (tlm_response_status)gp.get_response_status();
        msg.option      = (tlm_gp_option)gp.get_gp_option();
        sc_core::wait(t);
        ap.write(gp);
        this->Handler(msg);
    }
    virtual void Handler(const tlm_msg &msg)
    {
        if(!this->handler.empty()){
            for(auto &kv: this->handler){
                kv.second(msg);
            }
        }else{
            printf("[warn] raw UVMCSub::handler called, with datasize: %ld\n",
               msg.data.size());
        }
    }
    virtual void Connect(){
        if(!this->is_connected){
            uvmc::uvmc_connect(this->in, this->channel.c_str());
            this->is_connected = true;
        }
    }
};


sc_core::sc_time_unit inline str2timeunit(std::string unit, std::string where){
    sc_core::sc_time_unit u = sc_core::SC_NS;
    if(unit == "fs"){
        u = sc_core::SC_FS;
    }else if(unit == "ps"){
        u = sc_core::SC_PS;
    }else if(unit == "ns"){
        u = sc_core::SC_NS;
    }else if(unit == "us"){
        u = sc_core::SC_US;
    }else if(unit == "ms"){
        u = sc_core::SC_MS;
    }else if(unit == "s"){
        u = sc_core::SC_SEC;
    }else{
        printf("[warn] %s find unknown time unit: %s, use SC_NS as default\n", where.c_str(), unit.c_str());
    }
    return u;
}


class UVMCPub: public sc_core::sc_module
{
    tlm_utils::simple_initiator_socket<UVMCPub> out;
    tlm::tlm_analysis_port<tlm::tlm_generic_payload> ap;
    std::vector<tlm_msg*> data_to_send;
    bool exit = false;
    sc_core::sc_event not_empty;
    bool is_connected = false;
    std::map<void*, void*> sender;
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
    void SetSender(void* key){
        this->sender[key] = this;
    }
    void DelSender(void* key){
        if(this->sender.count(key)){
            this->sender.erase(key);
        }
    }
    bool IsSenderEmpty(){
        return this->sender.empty();
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
            auto dtime = sc_core::sc_time(data->delay, str2timeunit(data->time_unit, "UVMCPub::__run__ loop"));
            out->b_transport(gp, dtime);
            ap.write(gp);
            delete data;
        }
    }
    virtual void SendMsg(tlm_msg &msg)
    {
        auto m = msg.clone();
        this->data_to_send.push_back(m);
        this->not_empty.notify();
    }

    virtual void Connect(){
        if(!this->is_connected){
            uvmc::uvmc_connect(this->out, this->channel.c_str());
            this->is_connected = true;
        }
    }
};

void inline sc_run(double time, std::string unit = "ns")
{
    sc_core::sc_start(time, str2timeunit(unit, "sc_run"));
}

} // namespace xspcomm

#endif
