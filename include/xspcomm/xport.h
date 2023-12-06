#ifndef __xspcomm_xport__
#define __xspcomm_xport__

#include "xspcomm/xdata.h"
#include "xspcomm/xutil.h"
#include <map>

namespace xspcomm {
class XPort
{
private:
    std::string asKey(std::string name);
    std::map<std::string, std::string> port_name;
public:
    std::string prefix;
    std::map<std::string, xspcomm::XData *> port_list;
    XPort(std::string prefix = "");
    int PortCount();
    bool Add(std::string pin, xspcomm::XData &pin_data);
    bool Del(std::string pin);
    bool Connect(XPort &target);
    XPort &NewSubPort(std::string subprefix);
    xspcomm::XData &operator[](std::string key);
    xspcomm::XData &Get(std::string key, bool raw_key = false);
    XPort &Flip();
    XPort &AsBiIO();
    XPort &WriteOnRise();
    XPort &WriteOnFall();
    XPort &ReadFresh(xspcomm::WriteMode m);
    XPort &SetZero();
    std::string String(std::string prefix = "");
};

} // namespace xspcomm

#endif
