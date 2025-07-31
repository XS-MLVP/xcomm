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

uint64_t TEST_get_u64_step_func();
uint64_t TEST_get_u64_ris_fal_cblback_func();

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
    bool is_disable = false;
    // 0 default
    // 1 ignore read_refresh
    // 2 ignore step(false)
    // 3 ignore port write, use with XData writeImme Model.
    // 4-10 reserved for future use
    // 11 only step_ris cbs
    // 12 only step_fal cbs
    int fast_mode_level = 0;

    virtual void _step(bool d);
    void _shchedule_await();
    void _call_back(std::vector<XClockCallBack> &list);
    void _add_cb(std::vector<XClockCallBack> &cblist,
                 xfunction<void, u_int64_t, void *> func, void *args,
                 std::string desc);

public:
    std::vector<xspcomm::XData *> clock_pins;
    std::vector<xspcomm::XPort *> ports;
    std::map<XClock *, std::array<int, 2>> div_clk;
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
    XClock(uint64_t stepfunc, uint64_t dut,
           std::initializer_list<xspcomm::XData *> clock_pins  = {},
           std::initializer_list<xspcomm::XPort *> ports = {});
    void ReInit(uint64_t stepfunc, uint64_t dut,
                std::initializer_list<xspcomm::XData *> clock_pins  = {},
                std::initializer_list<xspcomm::XPort *> ports = {});
    XClock& Add(xspcomm::XData *d);
    XClock& Add(xspcomm::XData &d);
    XClock& Add(xspcomm::XPort *d);
    XClock& Add(xspcomm::XPort &d);
    XClock& AddPin(xspcomm::XData *d){return this->Add(d);}
    XClock& AddPin(xspcomm::XData &d){return this->Add(d);}
    void RefreshComb(){this->eval();}
    void RefreshCombT(){this->eval_t();}
    void Step(int s = 1);
    void Reset();
    void StepRis(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
                 std::string desc = "");
    void StepFal(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
                 std::string desc = "");
    void StepRis(u_int64_t func, u_int64_t args = 0, std::string desc = "");
    void StepFal(u_int64_t func, u_int64_t args = 0, std::string desc = "");
    int RemoveStepRisCbByDesc(std::string desc);
    int RemoveStepRisCbByFunc(xfunction<void, u_int64_t, void *> func);
    int RemoveStepRisCb(xfunction<void, u_int64_t, void *> func, std::string desc);
    int RemoveStepFalCbByDesc(std::string desc);
    int RemoveStepFalCbByFunc(xfunction<void, u_int64_t, void *> func);
    int RemoveStepFalCb(xfunction<void, u_int64_t, void *> func, std::string desc);
    std::vector<std::string> ListSteRisCbDesc();
    std::vector<std::string> ListSteFalCbDesc();
    int StepRisQueueSize();
    int StepFalQueueSize();
    void FreqDivWith(int div, XClock *clk, int shift=0);
    void FreqDivWith(int div, XClock &clk, int shift=0){return this->FreqDivWith(div, &clk, shift);};
    void FreqDivDelete(XClock *clk);
    void FreqDivDelete(XClock &clk){return this->FreqDivDelete(&clk);};
    void ClearRisCallBacks();
    void ClearFalCallBacks();
    void SetFastMode(int level){this->fast_mode_level = level;};
    int GetFastMode(){return this->fast_mode_level;}
    bool IsDisable(){return this->is_disable;}
    void Disable(){this->is_disable = true;}
    void Enable(){this->is_disable = false;}
    uint64_t CSelf(){return (uint64_t)this;}
    /*************************************************************** */
    //                  End of Stable public user APIs
    /*************************************************************** */
    void _fal_pins();
    void _fal_ports();
    void _fal_refresh();
    void _ris_pins();
    void _ris_ports();
    void _ris_refresh();
    int _remove_step_cb_by(int type, int group, std::string desc, xfunction<void, u_int64_t, void *> func=nullptr);
    std::vector<XClock *> _get_div_clk_ris(u_int64_t cycle, bool rise);
    std::vector<XClock *> _get_div_clk_fal(u_int64_t cycle, bool rise);
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
