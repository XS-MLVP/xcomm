#ifndef __xspcomm_tlm_msg__
#define __xspcomm_tlm_msg__

#include <cstdint>
#include <vector>

namespace xspcomm {
class tlm_msg
{
public:
    tlm_msg(){}
    tlm_msg(uint8_t*addr_start, uint8_t* addr_end):data(addr_start, addr_end){}
    tlm_msg(std::vector<uint8_t> &data):data(data){}
    void from_bytes(std::vector<uint8_t> &data) {this->data = data;}
    std::vector<uint8_t> as_bytes() const {return this->data;}
    uint8_t cmd;
    uint8_t resp_status;
    std::vector<uint8_t> data;
};

} // namespace xspcomm

#endif
