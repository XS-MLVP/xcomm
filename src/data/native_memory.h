#ifndef XSPCOMM_DATA_NATIVE_MEMORY_H
#define XSPCOMM_DATA_NATIVE_MEMORY_H

#include "xspcomm/xdata.h"
#include "xspcomm/common/memory_access.h"
#include <type_traits>
#include <utility>

namespace xspcomm::detail {
// Count == 0 uses a runtime loop; fixed counts are expanded at compile time.
template<class T, size_t Count = 0>
struct VecMemoryIO {
    static_assert(std::is_same_v<T, uint8_t> ||
                  std::is_same_v<T, uint16_t> ||
                  std::is_same_v<T, uint32_t>);

    template<size_t... I>
    static void ReadFixed(xsvLogicVecVal* dst, const unsigned char* src,
                          std::index_sequence<I...>) {
        ((dst[I].aval = static_cast<uint32_t>(memory::Load<T>(src + I * sizeof(T)))), ...);
    }

    template<size_t... I>
    static void WriteFixed(unsigned char* dst, const xsvLogicVecVal* src,
                           std::index_sequence<I...>) {
        (memory::Store<T>(dst + I * sizeof(T), static_cast<T>(src[I].aval)), ...);
    }

    static void Read(xsvLogicVecVal* dst, const void* src, size_t count) {
        const auto* bytes = static_cast<const unsigned char*>(src);
        const size_t words = sizeof(T) < sizeof(uint32_t) ? 1 : count;
        if constexpr (Count != 0) {
            // ReInit preserves callbacks, so the current width can differ.
            if (likely(words == Count)) {
                ReadFixed(dst, bytes, std::make_index_sequence<Count>{});
                return;
            }
        }
        for (size_t i = 0; i < words; ++i)
            dst[i].aval = static_cast<uint32_t>(memory::Load<T>(bytes + i * sizeof(T)));
    }

    static void Write(void* dst, const xsvLogicVecVal* src, size_t count) {
        auto* bytes = static_cast<unsigned char*>(dst);
        const size_t words = sizeof(T) < sizeof(uint32_t) ? 1 : count;
        if constexpr (Count != 0) {
            if (likely(words == Count)) {
                WriteFixed(bytes, src, std::make_index_sequence<Count>{});
                return;
            }
        }
        for (size_t i = 0; i < words; ++i)
            memory::Store<T>(bytes + i * sizeof(T), static_cast<T>(src[i].aval));
    }

    using VecCallback = xfunction<void, xsvLogicVecVal*>;
    static void Bind(uint64_t address, const uint32_t& count,
                     VecCallback& read, VecCallback& write) {
        read = [address, &count](xsvLogicVecVal* dst) {
            Read(dst, reinterpret_cast<const void*>(address), count);
        };
        write = [address, &count](const xsvLogicVecVal* src) {
            Write(reinterpret_cast<void*>(address), src, count);
        };
    }
};
} // namespace xspcomm::detail


#endif
