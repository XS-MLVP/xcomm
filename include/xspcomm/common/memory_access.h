#ifndef XSPCOMM_COMMON_MEMORY_ACCESS_H
#define XSPCOMM_COMMON_MEMORY_ACCESS_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace xspcomm::memory {

// Native storage access without alignment or strict-aliasing assumptions.
template<class T>
T Load(const void* source) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value;
    std::memcpy(&value, source, sizeof(T));
    return value;
}

template<class T>
void Store(void* destination, T value) {
    static_assert(std::is_trivially_copyable_v<T>);
    std::memcpy(destination, &value, sizeof(T));
}

template <size_t Bytes> struct UIntBytes;

template <typename T>
struct UIntWord {
    static uint64_t Load(const unsigned char* data) {
        return memory::Load<T>(data);
    }
};

template <size_t LowBytes, size_t HighBytes>
struct UIntParts {
    static uint64_t Load(const unsigned char* data) {
        return UIntBytes<LowBytes>::Load(data) |
               (UIntBytes<HighBytes>::Load(data + LowBytes) << (LowBytes * 8));
    }
};

// Fixed byte widths use native integers or consecutive low/high parts.
#define XSPCOMM_UINT_BYTES(Bytes, ...) \
    template <> struct UIntBytes<Bytes> : __VA_ARGS__ {};
XSPCOMM_UINT_BYTES(1, UIntWord<uint8_t>)
XSPCOMM_UINT_BYTES(2, UIntWord<uint16_t>)
XSPCOMM_UINT_BYTES(3, UIntParts<2, 1>)
XSPCOMM_UINT_BYTES(4, UIntWord<uint32_t>)
XSPCOMM_UINT_BYTES(5, UIntParts<4, 1>)
XSPCOMM_UINT_BYTES(6, UIntParts<4, 2>)
XSPCOMM_UINT_BYTES(7, UIntParts<6, 1>)
XSPCOMM_UINT_BYTES(8, UIntWord<uint64_t>)
#undef XSPCOMM_UINT_BYTES

// A non-owning reader for little-endian buffers, including a partial last word.
struct ByteWords {
    const unsigned char* data;
    size_t bytes;

    size_t Count() const { return bytes / 4 + (bytes % 4 != 0); }

    uint32_t operator()(size_t i) const {
        const size_t offset = i * 4;
        const size_t count = std::min(sizeof(uint32_t), bytes - offset);
        const auto* word = data + offset;
        if (count == sizeof(uint32_t)) return UIntBytes<4>::Load(word);
        switch (count) {
        case 1: return UIntBytes<1>::Load(word);
        case 2: return UIntBytes<2>::Load(word);
        default: return UIntBytes<3>::Load(word);
        }
    }
};

} // namespace xspcomm::memory

#endif
