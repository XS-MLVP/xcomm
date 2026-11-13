#include "xspcomm/xsignal_cfg.h"
#include "xspcomm/xexpr.h"
#include "xspcomm/detail/array_binding.h"
#include "fkyaml/node.hpp"
#include <stdexcept>

namespace xspcomm
{
    namespace {

    std::string trim_copy(const std::string &value){
        auto begin = value.find_first_not_of(" \t\r\n");
        if(begin == std::string::npos){
            return "";
        }
        auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    void replace_all(std::string &text, const std::string &from, const std::string &to){
        if(from.empty()){
            return;
        }
        size_t pos = 0;
        while((pos = text.find(from, pos)) != std::string::npos){
            text.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    } // namespace

    class XSignalCFG::Parser {
        XSignalCFG &owner;
    public:
        explicit Parser(XSignalCFG &owner) : owner(owner) {}
        bool _set_cfg_data(fkyaml::node &var, std::string prefix = "");
        bool _set_signal_meta(fkyaml::node &signal);
        int _rec_set_cfg_data(fkyaml::node &var, std::string prefix);
    };

    void XSignalCFG::_register_native_meta(const std::string &name, const s_xsignal_cfg &cfg){
        s_xsignal_meta meta;
        meta.kind = "native";
        meta.type = cfg.type;
        meta.rtl_width = cfg.rtl_width;
        meta.bindable = true;
        this->signal_meta_map[name] = meta;
    }

    std::string XSignalCFG::_normalize_expr(std::string expr) const{
        for (const auto &cast : {"(IData)", "(QData)", "(CData)", "(SData)", "(WData)", "(bool)", "(bool_t)"}) {
            replace_all(expr, cast, "");
        }
        replace_all(expr, "VL_ULL(", "(");
        replace_all(expr, "VL_UL(", "(");
        replace_all(expr, "VL_EDATASIZE(", "(");
        return trim_copy(expr);
    }

    uint64_t XSignalCFG::_parse_const_value(const std::string &value) const{
        auto expr = this->_normalize_expr(value);
        ExprEngine engine;
        return engine.Eval(engine.CompileExpr(expr, nullptr));
    }

    void XSignalCFG::load_cfg(){
        if(this->is_inited){
            return;
        }
        this->is_inited = true;
        if(this->cfg_data.empty()){
            this->init_error_msg = "cfg_data is empty (need a file path or yaml string)";
            return;
        }
        Parser parser(*this);
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
            if(vars.is_mapping()){
                int count = parser._rec_set_cfg_data(vars, "");
                Debug("%d signals loaded (map mode)", count);
            } else {
                this->init_error_msg = "variables is not a list or map";
                return;
            }
        } else {
            int i = 0;
            for(auto var : vars){
                if(!parser._set_cfg_data(var))return;
                i++;
            }
            Debug("%d signals loaded (list mode)", i);
        }

        if(!this->init_error_msg.empty()){
            return;
        }

        for(auto &e : this->cfg_map){
            this->_register_native_meta(e.first, e.second);
        }

        if(root.contains("signals")){
            auto signals = root["signals"];
            if(!signals.is_sequence()){
                this->init_error_msg = "signals is not a list";
                return;
            }
            for(auto signal : signals){
                if(!parser._set_signal_meta(signal)){
                    return;
                }
            }
        }
    }

    bool XSignalCFG::Parser::_set_cfg_data(fkyaml::node &var, std::string prefix){
        if(!var.is_mapping()){
            owner.init_error_msg = "variables item is not a map";
            return false;
        }
        for(auto key: {"offset", "mem_bytes", "rtl_width"}){
            if(!var.contains(key)){
                owner.init_error_msg = "variables item not contains key: " + std::string(key);
                return false;
            }
        }
        std::string cfg_key = prefix;
        if(var.contains("name")){
            cfg_key = prefix + (prefix.empty()? "":".") + var["name"].get_value<std::string>();
        }
        s_xsignal_cfg cfg;
        cfg.is_empty = false;
        cfg.offset = var["offset"].get_value<uint64_t>();
        cfg.mem_bytes = var["mem_bytes"].get_value<uint32_t>();
        cfg.rtl_width = var["rtl_width"].get_value<uint32_t>();
        if(var.contains("array_size"))cfg.array_size = var["array_size"].get_value<uint64_t>();
        if(var.contains("array_dimensions")) {
            for (const auto &dimension : var["array_dimensions"])
                cfg.array_dimensions.push_back(dimension.get_value<int>());
        }
        if(var.contains("array_strides")) {
            for (const auto &stride : var["array_strides"])
                cfg.array_strides.push_back(stride.get_value<uint64_t>());
        }
        if (cfg.array_size && cfg.array_dimensions.empty()) {
            owner.init_error_msg = "array dimensions are required: " + cfg_key;
            return false;
        }
        if (!cfg.array_dimensions.empty()) {
            uint64_t size = 1;
            for (int dimension : cfg.array_dimensions) {
                if (dimension <= 0 || size > UINT64_MAX / uint64_t(dimension)) {
                    owner.init_error_msg = "invalid array dimensions: " + cfg_key;
                    return false;
                }
                size *= uint64_t(dimension);
            }
            if (size != cfg.array_size || cfg.array_strides.size() != cfg.array_dimensions.size()) {
                owner.init_error_msg = "array shape/stride/count mismatch: " + cfg_key;
                return false;
            }
        } else if (!cfg.array_strides.empty()) {
            owner.init_error_msg = "array strides without dimensions: " + cfg_key;
            return false;
        }
        if(var.contains("type"))cfg.type = var["type"].get_value<std::string>();
        Assert(cfg_key.empty() == false, "cfg_key is empty, check prefix and name");
        owner.cfg_map[cfg_key] = cfg;
        return true;
    }

    bool XSignalCFG::Parser::_set_signal_meta(fkyaml::node &signal){
        if(!signal.is_mapping()){
            owner.init_error_msg = "signals item is not a map";
            return false;
        }
        if(!signal.contains("name") || !signal.contains("kind")){
            owner.init_error_msg = "signals item missing name/kind";
            return false;
        }
        auto name = signal["name"].get_value<std::string>();
        s_xsignal_meta meta;
        meta.kind = signal["kind"].get_value<std::string>();
        if(signal.contains("type"))meta.type = signal["type"].get_value<std::string>();
        if(signal.contains("rtl_width"))meta.rtl_width = signal["rtl_width"].get_value<uint32_t>();
        if(signal.contains("source"))meta.source = signal["source"].get_value<std::string>();
        if(signal.contains("expr"))meta.expr = signal["expr"].get_value<std::string>();
        if(signal.contains("value")){
            meta.value = signal["value"].get_value<std::string>();
        }
        if(signal.contains("deps")){
            auto deps = signal["deps"];
            if(!deps.is_sequence()){
                owner.init_error_msg = "signals.deps is not a list";
                return false;
            }
            for(auto dep : deps){
                if(dep.is_mapping()){
                    if(dep.contains("name")){
                        meta.deps.push_back(dep["name"].get_value<std::string>());
                    }
                }else{
                    meta.deps.push_back(dep.get_value<std::string>());
                }
            }
        }

        if(meta.kind == "direct"){
            meta.bindable = true;
            if(!this->_set_cfg_data(signal)){
                return false;
            }
        } else if(meta.kind == "const"){
            meta.is_const = true;
            meta.const_value = owner._parse_const_value(meta.value);
        } else {
            meta.bindable = false;
            if(meta.deps.empty() && !meta.source.empty()){
                meta.deps.push_back(meta.source);
            }
        }

        owner.signal_meta_map[name] = meta;
        return true;
    }

    int XSignalCFG::Parser::_rec_set_cfg_data(fkyaml::node &var, std::string prefix){
        if(!var.is_mapping())return 0;
        int count = 0;
        for (auto& pair : var.map_items()) {
            if(var.contains("offset") && var.contains("mem_bytes") && var.contains("rtl_width")){
                if(var["offset"].is_integer() && var["mem_bytes"].is_integer() && var["rtl_width"].is_integer()){
                    if(this->_set_cfg_data(var, prefix))return 1;
                    Error("set cfg data failed: %s", owner.init_error_msg.c_str());
                    return 0;
                }
            }
            if(pair.value().is_mapping()){
                count += this->_rec_set_cfg_data(pair.value(), prefix + (prefix.empty() ? "":".") + pair.key().get_value<std::string>());
            }
        }
        return count;
    }

    XData* XSignalCFG::new_empty_xdata(std::string name, std::string xname, s_xsignal_cfg &cfg, bool no_return){
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
            return (XData *)0x1;
        }
        if(xname.empty())xname = name;
        return new XData(cfg.rtl_width == 1 ? 0: cfg.rtl_width, XData::InOut, xname);
    }

