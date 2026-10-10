#include "xspcomm/common/memory.h"

namespace xspcomm {
namespace {

template<class T>
unsigned char *ArrayElement(uint64_t address, int index) {
    auto *base = reinterpret_cast<unsigned char*>(address);
    return base + ptrdiff_t(index) * ptrdiff_t(sizeof(T));
}

} // namespace

uint64_t GetFromU64Array(uint64_t address, int index) {
    return memory::Load<uint64_t>(ArrayElement<uint64_t>(address, index));
}
uint32_t GetFromU32Array(uint64_t address, int index) {
    return memory::Load<uint32_t>(ArrayElement<uint32_t>(address, index));
}
uint8_t GetFromU8Array(uint64_t address, int index) {
    return memory::Load<uint8_t>(ArrayElement<uint8_t>(address, index));
}

uint64_t GetFromU64Array(unsigned long long *address, int index) { return memory::Load<unsigned long long>(address + index); }
uint32_t GetFromU32Array(unsigned int *address, int index) { return memory::Load<unsigned int>(address + index); }
uint8_t GetFromU8Array(unsigned char *address, int index) { return memory::Load<unsigned char>(address + index); }

void SetU64Array(uint64_t address, int index, uint64_t data) {
    memory::Store<uint64_t>(ArrayElement<uint64_t>(address, index), data);
}
void SetU32Array(uint64_t address, int index, uint32_t data) {
    memory::Store<uint32_t>(ArrayElement<uint32_t>(address, index), data);
}
void SetU8Array(uint64_t address, int index, uint8_t data) {
    memory::Store<uint8_t>(ArrayElement<uint8_t>(address, index), data);
}

void SetU64Array(unsigned long long *address, int index, uint64_t data) { memory::Store<unsigned long long>(address + index, data); }
void SetU32Array(unsigned int *address, int index, uint32_t data) { memory::Store<unsigned int>(address + index, data); }
void SetU8Array(unsigned char *address, int index, uint8_t data) { memory::Store<unsigned char>(address + index, data); }

uint64_t U64PtrAsU64(unsigned long long *p) { return reinterpret_cast<uint64_t>(p); }
uint64_t U32PtrAsU64(unsigned int *p) { return reinterpret_cast<uint64_t>(p); }
uint64_t U8PtrAsU64(unsigned char *p) { return reinterpret_cast<uint64_t>(p); }
unsigned long long *U64AsU64Ptr(uint64_t p) { return reinterpret_cast<unsigned long long*>(p); }
unsigned int *U64AsU32Ptr(uint64_t p) { return reinterpret_cast<unsigned int*>(p); }
unsigned char *U64AsU8Ptr(uint64_t p) { return reinterpret_cast<unsigned char*>(p); }

} // namespace xspcomm
