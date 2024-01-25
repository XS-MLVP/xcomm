#ifndef __xspcomm_uvm_msg__
#define __xspcomm_uvm_msg__

#include <cstdint>
#include <vector>

namespace xspcomm {
class uvm_msg
{
public:
    uvm_msg(){}
    uvm_msg(uint8_t*addr_start, uint8_t* addr_end):data(addr_start, addr_end){}
    uvm_msg(std::vector<uint8_t> &data):data(data){}
    void from_bytes(std::vector<uint8_t> &data) {this->data = data;}
    std::vector<uint8_t> as_bytes() const {return this->data;}
    uint8_t cmd;
    uint8_t resp_status;
    std::vector<uint8_t> data;
};

} // namespace xspcomm

#endif
