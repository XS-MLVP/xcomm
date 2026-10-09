#ifndef XSPCOMM_XCOMUSE_UTILS_H
#define XSPCOMM_XCOMUSE_UTILS_H

#include "xspcomm/xutil.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace xspcomm {

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
        bool operator==(const ComUseDataArray & t) const{
            if(this->byte_size != t.byte_size)return false;
            return memcmp(this->buffer, t.buffer, this->byte_size) == 0;
        }
        ComUseDataArray * Copy(){
            auto ret = new ComUseDataArray(this->byte_size);
            ret->SyncFrom(this->BaseAddr(), this->byte_size);
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

} // namespace xspcomm

#endif
