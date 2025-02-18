#include "xspcomm/xsignal_cfg.h"

namespace xspcomm
{
    void XSignalCFG::load_cfg(){
        if(this->is_inited){
            return;
        }
        this->is_inited = true;
        if(this->cfg_data.empty()){
            this->init_error_msg = "cfg_data is empty (need a file path or yaml string)";
            return;
        }
        fkyaml::node root;
        if (fileExists(this->cfg_data)){
            std::ifstream ifs(this->cfg_data);
            root = fkyaml::node::deserialize(ifs);
            Debug("Init yaml signal from file: %s", this->cfg_data.c_str());
        }else {
            root= fkyaml::node::deserialize(this->cfg_data);
            Debug("Init yaml signal from string");
        }
        if(!root.contains("variables")){
            this->init_error_msg = "key: variables not found";
            return;
        }
        auto vars = root["variables"];
        if(!vars.is_sequence()){
            this->init_error_msg = "variables is not a list";
            return;
        }
        int i = 0;
        for(auto var : vars){
            if(!var.is_mapping()){
                this->init_error_msg = "variables item is not a map";
                return;
            }
            for(auto key: {"name", "offset", "mem_bytes", "rtl_width"}){
                if(!var.contains(key)){
                    this->init_error_msg = "variables item not contains key: " + std::string(key);
                    return;
                }
            }
            if(!var.contains("name")){
                this->init_error_msg = "variables item not contains name";
                return;
            }
            s_xsignal_cfg cfg;
            cfg.offset = var["offset"].get_value<uint64_t>();
            cfg.mem_bytes = var["mem_bytes"].get_value<uint32_t>();
            cfg.rtl_width = var["rtl_width"].get_value<uint32_t>();
            if(var.contains("array_size"))cfg.array_size = var["array_size"].get_value<uint64_t>();
            if(var.contains("type"))cfg.type = var["type"].get_value<std::string>();
            this->cfg_map[var["name"].get_value<std::string>()] = cfg;
            i++;
        }
        Debug("%d signals loaded", i);
    }
    XData* XSignalCFG::new_empty_xdata(std::string name, s_xsignal_cfg &cfg, bool no_return){
        this->load_cfg();
        if(!this->init_error_msg.empty()){
            Error("%s", this->init_error_msg.c_str());
            return nullptr;
        }
        if(!this->cfg_map.count(name)){
            Error("signal name: %s not found", name.c_str());
            return nullptr;
        }
        cfg = this->cfg_map[name];
        if(no_return){
            return (XData *)0x1; // return a fake xdata address
        }
        return new XData(cfg.rtl_width == 1 ? 0: cfg.rtl_width, XData::InOut, name);
    }
    std::vector<std::string> XSignalCFG::GetSiganlNames(std::string pattern){
        this->load_cfg();
        std::vector<std::string> vec;
        if(!this->init_error_msg.empty()){
            Error("%s", this->init_error_msg.c_str());
            return vec;
        }
        if(pattern.empty()){
            for(auto &e : this->cfg_map){
                vec.push_back(e.first);
            }
        }else{
            for(auto &e : this->cfg_map){
                if(e.first.find(pattern) != std::string::npos){
                    vec.push_back(e.first);
                }
            }
        }
        return vec;
    }
    XData* XSignalCFG::NewXData(std::string name){
        s_xsignal_cfg cfg;
        auto xdata = new_empty_xdata(name, cfg);
        if(xdata)xdata->BindNativeData(this->cfg_base_address + cfg.offset);
        return xdata;
    }
    XData* XSignalCFG::NewXData(std::string name, int array_index){
        s_xsignal_cfg cfg;
        auto xdata = new_empty_xdata(name, cfg);
        if(xdata)xdata->BindNativeData(this->cfg_base_address + cfg.offset + cfg.mem_bytes * array_index);
        return xdata;
    }
    std::vector<XData*> XSignalCFG::NewXDataArray(std::string name){
        std::vector<XData*> vec;
        s_xsignal_cfg cfg;
        auto xdata = new_empty_xdata(name, cfg, true);
        if(xdata){
            for(uint64_t i = 0; i < cfg.array_size; i++){
                auto x = new XData(cfg.rtl_width == 1 ? 0: cfg.rtl_width, XData::InOut, name);
                x->BindNativeData(this->cfg_base_address + cfg.offset + cfg.mem_bytes * i);
                vec.push_back(x);
            }
        }
        return vec;
    }
    s_xsignal_cfg XSignalCFG::At(std::string name){
        s_xsignal_cfg cfg;
        new_empty_xdata(name, cfg, true);
        return cfg;
    }
    std::string XSignalCFG::String(){
        this->load_cfg();
        std::string ret = "\nBaseAddress: " + std::to_string(this->cfg_base_address) + "\n";
        for(auto &e : this->cfg_map){
            ret += e.first + ":\n";
            ret += "  offset: " + std::to_string(e.second.offset) + " (address: " + \
                std::to_string(this->cfg_base_address + e.second.offset) + ")\n";
            ret += "  mem_bytes: " + std::to_string(e.second.mem_bytes) + "\n";
            ret += "  rtl_width: " + std::to_string(e.second.rtl_width) + "\n";
            if(e.second.array_size > 0)ret += "  array_size: " + std::to_string(e.second.array_size) + "\n";
            if(!e.second.type.empty())ret += "  type: " + e.second.type + "\n";
        }
        return ret;
    }

} // namespace xspcomm
