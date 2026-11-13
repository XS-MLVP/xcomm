#ifndef XSPCOMM_DATA_ARRAY_STORAGE_H
#define XSPCOMM_DATA_ARRAY_STORAGE_H

#include "xspcomm/detail/array_binding.h"

namespace xspcomm::detail {

struct ArrayStorage {
    uint32_t width;
    std::vector<int> left, right, shape;
    std::vector<unsigned long long> strides;
    uint64_t size = 1, base = 0, getter = 0, setter = 0;
    XDataBackendKind kind = XDataBackendKind::Unknown;

    ArrayStorage(uint32_t width, const std::vector<int> &left, const std::vector<int> &right);
    void CheckIndex(uint64_t index) const;
    uint64_t Address(uint64_t index) const;
    // Locations are resolved once at bind time: DPI index or native address.
    void Read(uint64_t location, void *value) const;
    void Write(uint64_t location, const void *value) const;
};

} // namespace xspcomm::detail
#endif
