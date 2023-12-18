#ifndef __js_extend__
#define __js_extend__

#include "xspcomm/xclock.h"
#include <napi.h>

namespace xspcomm {

// XClock rewrite
class JSXClock : public xspcomm::XClock
{
public:

    JSXClock(Napi::Function callback) : xspcomm::XClock(nullptr, {}, {})
    {
        this->js_step_fc = Napi::Persistent(callback);
        auto env = this->js_step_fc.Env();
    };

    void _step(bool d) override
    {
        auto env = this->js_step_fc.Env();
        this->js_step_fc.Call(env.Global(), {Napi::Boolean::New(env, d)});
    }

    void default_stop_on_rise(bool rise)
    {
        return xspcomm::XClock::default_stop_on_rise(rise);
    }
    void Add(xspcomm::XData *d) { return xspcomm::XClock::Add(d); }
    void Add(xspcomm::XPort *d) { return xspcomm::XClock::Add(d); }
    void Step(int s = 1) { return xspcomm::XClock::Step(s); }
    void RunStep(int s = 1) { return xspcomm::XClock::RunStep(s); }
    void Reset() { return xspcomm::XClock::Reset(); }
    u_int64_t GetClk() { return this->clk; }

    void _step_fal() override
    {
        xspcomm::XClock::_step_fal();
        this->_call_cb(this->js_list_call_back_fal);
    }
    void _step_ris() override
    {
        xspcomm::XClock::_step_ris();
        this->_call_cb(this->js_list_call_back_ris);
    }
    void StepRis(Napi::Function func)
    {
        this->js_list_call_back_ris.push_back(Napi::Persistent(func));
    }
    void StepFal(Napi::Function func)
    {
        this->js_list_call_back_fal.push_back(Napi::Persistent(func));
    }

private:
    void _call_cb(std::vector<Napi::FunctionReference> &cb_list)
    {
        for (auto &cb : cb_list) {
            auto env = cb.Env();
            cb.Call(env.Global(), {Napi::Number::New(env, this->clk)});
        }
    }
    Napi::FunctionReference js_step_fc;
    std::vector<Napi::FunctionReference> js_list_call_back_ris;
    std::vector<Napi::FunctionReference> js_list_call_back_fal;
};

} // namespace xspcomm

#endif
