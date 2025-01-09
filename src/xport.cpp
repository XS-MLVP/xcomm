#include "xspcomm/xport.h"

namespace xspcomm {

std::string XPort::asKey(std::string name)
{
    return this->prefix + name;
}

XPort::XPort(std::string prefix)
{
    this->prefix = prefix;
}

int XPort::PortCount()
{
    return this->port_list.size();
}

bool XPort::Add(std::string pin, xspcomm::XData &pin_data)
{
    auto pin_name = this->asKey(pin);
    Assert("pin_name" != pin_name, "pin name cannot be empty!");
    if (this->port_list.count(pin_name) > 0) {
        Error("Add PIN[name=%s] fail, name already exits", pin_name.c_str());
        return false;
    }
    this->port_list[pin_name] = &pin_data;
    this->port_name[pin_name] = pin;
    this->name_port[pin] = pin_name;
    return true;
}

bool XPort::Del(std::string pin)
{
    auto pin_name = this->asKey(pin);
    if (this->port_list.count(pin_name) == 0) {
        Debug("PIN: %s not exits", pin_name.c_str());
        return false;
    }
    this->port_list.erase(pin_name);
    this->port_name.erase(pin_name);
    this->name_port.erase(pin);
    return true;
}

bool XPort::Connect(XPort &target)
{
    for (auto &e : this->port_name) {
        // Connect: xxx_A with yyy_A
        if (target.name_port.count(e.second) == 0) {
            Warn("canot find PIN (name=%s%s)", target.prefix.c_str(),
                 e.second.c_str());
            continue;
            ;
        }
        this->port_list[e.first]->Connect(*target.port_list[target.name_port[e.second]]);
    }
    return true;
}

XPort &XPort::NewSubPort(std::string subprefix)
{
    auto port = new XPort(this->prefix + subprefix);
    for (auto &e : this->port_list) {
        if (sWith(e.first, port->prefix)) {
            port->port_list[e.first] = e.second;
            port->port_name[e.first] = e.first.substr(port->prefix.length());
            port->name_port[port->port_name[e.first]] = e.first;
        }
    }
    return *port;
}

XPort &XPort::SelectPins(std::vector<std::string> pins){
    auto port = new XPort(this->prefix);
    for(auto &p : pins){
        if (this->port_list.count(this->asKey(p)) > 0){
            port->port_list[this->asKey(p)] = this->port_list[this->asKey(p)];
            port->port_name[this->asKey(p)] = p;
            port->name_port[p] = this->asKey(p);
        }else{
            Error("PIN: %s not exits", p.c_str());
        }
    }
    return *port;
}

XPort &XPort::SelectPins(std::initializer_list<std::string> pins){
    return this->SelectPins(std::vector<std::string>(pins));
}

xspcomm::XData &XPort::operator[](std::string key)
{
    return this->Get(key);
}

xspcomm::XData &XPort::Get(std::string key, bool raw_key)
{
    Assert(this->port_list.count(raw_key ? key : this->asKey(key)) > 0,
           "key: %s not find!",
           raw_key ? key.c_str() : this->asKey(key).c_str());
    return *this->port_list[raw_key ? key : this->asKey(key)];
}

XPort &XPort::FlipIOType()
{
    for (auto &e : this->port_list) { e.second->FlipIOType(); }
    return *this;
}

XPort &XPort::AsBiIO()
{
    for (auto &e : this->port_list) { e.second->AsBiIO(); }
    return *this;
}

XPort &XPort::WriteOnRise()
{
    for (auto &e : this->port_list) { e.second->WriteOnRise(); }
    return *this;
}

XPort &XPort::WriteOnFall()
{
    for (auto &e : this->port_list) { e.second->WriteOnFall(); }
    return *this;
}

XPort &XPort::AsImmWrite(){
    for (auto &e : this->port_list) { if(!e.second->IsOutIO())e.second->AsImmWrite(); }
    return *this;
}
XPort &XPort::AsRiseWrite(){
    for (auto &e : this->port_list) { if(!e.second->IsOutIO())e.second->AsRiseWrite(); }
    return *this;
}
XPort &XPort::AsFallWrite(){
    for (auto &e : this->port_list) { if(!e.second->IsOutIO())e.second->AsFallWrite(); }
    return *this;
}

XPort &XPort::ReadFresh(xspcomm::WriteMode m)
{
    for (auto &e : this->port_list) { e.second->ReadFresh(m); }
    return *this;
}

XPort &XPort::SetZero()
{
    for (auto &e : this->port_list) {
        if (e.second->mIOType != IOType::Output) { *e.second = 0; };
    }
    return *this;
}

std::string XPort::String(std::string prefix)
{
    std::string ret;
    for (auto &e : this->port_list) {
        if (prefix.length() > 0) {
            if (!sWith(e.first, this->asKey(prefix))) continue;
        }
        ret += e.first + "=0x" + e.second->String() + " ";
    }
    return ret;
}

std::string XPort::GetPrefix(){
    return this->prefix;
}

std::vector<std::string> XPort::GetKeys(bool raw_key){
    std::vector<std::string> ret;
    for(auto &i : this->port_list){
        if (raw_key){
            ret.push_back(i.first);
        }
        else{
            ret.push_back(i.first.substr(this->prefix.length()));
        }
    }
    return ret;
}

XPort &XPort::operator=(XPort &data){
    this->name_port = data.name_port;
    this->port_list = data.port_list;
    this->port_name = data.port_name;
    this->prefix = data.prefix;
    return *this;
}
} // namespace xspcomm