    std::vector<std::string> XSignalCFG::GetSignalNames(std::string pattern){
        this->load_cfg();
        std::vector<std::string> vec;
        if(!this->init_error_msg.empty()){
            Error("%s", this->init_error_msg.c_str());
            return vec;
        }
        for(auto &e : this->signal_meta_map){
            if(pattern.empty() || e.first.find(pattern) != std::string::npos){
                vec.push_back(e.first);
            }
        }
        return vec;
    }

    XData* XSignalCFG::NewXData(std::string name, std::string xname){
        this->load_cfg();
        if(!this->init_error_msg.empty()){
            Error("%s", this->init_error_msg.c_str());
            return nullptr;
        }
        auto metaIt = this->signal_meta_map.find(name);
        if(metaIt == this->signal_meta_map.end()){
            Error("signal name: %s not found", name.c_str());
            return nullptr;
        }
        if(xname.empty())xname = name;
        auto &meta = metaIt->second;
        if(meta.bindable){
            if (this->cfg_map[name].array_size) return this->NewXData(name, 0, xname);
            s_xsignal_cfg cfg;
            auto xdata = new_empty_xdata(name, xname, cfg);
            if(xdata)xdata->BindNativeData(this->cfg_base_address + cfg.offset);
            return xdata;
        }
        if(meta.is_const){
            auto xdata = new XData(meta.rtl_width == 1 ? 0 : meta.rtl_width, XData::Out, xname);
            xdata->BindConst(meta.const_value);
            return xdata;
        }
        if(this->constructing_signals.count(name)){
            Error("derived signal dependency cycle detected: %s", name.c_str());
            return nullptr;
        }
        this->constructing_signals.insert(name);
        auto cleanup = [this, &name](){ this->constructing_signals.erase(name); };
        try{
            auto engine = std::make_shared<ExprEngine>();
            auto expr = this->_normalize_expr(meta.expr);
            auto root = engine->CompileExpr(expr, this);
            auto xdata = new XData(meta.rtl_width == 1 ? 0 : meta.rtl_width, XData::Out, xname);
            xdata->BindExpr(engine, root);
            cleanup();
            return xdata;
        } catch (const std::exception &e){
            cleanup();
            Error("CompileExpr failed for signal %s: %s", name.c_str(), e.what());
            return nullptr;
        }
    }

