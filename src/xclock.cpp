#include "xspcomm/xclock.h"

namespace xspcomm {
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

XClock::XClock() : XClock(nullptr, {}, {}){};

/// @brief set the step function, bind clock pins which need 0/1 and bind ports
/// to write/read
/// @param stepfunc
/// @param clock_pins the clock pins which need set high/low
/// @param ports
XClock::XClock(xfunction<int, bool> stepfunc,
               std::initializer_list<xspcomm::XData *> clock_pins,
               std::initializer_list<xspcomm::XPort *> ports)
{
    this->ReInit(stepfunc, clock_pins, ports);
}
XClock::XClock(uint64_t stepfunc, uint64_t dut,
               std::initializer_list<xspcomm::XData *> clock_pins,
               std::initializer_list<xspcomm::XPort *> ports)
{
    this->ReInit(stepfunc, dut, clock_pins, ports);
}
void XClock::ReInit(uint64_t stepfunc, uint64_t dut,
                    std::initializer_list<xspcomm::XData *> clock_pins,
                    std::initializer_list<xspcomm::XPort *> ports)
{
    auto step_func = (int (*)(uint64_t, bool))stepfunc;
    auto step = [step_func, dut](bool d) {
        return step_func(dut, d);
    };
    this->ReInit(step, clock_pins, ports);
}
void XClock::ReInit(xfunction<int, bool> stepfunc,
                    std::initializer_list<xspcomm::XData *> clock_pins,
                    std::initializer_list<xspcomm::XPort *> ports)
{
    this->step_fc = stepfunc;
    for (auto &d : clock_pins) { this->Add(d); }
    for (auto &d : ports) { this->Add(d); }
}

/// @brief add a clock pin which need set high/low
XClock& XClock::Add(xspcomm::XData *d)
{
    if (contians(this->clock_pins, d)) {
        Warn("pin(%s) is already added", d->mName.c_str());
        return *this;
    }
    d->SetWriteMode(d->Imme);
    this->clock_pins.push_back(d);
    return *this;
}

/// @brief add a clock pin which need set high/low
XClock& XClock::Add(xspcomm::XData &d)
{
    if (contians(this->clock_pins, &d)) {
        Warn("pin(%s) is already added", d.mName.c_str());
        return *this;
    }
    d.SetWriteMode(d.Imme);
    this->clock_pins.push_back(&d);
    return *this;
}


/// @brief add a port which need write/read on rise/fall
XClock& XClock::Add(xspcomm::XPort *d)
{
    if (contians(this->ports, d)) {
        Warn("port(%s*) is already added", d->prefix.c_str());
        return *this;
    }
    this->ports.push_back(d);
    return *this;
}

/// @brief add a port which need write/read on rise/fall
XClock& XClock::Add(xspcomm::XPort &d)
{
    if (contians(this->ports, &d)) {
        Warn("port(%s*) is already added", d.prefix.c_str());
        return *this;
    }
    this->ports.push_back(&d);
    return *this;
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
    auto sub_clk_ris = this->_get_div_clk_ris(this->clk, false);
    auto sub_clk_fal = this->_get_div_clk_fal(this->clk, false);
    this->_fal_pins();
    for(auto &c : sub_clk_fal){c->_fal_pins();}
    for(auto &c : sub_clk_ris){c->_ris_pins();}
    this->_step(false);
    this->_fal_ports();
    for(auto &c : sub_clk_fal){c->_fal_ports();}
    for(auto &c : sub_clk_ris){c->_ris_ports();}
    this->_step(true);
    this->_fal_refresh();
    for(auto &c : sub_clk_fal){c->_fal_refresh();}
    for(auto &c : sub_clk_ris){c->_ris_refresh();}
}

void XClock::_step_ris()
{
    auto sub_clk_ris = this->_get_div_clk_ris(this->clk, true);
    auto sub_clk_fal = this->_get_div_clk_fal(this->clk, true);
    this->_ris_pins();
    for(auto &c : sub_clk_ris){c->_ris_pins();}
    for(auto &c : sub_clk_fal){c->_fal_pins();}
    this->_step(false);
    this->_ris_ports();
    for(auto &c : sub_clk_ris){c->_ris_ports();}
    for(auto &c : sub_clk_fal){c->_fal_ports();}
    this->_step(true);
    this->_ris_refresh();
    for(auto &c : sub_clk_ris){c->_ris_refresh();}
    for(auto &c : sub_clk_fal){c->_fal_refresh();}
}

void XClock::eval()
{
    this->_step(false);
}

void XClock::eval_t()
{
    this->_step(true);
}

void XClock::Reset()
{
    this->clk = 0;
}

void XClock::StepRis(xfunction<void, u_int64_t, void *> func, void *args,
                     std::string desc)
{
    return this->_add_cb(this->list_call_back_ris, func, args, desc);
}
void XClock::StepFal(xfunction<void, u_int64_t, void *> func, void *args,
                     std::string desc)
{
    return this->_add_cb(this->list_call_back_fal, func, args, desc);
}

void XClock::StepRis(u_int64_t func, u_int64_t args, std::string desc){
    return this->StepRis((void(*)(u_int64_t, void*))func, (void *)args, desc);
}
void XClock::StepFal(u_int64_t func, u_int64_t args, std::string desc){
    return this->StepFal((void(*)(u_int64_t, void*))func, (void *)args, desc);
}

void XClock::ClearRisCallBacks(){this->list_call_back_ris.clear();}
void XClock::ClearFalCallBacks(){this->list_call_back_fal.clear();}

int XClock::StepRisQueueSize(){ return (int)this->list_call_back_ris.size(); }
int XClock::StepFalQueueSize(){ return (int)this->list_call_back_fal.size(); }

void XClock::_fal_pins(){
    for (auto &v : this->clock_pins) { *v = 0; }
}

void XClock::_fal_ports(){
    for (auto &p : this->ports) { p->WriteOnFall();}
}

void XClock::_fal_refresh(){
    for (auto &p : this->ports) { p->ReadFresh(WriteMode::Fall); }
    this->_call_back(this->list_call_back_fal);
}

void XClock::_ris_pins(){
    for (auto &v : this->clock_pins) { *v = 1; }
}

void XClock::_ris_ports(){
    for (auto &p : this->ports) { p->WriteOnRise(); }
}

void XClock::_ris_refresh(){
    for (auto &p : this->ports) { p->ReadFresh(WriteMode::Rise); }
    this->_call_back(this->list_call_back_ris);
}

void XClock::FreqDivWith(int div, XClock *clk, int shift){
    if (this->div_clk.count(clk) > 0){
        Warn("Clock: %p already div with (div: %d, shift: %d), ignore", clk, this->div_clk[clk][0], this->div_clk[clk][1]);
        return;
    }
    this->div_clk[clk] = {div, shift};
}

void XClock::FreqDivDelete(XClock *clk){
    if (this->div_clk.count(clk) == 0){
        Warn("Clock: %p not div with %p, ignore", clk, this);
        return;
    }
    this->div_clk.erase(clk);
}

std::vector<XClock *> XClock::_get_div_clk_ris(u_int64_t cycle, bool rise){
    std::vector<XClock *> ret;
    auto tick = cycle * 2 - (rise ? 0 : 1);
    for (auto &e : this->div_clk){
        auto ptick = tick + e.second[1];
        if (ptick % (2 * e.second[0]) == 0){
            if (!e.first->stop_on_rise) e.first->clk += 1;
            ret.push_back(e.first);
        }
    }
    return ret;
}

std::vector<XClock *> XClock::_get_div_clk_fal(u_int64_t cycle, bool rise){
    std::vector<XClock *> ret;
    auto tick = cycle * 2 - (rise ? 0 : 1);
    for (auto &e : this->div_clk){
        auto ptick = tick + e.second[1];
        if ((ptick + e.second[0]) % (2 * e.second[0]) == 0){
            if (e.first->stop_on_rise) e.first->clk += 1;
            ret.push_back(e.first);
        }
    }
    return ret;
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

extern "C" {
    int test_step_fun(void* self, bool d){
        Info("[%p]test u64 step func called: %d", self, d);
        return 0;
    };
    void test_risfail_cb(u_int64_t cycle, void* args){
        Info("[%ld]test ris/fal callback called: %p", cycle, args);
    }
}

uint64_t TEST_get_u64_step_func(){
    return (uint64_t)test_step_fun;
}
uint64_t TEST_get_u64_ris_fal_cblback_func(){
    return (uint64_t)test_risfail_cb;
}

} // namespace xspcomm
