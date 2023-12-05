#include "xcomm/xcoroutine.h"

namespace xcomm {
#if ENABLE_XCOROUTINE
#pragma message("ENABLE_XCOROUTINE is enabled")

std::map<std::coroutine_handle<>, _XAWait *> __xhandl_list__;

bool schedule_awit(std::map<std::coroutine_handle<>, _XAWait *> *p)
{
    for (auto it = p->begin(); it != p->end();) {
        if (it->first.done()) {
            it->first.destroy();
            delete it->second;
            it = p->erase(it);
        } else {
            it++;
        }
    }

    std::map<std::coroutine_handle<>, _XAWait *> xhandl_list(*p);
    for (auto it = xhandl_list.begin(); it != xhandl_list.end();) {
        Assert(it->second != nullptr, "alive coroutine need a await object");
        if (!it->first.done() && it->second->ready()) { it->first.resume(); }
        it++;
    }
    return xhandl_list.size() == 0 && (*p).size() == 0;
}

bool _PromiseBase::set_handler_list(
    std::map<std::coroutine_handle<>, _XAWait *> *p)
{
    if (this->_p_hd_list == nullptr) {
        this->_p_hd_list = p;
        return true;
    }
    return false;
}

_XAWait::_XAWait(
    std::map<std::coroutine_handle<>, _XAWait *> *p)
{
    _p_xhandler_list = p;
}

bool _XAWait::await_ready() noexcept
{
    return this->ready();
}

void _XAWait::await_suspend(std::coroutine_handle<> handler)
{
    if (this->_p_xhandler_list == nullptr) {
        Warn("XCorutine is not initialized with xhandler list !");
        return;
    }
    if (_p_xhandler_list->count(handler) > 0) {
        delete (*_p_xhandler_list)[handler];
    }
    (*_p_xhandler_list)[handler] = this->clone();
    std::coroutine_handle<_PromiseBase>::from_address(handler.address())
        .promise()
        .set_handler_list(_p_xhandler_list);
}

XCondition::XCondition(
    std::function<bool(void)> checker,
    std::map<std::coroutine_handle<>, _XAWait *> *p) :
    _XAWait(p)
{
    this->checker = checker;
}

XCondition *XCondition::clone() const
{
    return new XCondition(*this);
}

bool XCondition::ready()
{
    return this->checker();
}

constexpr void XCondition::await_resume() const noexcept {}

XNext::XNext(int n, std::map<std::coroutine_handle<>, _XAWait *> *p) :
    _XAWait(p)
{
    this->step = n;
}

XNext *XNext::clone() const
{
    return new XNext(*this);
}

bool XNext::ready()
{
    if (this->step <= 0) return true;
    this->step -= 1;
    return false;
}
constexpr void XNext::await_resume() const noexcept {}

#else
#pragma message("ENABLE_XCOROUTINE is not enabled, it needs C++20 support")

bool schedule_awit(void *p)
{
    Assert(false, "schedule_awit need c++20 support!");
}

XCondition::XCondition(std::function<bool(void)> checker, void *p)
{
    Assert(false, "XCondition needs C++20 support");
}

#endif // ENABLE_XCOROUTINE

} // namespace xcomm
