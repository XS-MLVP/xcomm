#ifndef __xspcomm_tlm_msg__
#define __xspcomm_tlm_msg__

#include <cstdint>
#include <vector>

namespace xspcomm {

enum tlm_command { TLM_READ_COMMAND, TLM_WRITE_COMMAND, TLM_IGNORE_COMMAND };

enum tlm_response_status {
    TLM_OK_RESPONSE                = 1,
    TLM_INCOMPLETE_RESPONSE        = 0,
    TLM_GENERIC_ERROR_RESPONSE     = -1,
    TLM_ADDRESS_ERROR_RESPONSE     = -2,
    TLM_COMMAND_ERROR_RESPONSE     = -3,
    TLM_BURST_ERROR_RESPONSE       = -4,
    TLM_BYTE_ENABLE_ERROR_RESPONSE = -5
};

enum tlm_gp_option {
    TLM_MIN_PAYLOAD,
    TLM_FULL_PAYLOAD,
    TLM_FULL_PAYLOAD_ACCEPTED
};

class tlm_msg
{
public:
    // For c++
    tlm_command cmd;
    tlm_response_status resp_status;
    tlm_gp_option option;
    std::vector<uint8_t> data;
    tlm_msg() {
        this->cmd = TLM_IGNORE_COMMAND;
        this->resp_status = TLM_OK_RESPONSE;
        this->option = TLM_MIN_PAYLOAD;
    }
    tlm_msg(const tlm_msg &msg) {
        this->cmd = msg.cmd;
        this->resp_status = msg.resp_status;
        this->option = msg.option;
        this->data.assign(msg.data.begin(), msg.data.end());
    }
    tlm_msg(uint8_t *addr_start, uint8_t *addr_end)
    {
        std::vector<uint8_t> tmp(addr_start, addr_end);
        this->data.assign(tmp.begin(), tmp.end());
        tlm_msg();
    }
    // For swig-python
    tlm_msg(std::vector<unsigned char> &data) : data(data) {
        tlm_msg();
    }
    void from_bytes(std::vector<unsigned char> &data) { this->data = data; }
    std::vector<unsigned char> as_bytes() { return this->data; }
    tlm_msg *clone() { return new tlm_msg(*this); }
};

} // namespace xspcomm

#endif
