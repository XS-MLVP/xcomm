#ifndef __xspcomm_xcfg__
#define __xspcomm_xcfg__

#include "xspcomm/xdata.h"
#include "xspcomm/node.hpp"
#include "xspcomm/xutil.h"
#include <set>

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

typedef struct
{
    std::string kind = "native";
    std::string type = "";
    uint32_t rtl_width = 0;
    std::string source = "";
    std::string expr = "";
    std::string value = "";
    uint64_t const_value = 0;
    bool bindable = false;
    bool is_const = false;
    std::vector<std::string> deps;
} s_xsignal_meta, *p_xsignal_meta;

class XSignalCFG {
    bool is_inited = false;
    std::string init_error_msg = "";
    std::map<std::string, s_xsignal_cfg> cfg_map;
    std::map<std::string, s_xsignal_meta> signal_meta_map;
    std::set<std::string> constructing_signals;
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
    std::vector<std::shared_ptr<XData>> NewXDataArray(std::string name, std::string xname="");
    std::vector<std::string> GetSignalNames(std::string patten = "");
    s_xsignal_cfg At(std::string name);
    uint64_t Address(std::string name);
    /*************************************************************** */
    //                  End of Stable public user APIs
    /*************************************************************** */
    std::string String();
    s_xsignal_cfg operator[](std::string name){return this->At(name);}
    private:
    XData* new_empty_xdata(std::string name, std::string xname, s_xsignal_cfg &cfg, bool no_return=false);
    void load_cfg();
    bool _set_cfg_data(fkyaml::node &var, std::string prefix="");
    bool _set_signal_meta(fkyaml::node &signal);
    int _rec_set_cfg_data(fkyaml::node &var, std::string prefix);
    void _register_native_meta(const std::string &name, const s_xsignal_cfg &cfg);
    std::string _normalize_expr(std::string expr) const;
    uint64_t _parse_const_value(const std::string &value) const;
};

} // namespace xspcomm

#endif
