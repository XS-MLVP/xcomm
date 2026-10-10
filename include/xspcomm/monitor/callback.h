#ifndef XSPCOMM_MONITOR_CALLBACK_H
#define XSPCOMM_MONITOR_CALLBACK_H

#include "xspcomm/xutil.h"
#include "xspcomm/common/lifetime.h"
#include <string>
#include <memory>
#include <vector>

namespace xspcomm {

    class XData;
    class XClock;

    class XStepCallback{
        int cb_maxcts = -1;
        int cb_counts = 0;
        bool cb_enable = true;
        detail::Lifetime lifetime;
        struct Registration {
            XClock *clock;
            std::weak_ptr<const int> lifetime;
            bool rising;
            std::string description;
        };
        std::vector<Registration> registrations;
    public:
        uint64_t cycle = 0;
        XStepCallback() = default;
        XStepCallback(const XStepCallback &) = delete;
        XStepCallback &operator=(const XStepCallback &) = delete;
        virtual ~XStepCallback();
        void Attach(XClock *clock, bool rising = true);
        void Detach();
        void Disable();
        void Enable();
        bool IsDisable();
        void SetMaxCbs(int c);
        int GetCbCount();
        int IncCbCount();
        int DecCbCount();
        void Reset();
        static u_int64_t GetCb();
        u_int64_t CSelf(){return (u_int64_t)this;};
        static void Cb(uint64_t c, void *self);
        virtual void Call();
    };

    // Echo data when valid != 0
    class XEcho: public XStepCallback{
    public:
        bool stderr_echo;
        XData* valid = NULL;
        XData* data  = NULL;
        std::string fmt;
        int convert; // 0 (char), 1 (int), 2 (float), 3 (double), 4 (string)
        XEcho(u_int64_t valid, u_int64_t data,
                   bool stderr_echo = true,
                   std::string fmt="%c",
                   int convert = 0):
            stderr_echo(stderr_echo), fmt(fmt), convert(convert){
                this->valid = (XData*) valid;
                this->data = (XData*) data;
            }
        virtual void Call();
    };

} // namespace xspcomm

#endif
