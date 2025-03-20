#ifndef __xspcomm_xcomuse_h__
#define __xspcomm_xcomuse_h__

#include "xspcomm/xcomm.h"
#include <tuple>

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
    unsigned long long *U64AsU64Ptr(uint64_t p);
    unsigned int       *U64AsU32Ptr(uint64_t p);
    unsigned char      *U64AsU8Ptr(uint64_t p);

    // Condition Checker
    enum class ComUseCondCmp{
        EQ = 0,
        NE = 1,
        GT = 2,
        GE = 3,
        LT = 4,
        LE = 5,
    };
    class ComUseCondCheck: public ComUseStepCb{
        std::vector<XClock*> clk_list;
        std::map<std::string, std::tuple<XData*, XData*, ComUseCondCmp, XData*, XData*, xfunction<bool, XData*, XData*>, int>> cond_map_xdata;
        std::map<std::string, std::tuple<uint64_t, uint64_t, ComUseCondCmp, int, uint64_t, uint64_t, xfunction<bool, uint64_t, uint64_t>, int>> cond_map_uint64;
    public:
        ComUseCondCheck(XClock* clk=nullptr){if(clk)this->clk_list.push_back(clk);}
        void BindXClock(XClock *clk);
        void SetCondition(std::string unique_name, XData* pin, XData* val, ComUseCondCmp cmp, XData *valid = nullptr, XData *valid_value = nullptr, xfunction<bool, XData*, XData*> func = nullptr);
        void SetCondition(std::string unique_name, uint64_t pin_ptr, uint64_t val_ptr, ComUseCondCmp cmp, int bytes, uint64_t valid_ptr = 0, uint64_t valid_value_ptr = 0, xfunction<bool, uint64_t, uint64_t> func = nullptr);
        void RemoveCondition(std::string unique_name);
        std::vector<std::string> GetTriggeredConditionKeys();
        void ClearClock();
        void ClearCondition();
        void ClearAll(){this->ClearClock(); this->ClearCondition();};
        virtual void Call();
    };

}

#endif