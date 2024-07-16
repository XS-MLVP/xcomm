#ifndef __xspcomm_xclock__
#define __xspcomm_xclock__

#include "xspcomm/xdata.h"
#include "xspcomm/xport.h"
#include "xspcomm/xutil.h"

#if ENABLE_XCOROUTINE
#include "xspcomm/xcoroutine.h"
#include "xspcomm/xcallback.h"
#endif

namespace xspcomm {
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

    virtual void _step(bool d);
    void _shchedule_await();
    void _call_back(std::vector<XClockCallBack> &list);
    void _add_cb(std::vector<XClockCallBack> &cblist,
                 xfunction<void, u_int64_t, void *> func, void *args,
                 std::string desc);

public:
    std::vector<xspcomm::XData *> clock_pins;
    std::vector<xspcomm::XPort *> ports;
    u_int64_t clk     = 0;
    bool stop_on_rise = true;
    void default_stop_on_rise(bool rise);
    /*************************************************************** */
    //                  Start of Stable public user APIs
    /*************************************************************** */
    XClock();
    XClock(xfunction<int, bool> stepfunc,
           std::initializer_list<xspcomm::XData *> clock_pins  = {},
           std::initializer_list<xspcomm::XPort *> ports = {});
    void ReInit(xfunction<int, bool> stepfunc,
                std::initializer_list<xspcomm::XData *> clock_pins  = {},
                std::initializer_list<xspcomm::XPort *> ports = {});
    void Add(xspcomm::XData *d);
    void Add(xspcomm::XData &d);
    void Add(xspcomm::XPort *d);
    void Add(xspcomm::XPort &d);
    void RefreshComb(){this->eval();}
    void RefreshCombT(){this->eval_t();}
    void Step(int s = 1);
    void Reset();
    void StepRis(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
                 std::string desc = "");
    void StepFal(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
                 std::string desc = "");
    /*************************************************************** */
    //                  End of Stable public user APIs
    /*************************************************************** */
#if ENABLE_XCOROUTINE
    XStep AStep(int i = 1);
    XCondition ACondition(std::function<bool(void)> checker);
    XNext ANext(int n = 1);
#endif
    // Propagate Combinational Logic
    void eval();
    void eval_t();
    /**
     * @brief Step the clock, and call all the callback functions. After every
     * step, it stop on a rise edge (clock = 1).
     */
    virtual void _step_fal();
    virtual void _step_ris();
    void RunStep(int s = 1);
};

} // namespace xspcomm

#endif
