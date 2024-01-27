#ifndef __xspcomm_tlm_msg__
#define __xspcomm_tlm_msg__

#include <cstdint>
#include <vector>

namespace xspcomm {
class tlm_msg
{
public:
    // For c++
    uint8_t cmd;
    uint8_t resp_status;
    std::vector<uint8_t> data;
    tlm_msg(){}
    tlm_msg(uint8_t*addr_start, uint8_t* addr_end):data(addr_start, addr_end){}
    // For swig-python
    tlm_msg(std::vector<unsigned char> &data):data(data){}
    void from_bytes(std::vector<unsigned char> &data) {this->data = data;}
    std::vector<unsigned char> as_bytes() {return this->data;}
};

} // namespace xspcomm

#endif
