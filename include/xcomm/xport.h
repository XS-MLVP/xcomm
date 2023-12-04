#ifndef __xcomm_xport__
#define __xcomm_xport__

#include "xcomm/xdata.h"
#include "xcomm/xutil.h"
#include <map>

namespace xcomm {
class XPort
{
private:
    std::string asKey(std::string name);
    std::map<std::string, std::string> port_name;
public:
    std::string prefix;
    std::map<std::string, xcomm::XData *> port_list;
    XPort(std::string prefix = "");
    int PortCount();
    bool Add(std::string pin, xcomm::XData &pin_data);
    bool Del(std::string pin);
    bool Connect(XPort &target);
    XPort &NewSubPort(std::string subprefix);
    xcomm::XData &operator[](std::string key);
    xcomm::XData &Get(std::string key, bool raw_key = false);
    XPort &Flip();
    XPort &AsBiIO();
    XPort &WriteOnRise();
    XPort &WriteOnFall();
    XPort &ReadFresh(xcomm::WriteMode m);
    XPort &SetZero();
    std::string String(std::string prefix = "");
};

} // namespace xcomm

#endif
