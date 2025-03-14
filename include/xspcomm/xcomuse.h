#ifndef __xspcomm_xcomuse_h__
#define __xspcomm_xcomuse_h__

#include "xspcomm/xcomm.h"

namespace xspcomm {

    class ComUseStepCb{
    public:
        uint64_t cycle;
        ComUseStepCb(){}
        static u_int64_t GetCb();
        u_int64_t CSelf(){return (u_int64_t)this;};
        static void Cb(uint64_t c, void *self);
        virtual void Call();
    };

    // Echo data when valid != 0
    class ComUseEcho: public ComUseStepCb{
    public:
        bool stderr_echo;
        XData* valid = NULL;
        XData* data  = NULL;
        std::string fmt;
        int convert; // 0 (char), 1 (int), 2 (float), 3 (double), 4 (string)
        ComUseEcho(u_int64_t valid, u_int64_t data,
                   bool stderr_echo = true,
                   std::string fmt="%c",
                   int convert = 0):
            stderr_echo(stderr_echo), fmt(fmt), convert(convert){
                this->valid = (XData*) valid;
                this->data = (XData*) data;
            }
        virtual void Call();
    };

}

#endif