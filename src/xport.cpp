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
    return true;
}

bool XPort::Connect(XPort &target)
{
    for (auto &e : this->port_name) {
        // Connect: xxx_A with yyy_A
        if (target.port_name.count(e.second) == 0) {
            Warn("canot find PIN (name=%s%s)", target.prefix.c_str(),
                 e.second.c_str());
            continue;
            ;
        }
        this->port_list[e.first]->Connect(*target.port_list[e.first]);
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
        }
    }
    return *port;
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

XPort &XPort::Flip()
{
    for (auto &e : this->port_list) { e.second->Flip(); }
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
} // namespace xspcomm
