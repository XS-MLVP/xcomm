#ifndef __xcomm_xcoroutine__
#define __xcomm_xcoroutine__

#include "xcomm/xutil.h"

#if ENABLE_XCOROUTINE

#include <coroutine>
#include <map>
#include <type_traits>
#include <typeinfo>

namespace xcomm {

class _XAWait;
extern std::map<std::coroutine_handle<>, _XAWait *> __xhandl_list__;

class _PromiseBase
{
public:
    std::map<std::coroutine_handle<>, _XAWait *> *_p_hd_list = nullptr;
    bool set_handler_list(std::map<std::coroutine_handle<>, _XAWait *> *p);
};

class _XAWait
{
    std::map<std::coroutine_handle<>, _XAWait *> *_p_xhandler_list = nullptr;

public:
    _XAWait(std::map<std::coroutine_handle<>, _XAWait *> *p = &__xhandl_list__);
    virtual bool ready()           = 0;
    virtual _XAWait *clone() const = 0;
    virtual bool await_ready() noexcept;
    virtual void await_suspend(std::coroutine_handle<> handler);
};

template <typename T>
class XPromise;

template <typename T>
class XCorutine : public _XAWait
{
public:
    using promise_type = XPromise<T>;
    XCorutine(std::coroutine_handle<XPromise<T>> h) :
        handle(h), _XAWait(nullptr)
    {}
    std::coroutine_handle<XPromise<T>> handle;
    bool ready() override { return handle.promise().is_returned; }
    XCorutine<T> *clone() const { return new XCorutine<T>(*this); }
    T await_resume() const noexcept
    {
        if constexpr (!std::is_same<T, void>::value) {
            return handle.promise().value;
        }
    }
    void await_suspend(std::coroutine_handle<> h) override
    {
        auto p = handle.promise();
        Assert(p._p_hd_list != nullptr,
               "XCorutine is not initialized with xhandler list !");
        if (p._p_hd_list->count(h) > 0) { delete (*p._p_hd_list)[h]; }
        (*p._p_hd_list)[h] = this->clone();
        std::coroutine_handle<_PromiseBase>::from_address(h.address())
            .promise()
            .set_handler_list(p._p_hd_list);
    }
};

template <typename T>
class XPromise : public _PromiseBase
{
public:
    class empty
    {};
    bool is_returned = false;
    typename std::conditional<std::is_same<T, void>::value, empty, T>::type
        value;
    XCorutine<T> get_return_object()
    {
        return std::coroutine_handle<XPromise<T>>::from_promise(*this);
    }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept(true) { return {}; }
    void unhandled_exception() {}

    template <typename U = T>
    void return_value(U v = U{})
    {
        if constexpr (!std::is_same<U, void>::value) { value = v; }
        is_returned = true;
    }
};

template <>
class XPromise<void> : public _PromiseBase
{
public:
    bool is_returned = false;
    XCorutine<void> get_return_object()
    {
        return std::coroutine_handle<XPromise<void>>::from_promise(*this);
    }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept(true) { return {}; }
    void unhandled_exception() {}
    void return_void() { is_returned = true; }
};

template <typename T = void>
using xcorutine = XCorutine<T>;

bool schedule_awit(
    std::map<std::coroutine_handle<>, _XAWait *> *p = &__xhandl_list__);

class XCondition : public _XAWait
{
    std::function<bool(void)> checker;

public:
    XCondition(
        std::function<bool(void)> checker,
        std::map<std::coroutine_handle<>, _XAWait *> *p = &__xhandl_list__);
    XCondition *clone() const override;
    bool ready() override;
    constexpr void await_resume() const noexcept;
};

class XNext : public _XAWait
{
    int step;

public:
    XNext(int n                                           = 1,
          std::map<std::coroutine_handle<>, _XAWait *> *p = &__xhandl_list__);
    XNext *clone() const override;
    bool ready() override;
    constexpr void await_resume() const noexcept;
};

} // namespace xcomm

#else

namespace xcomm {
bool schedule_awit(void *p = nullptr);

class XCondition
{
public:
    XCondition(std::function<bool(void)> checker,
               void *p = nullptr);
};
} // namespace xcomm
#endif // ENABLE_XCOROUTINE
#endif
