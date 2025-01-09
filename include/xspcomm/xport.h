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
    std::map<std::string, std::string> name_port;
public:
    std::string prefix;
    std::map<std::string, xspcomm::XData *> port_list;
    /*************************************************************** */
    //                  Start of Stable public user APIs
    /*************************************************************** */
    XPort(std::string prefix = "");
    int PortCount();
    bool Add(std::string pin, xspcomm::XData &pin_data);
    bool Del(std::string pin);
    bool Connect(XPort &target);
    XPort &NewSubPort(std::string subprefix);
    XPort &SelectPins(std::vector<std::string> pins);
    XPort &SelectPins(std::initializer_list<std::string> pins);
    xspcomm::XData &Get(std::string key, bool raw_key = false);
    XPort &FlipIOType();
    XPort &AsBiIO();
    XPort &WriteOnRise();
    XPort &WriteOnFall();
    XPort &ReadFresh(xspcomm::WriteMode m);
    XPort &SetZero();
    std::string String(std::string prefix = "");
    std::string GetPrefix();
    std::vector<std::string> GetKeys(bool raw_key = false);
    XPort &AsImmWrite();
    XPort &AsRiseWrite();
    XPort &AsFallWrite();
    uint64_t CSelf(){return (uint64_t)this;}
    /*************************************************************** */
    //                  End of Stable public user APIs
    /*************************************************************** */
    xspcomm::XData &operator[](std::string key);
    XPort &operator=(XPort &data);
};

} // namespace xspcomm

#endif
