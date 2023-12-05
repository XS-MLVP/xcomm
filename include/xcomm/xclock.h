#ifndef __xcomm_xclock__
#define __xcomm_xclock__

#include "xcomm/xdata.h"
#include "xcomm/xport.h"
#include "xcomm/xutil.h"
#include "xcomm/xcoroutine.h"
#include "xcomm/xcallback.h"

namespace xcomm {
class XClock;
#if ENABLE_XCOROUTINE
class XStep : public _XAWait
{
    u_int64_t target_clk;
    XClock *clk;

public:
    XStep(XClock &clk, u_int64_t step = 1,
          std::map<std::coroutine_handle<>, _XAWait *> *p = &__xhandl_list__);
    XStep *clone() const override;
    bool await_ready() noexcept override;
    bool ready() override;
    constexpr void await_resume() const noexcept {}
};
#else
class XStep
{
public:
    XStep(XClock &clk, u_int64_t step = 1);
};
#endif

struct XClockCallBack {
    std::string desc;
    xfunction<void, u_int64_t, void *> func;
    void *args;
};

class XClock
{
    xfunction<int, bool> step_fc;
#if ENABLE_XCOROUTINE
    std::map<std::coroutine_handle<>, _XAWait *> cor_handler;
#endif
    std::vector<XClockCallBack> list_call_back_ris;
    std::vector<XClockCallBack> list_call_back_fal;
    bool in_callback = false;

    void _step(bool d);
    void _shchedule_await();
    void _call_back(std::vector<XClockCallBack> &list);
    void _add_cb(std::vector<XClockCallBack> &cblist,
                 xfunction<void, u_int64_t, void *> func, void *args,
                 std::string desc);

public:
    std::vector<xcomm::XData *> pins;
    std::vector<xcomm::XPort *> ports;
    u_int64_t clk     = 0;
    bool stop_on_rise = true;

    void default_stop_on_rise(bool rise);
    XClock(xfunction<int, bool> stepfunc,
           std::initializer_list<xcomm::XData *> pins  = {},
           std::initializer_list<xcomm::XPort *> ports = {});
    void Add(xcomm::XData *d);
    void Add(xcomm::XData &d);
    void Add(xcomm::XPort *d);
    void Add(xcomm::XPort &d);

    /**
     * @brief Step the clock, and call all the callback functions. After every
     * step, it stop on a rise edge (clock = 1).
     */
    void Step(int s = 1);
    void RunStep(int s = 1);
    void _step_fal();
    void _step_ris();

    void Reset();
    void StepRis(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
                 std::string desc = "");
    void StepFal(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
                 std::string desc = "");
#if ENABLE_XCOROUTINE
    XStep AStep(int i = 1);
    XCondition ACondition(std::function<bool(void)> checker);
    XNext ANext(int n = 1);
#endif
};

} // namespace xcomm

#endif
