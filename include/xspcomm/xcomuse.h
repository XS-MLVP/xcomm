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

    // Get Array Item
    uint64_t GetFromU64Array(uint64_t address, int index);
    uint32_t GetFromU32Array(uint64_t address, int index);
    uint8_t   GetFromU8Array(uint64_t address, int index);

    uint64_t GetFromU64Array(unsigned long long *address, int index);
    uint32_t GetFromU32Array(unsigned int       *address, int index);
    uint8_t   GetFromU8Array(unsigned char      *address, int index);

    // Ptr As U64
    uint64_t U64PtrAsU64(unsigned long long *p);
    uint64_t U32PtrAsU64(unsigned int       *p);
    uint64_t  U8PtrAsU64(unsigned char      *p);

}

#endif