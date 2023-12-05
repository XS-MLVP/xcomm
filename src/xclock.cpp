#include "xcomm/xclock.h"

namespace xcomm {
#if ENABLE_XCOROUTINE
XStep::XStep(XClock &clk, u_int64_t step,
             std::map<std::coroutine_handle<>, _XAWait *> *p) :
    _XAWait(p)
{
    Assert(step > 0, "step must > 0");
    this->target_clk = step + clk.clk;
    this->clk        = &clk;
}
XStep *XStep::clone() const
{
    return new XStep(*this);
}
bool XStep::await_ready() noexcept
{
    Assert(this->clk->clk <= this->target_clk, "clk overflow: %lx > %lx",
           this->clk->clk, this->target_clk);
    return this->clk->clk == this->target_clk;
}
bool XStep::ready()
{
    Assert(this->clk->clk <= this->target_clk, "clk overflow: %lx > %lx",
           this->clk->clk, this->target_clk);
    return this->clk->clk == this->target_clk;
}
#else
XStep::XStep(XClock &clk, u_int64_t step)
{
    Assert(false, "XStep needs C++20 support");
}
#endif

void XClock::_step(bool d)
{
    if (this->step_fc)
        this->step_fc(d);
    else
        Warn("XClock.step_fc is not set!");
}

void XClock::_shchedule_await()
{
#if ENABLE_XCOROUTINE
    schedule_awit(&this->cor_handler);
#endif
}

void XClock::_call_back(std::vector<XClockCallBack> &list)
{
    this->in_callback = true;
    for (auto &e : list) {
        Assert(e.func != nullptr,
               "Clock callback corrupted, may be stackoverflow!!");
        e.func(this->clk, e.args);
    }
    this->in_callback = false;
}
void XClock::_add_cb(std::vector<XClockCallBack> &cblist,
                     xfunction<void, u_int64_t, void *> func, void *args,
                     std::string desc)
{
    XClockCallBack cb;
    cb.args = args;
    cb.func = func;
    cb.desc = desc;
    cblist.push_back(cb);
}

void XClock::default_stop_on_rise(bool rise)
{
    this->stop_on_rise = rise;
}
XClock::XClock(xfunction<int, bool> stepfunc,
               std::initializer_list<xcomm::XData *> pins,
               std::initializer_list<xcomm::XPort *> ports)
{
    this->step_fc = stepfunc;
    for (auto &d : pins) { this->Add(d); }
    for (auto &d : ports) { this->Add(d); }
}

void XClock::Add(xcomm::XData *d)
{
    if (contians(this->pins, d)) {
        Warn("pin(%s) is already added", d->mName.c_str());
        return;
    }
    d->SetWriteMode(d->Imme);
    this->pins.push_back(d);
}
void XClock::Add(xcomm::XData &d)
{
    if (contians(this->pins, &d)) {
        Warn("pin(%s) is already added", d.mName.c_str());
        return;
    }
    d.SetWriteMode(d.Imme);
    this->pins.push_back(&d);
}

void XClock::Add(xcomm::XPort *d)
{
    if (contians(this->ports, d)) {
        Warn("port(%s*) is already added", d->prefix.c_str());
        return;
    }
    this->ports.push_back(d);
}

void XClock::Add(xcomm::XPort &d)
{
    if (contians(this->ports, &d)) {
        Warn("port(%s*) is already added", d.prefix.c_str());
        return;
    }
    this->ports.push_back(&d);
}

void XClock::Step(int s)
{
    if (this->in_callback == true) {
        Warn("Cannot call XClock.Step in callbacks, Ignore!");
        return;
    }
    for (int i = 0; i < s; i++) {
        this->clk += 1;
        if (this->stop_on_rise) {
            this->_step_fal();
            this->_step_ris();
        } else {
            this->_step_ris();
            this->_step_fal();
        }
        this->_shchedule_await();
    }
}

void XClock::RunStep(int s)
{
    return this->Step(s);
}

void XClock::_step_fal()
{
    for (auto &v : this->pins) { *v = 0; }
    this->_step(false);
    for (auto &p : this->ports) { p->WriteOnFall(); }
    this->_step(true);
    for (auto &p : this->ports) { p->ReadFresh(WriteMode::Fall); }
    this->_call_back(this->list_call_back_fal);
}

void XClock::_step_ris()
{
    for (auto &v : this->pins) { *v = 1; }
    this->_step(false);
    for (auto &p : this->ports) { p->WriteOnRise(); }
    this->_step(true);
    for (auto &p : this->ports) { p->ReadFresh(WriteMode::Rise); }
    this->_call_back(this->list_call_back_ris);
}

void XClock::Reset()
{
    this->clk = 0;
}

void XClock::StepRis(xfunction<void, u_int64_t, void *> func,
                     void *args, std::string desc)
{
    return this->_add_cb(this->list_call_back_ris, func, args, desc);
}
void XClock::StepFal(xfunction<void, u_int64_t, void *> func,
                     void *args, std::string desc)
{
    return this->_add_cb(this->list_call_back_fal, func, args, desc);
}

#if ENABLE_XCOROUTINE
XStep XClock::AStep(int i)
{
    return XStep(*this, i, &this->cor_handler);
}
XCondition XClock::ACondition(std::function<bool(void)> checker)
{
    return XCondition(checker, &this->cor_handler);
}
XNext XClock::ANext(int n)
{
    return XNext(n, &this->cor_handler);
}
#endif

} // namespace xcomm
