#ifndef __xspcomm_xcfg__
#define __xspcomm_xcfg__

#include "xspcomm/xdata.h"
#include "xspcomm/node.hpp"
#include "xspcomm/xutil.h"

namespace xspcomm {
typedef struct
{
    uint64_t offset = 0;
    uint32_t mem_bytes = 0;
    uint32_t rtl_width = 0;
    uint64_t array_size = 0;
    bool is_empty = true;
    std::string type = "";
} s_xsignal_cfg, *p_xsignal_cfg;

class XSignalCFG {
    bool is_inited = false;
    std::string init_error_msg = "";
    std::map<std::string, s_xsignal_cfg> cfg_map;
    public:
    std::string cfg_data;
    uint64_t cfg_base_address = 0;
    public:
    /*************************************************************** */
    //                  Start of Stable public user APIs
    /*************************************************************** */
    XSignalCFG(std::string path_or_str_data, uint64_t base_address = 0): cfg_data(path_or_str_data), cfg_base_address(base_address){};
    XData* NewXData(std::string name, std::string xname="");
    XData* NewXData(std::string name, int array_index, std::string xname="");
    std::vector<XData*> NewXDataArray(std::string name, std::string xname="");
    std::vector<std::string> GetSignalNames(std::string patten = "");
    s_xsignal_cfg At(std::string name);
    /*************************************************************** */
    //                  End of Stable public user APIs
    /*************************************************************** */
    std::string String();
    s_xsignal_cfg operator[](std::string name){return this->At(name);}
    private:
    XData* new_empty_xdata(std::string name, std::string xname, s_xsignal_cfg &cfg, bool no_return=false);
    void load_cfg();
};

} // namespace xspcomm

#endif

