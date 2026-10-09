#include "xspcomm/common/memory.h"

namespace xspcomm {

uint64_t GetFromU64Array(uint64_t address, int index){
    return ((uint64_t *)address)[index];
}
uint32_t GetFromU32Array(uint64_t address, int index){
    return ((uint32_t *)address)[index];
}
uint8_t  GetFromU8Array(uint64_t address,  int index){
    return ((uint8_t *)address)[index];
}

uint64_t GetFromU64Array(unsigned long long * address, int index){
    return address[index];
}
uint32_t GetFromU32Array(unsigned int * address, int index){
    return address[index];
}
uint8_t   GetFromU8Array(unsigned char * address, int index){
    return address[index];
}

// Get Array Item
void SetU64Array(uint64_t address, int index, uint64_t data){((uint64_t*)address)[index]=data;}
void SetU32Array(uint64_t address, int index, uint32_t data){((uint32_t*)address)[index]=data;}
void  SetU8Array(uint64_t address, int index, uint8_t  data){((uint8_t*)address)[index] =data;}

void SetU64Array(unsigned long long *address, int index, uint64_t data){address[index]=data;}
void SetU32Array(unsigned int       *address, int index, uint32_t data){address[index]=data;}
void  SetU8Array(unsigned char      *address, int index, uint8_t  data){address[index]=data;}

uint64_t U64PtrAsU64(unsigned long long *p){return (uint64_t)p;}
uint64_t U32PtrAsU64(unsigned int *p){return (uint64_t)p;}
uint64_t  U8PtrAsU64(unsigned char *p){return (uint64_t)p;}

unsigned long long *U64AsU64Ptr(uint64_t p){return (unsigned long long *)p;}
unsigned int       *U64AsU32Ptr(uint64_t p){return (unsigned int *)p;}
unsigned char      *U64AsU8Ptr(uint64_t p){return (unsigned char *)p;}

} // namespace xspcomm