    XData* XSignalCFG::NewXData(std::string name, int array_index, std::string xname){
        auto array = NewXArray(name);
        if (!array) return nullptr;
        uint64_t size = 1;
        for (int dimension : array->Shape()) size *= uint64_t(dimension);
        if (array_index < 0 || uint64_t(array_index) >= size)
            throw std::out_of_range("Native array index out of range");
        s_xsignal_cfg cfg;
        auto xdata = new_empty_xdata(name, xname, cfg);
        if(xdata)array->BindElement(*xdata, uint64_t(array_index));
        return xdata;
    }

    std::shared_ptr<XArray> XSignalCFG::NewXArray(std::string name) {
        auto found = native_arrays.find(name);
        if (found != native_arrays.end()) return found->second;
        const auto cfg = At(name);
        if (cfg.is_empty || !cfg.array_size) return nullptr;
        std::vector<int> left(cfg.array_dimensions.size(), 0), right;
        for (int dimension : cfg.array_dimensions) right.push_back(dimension - 1);
        if (cfg.offset > UINT64_MAX - cfg_base_address) throw std::overflow_error("Native array base overflow");
        const auto width = cfg.rtl_width == 1 ? 0 : cfg.rtl_width;
        auto binding = std::make_shared<detail::ArrayBinding>(width, left, right);
        binding->BindNativeData(cfg_base_address + cfg.offset, cfg.array_strides);
        auto array = XArray::FromBinding(std::move(binding), width, name);
        native_arrays.emplace(name, array);
        return array;
    }

    s_xsignal_cfg XSignalCFG::At(std::string name){
        s_xsignal_cfg cfg;
        new_empty_xdata(name, "", cfg, true);
        return cfg;
    }

    uint64_t XSignalCFG::Address(std::string name){
        auto cfg = this->At(name);
        return this->cfg_base_address + cfg.offset;
    }

    std::string XSignalCFG::String(){
        this->load_cfg();
        std::string ret = "\nBaseAddress: " + std::to_string(this->cfg_base_address) + "\n";
        for(auto &e : this->signal_meta_map){
            ret += e.first + ":\n";
            ret += "  kind: " + e.second.kind + "\n";
            if(e.second.bindable && this->cfg_map.count(e.first)){
                auto &cfg = this->cfg_map[e.first];
                ret += "  offset: " + std::to_string(cfg.offset) + " (address: " +
                    std::to_string(this->cfg_base_address + cfg.offset) + ")\n";
                ret += "  mem_bytes: " + std::to_string(cfg.mem_bytes) + "\n";
                ret += "  rtl_width: " + std::to_string(cfg.rtl_width) + "\n";
                if(cfg.array_size > 0)ret += "  array_size: " + std::to_string(cfg.array_size) + "\n";
                if(!cfg.type.empty())ret += "  type: " + cfg.type + "\n";
            } else {
                if(e.second.rtl_width > 0)ret += "  rtl_width: " + std::to_string(e.second.rtl_width) + "\n";
                if(!e.second.type.empty())ret += "  type: " + e.second.type + "\n";
                if(!e.second.expr.empty())ret += "  expr: " + e.second.expr + "\n";
            }
        }
        return ret;
    }

} // namespace xspcomm
