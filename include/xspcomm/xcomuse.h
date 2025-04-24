#ifndef __xspcomm_xcomuse_h__
#define __xspcomm_xcomuse_h__

#include "xspcomm/xcomm.h"
#include <tuple>

namespace xspcomm {

    class ComUseStepCb{
        int cb_maxcts = -1;
        int cb_counts = 0;
        bool cb_enable = true;
    public:
        uint64_t cycle;
        ComUseStepCb(){}
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

    // Get Array Item
    void SetU64Array(uint64_t address, int index, uint64_t data);
    void SetU32Array(uint64_t address, int index, uint32_t data);
    void  SetU8Array(uint64_t address, int index, uint8_t  data);

    void SetU64Array(unsigned long long *address, int index, uint64_t data);
    void SetU32Array(unsigned int       *address, int index, uint32_t data);
    void  SetU8Array(unsigned char      *address, int index, uint8_t  data);

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
        std::map<std::string, std::tuple<XData*, XData*, ComUseCondCmp, XData*, XData*, xfunction<bool, XData*, XData*, uint64_t>, int, uint64_t, int>> cond_map_xdata;
        std::map<std::string, std::tuple<uint64_t, uint64_t, ComUseCondCmp, int, uint64_t, uint64_t, int, xfunction<bool, uint64_t, uint64_t, uint64_t>, int, uint64_t, int>> cond_map_uint64;
        int _valcmp(uint64_t a, u_int64_t b, int bytes);
    public:
        ComUseCondCheck(XClock* clk=nullptr){if(clk)this->clk_list.push_back(clk);}
        void BindXClock(XClock *clk);
        void SetCondition(std::string unique_name, XData* pin, XData* val, ComUseCondCmp cmp, XData *valid = nullptr, XData *valid_value = nullptr, xfunction<bool, XData*, XData*, uint64_t> func = nullptr, uint64_t arg=0);
        void SetCondition(std::string unique_name, uint64_t pin_ptr, uint64_t val_ptr, ComUseCondCmp cmp, int bytes, uint64_t valid_ptr = 0, uint64_t valid_value_ptr = 0, int valid_bytes = 1, xfunction<bool, uint64_t, uint64_t, uint64_t> func = nullptr, uint64_t arg=0);
        ComUseCondCmp GetValidCmpMode(std::string unique_name);
        void SetValidCmpMode(std::string unique_name, ComUseCondCmp cmp);
        void RemoveCondition(std::string unique_name);
        std::map<std::string, bool> ListCondition();
        std::vector<std::string> GetTriggeredConditionKeys();
        void ClearClock();
        void ClearCondition();
        void ClearAll(){this->ClearClock(); this->ClearCondition();};
        virtual void Call();
    };

    class ComUseDataArray{
        bool is_ref = false;
        int byte_size;
        int *buffer = nullptr;
    public:
        ComUseDataArray(int byte_size):byte_size(byte_size){
            Assert(byte_size > 0, "Need size > 0");
            int n = byte_size/4 + (byte_size % 4 == 0 ? 0:1);
            this->buffer = new int[n];
            memset(this->buffer, 0, this->byte_size);
        }
        ComUseDataArray(uint64_t base, int byte_size):byte_size(byte_size){
            this->buffer = (int*)base;
            this->is_ref = true;
        }
        ~ComUseDataArray(){if(!this->is_ref)delete[] this->buffer;}
        bool operator==(ComUseDataArray & t){
            if(this->byte_size != t.byte_size)return false;
            return memcmp(this->buffer, t.buffer, this->byte_size) == 0;
        }
        ComUseDataArray * Copy(){
            auto ret = new ComUseDataArray(this->byte_size);
            ret->SyncFrom(ret->BaseAddr(), this->byte_size);
            return ret;
        }
        void SyncFrom(uint64_t addr, int size){
            memcpy(this->buffer, (void*)addr, size);
        }
        void SyncTo(uint64_t addr, int size){
            memcpy((void*)addr, this->buffer, size);
        }
        void SetZero(){memset(this->buffer, 0, this->byte_size);}
        uint64_t BaseAddr(){return (u_int64_t)this->buffer;}
        int Size(){return this->byte_size;}
        std::vector<unsigned char> AsBytes(){
            std::vector<unsigned char> ret;
            unsigned char * base = (unsigned char*)this->buffer;
            for(int i=0; i<this->byte_size; i++){
                ret.push_back(base[i]);
            }
            return ret;
        }
        int FromBytes(std::vector<unsigned char> &input){
            auto size = std::min(this->byte_size, (int)input.size());
            unsigned char * base = (unsigned char*)this->buffer;
            for(int i=0; i < size; i++){
                base[i] = input[i];
            }
            return size;
        }
    };

    class ComUseRangeCheck {
        int bytes;
        int range;
    public:
        ComUseRangeCheck(int range, int bytes):bytes(bytes), range(range){
            Assert(bytes <= 8, "FIXME: bytes more than 8 is not supported!");
        }
        static bool cmp(uint64_t t, uint64_t c, int r){
            if(r >= 0){
                return (c - r <= t) && (c >= t);
            }
            return (c - r >= t) && (c <= t);
        }
        static bool ArrayCmp(uint64_t a, uint64_t b, uint64_t self){
            ComUseRangeCheck * p = (ComUseRangeCheck*)self;
            uint64_t vamask = p->bytes == 8 ? -1 : (((uint64_t)1 << (8*p->bytes)) - 1);
            uint64_t target = vamask & (*(uint64_t*)a);
            uint64_t cvalue = vamask & (*(uint64_t*)b);
            return cmp(target, cvalue, p->range);
        }
        static bool XDataCmp(XData *a, XData *b, uint64_t self){
            return cmp(a->U(), b->U(), ((ComUseRangeCheck*)self)->range);
        }
        uint64_t CSelf(){return (uint64_t)this;}
        xfunction<bool, uint64_t, uint64_t, uint64_t> GetArrayCmp(){
            return (bool (*)(uint64_t, uint64_t, uint64_t))ArrayCmp;
        }
        xfunction<bool, XData*, XData*, uint64_t> GetXDataCmp(){
            return (bool (*)(XData*, XData*, uint64_t))XDataCmp;
        }
    };

    class CString {
        public:
        std::string str;
        CString(std::string val):str(val){}
        CString():str(""){}
        uint64_t CharAddress(){
            return (uint64_t)this->str.c_str();
        }
        void AssignTo(char * addr){
            addr = (char *)this->str.c_str();
        }
        void AssignTo(uint64_t addr){
            return this->AssignTo((char*)addr);
        }
        void AssignFrom(const char * val){
            this->str = std::string(val);
        }
        void AssignFrom(uint64_t val){
            return this->AssignFrom((const char*)val);
        }
        std::string Get(){return this->str;}
        void Set(std::string val){this->str = val;}
    };
}

#endif
