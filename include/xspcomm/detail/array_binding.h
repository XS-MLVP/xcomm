#ifndef XSPCOMM_DETAIL_ARRAY_BINDING_H
#define XSPCOMM_DETAIL_ARRAY_BINDING_H

#include "xspcomm/xdata.h"
#include <map>

namespace xspcomm::detail {
struct ArrayStorage;

// Indexed backend state, not an array in the public signal model. Picker uses
// this C++ implementation contract to bind existing leaves without closures.
class ArrayBinding {
    std::shared_ptr<ArrayStorage> storage;
    std::map<uint64_t, std::shared_ptr<XData>> elements;
public:
    ArrayBinding(uint32_t width, const std::vector<int> &left, const std::vector<int> &right);
    uint64_t Size() const;
    std::vector<int> Left() const;
    std::vector<int> Right() const;
    std::vector<int> Shape() const;
    uint64_t FlatIndex(const std::vector<int> &indices) const;
    std::vector<int> Indices(uint64_t index) const;
    void CheckBound() const;
    void BindDPIPtr(uint64_t read, uint64_t write);
    void BindNativeData(uint64_t base, const std::vector<unsigned long long> &strides = {});
    uint64_t Address(uint64_t index) const;
    std::vector<unsigned long long> Strides() const;
    void BindElement(XData &element, uint64_t index) const;
    std::shared_ptr<XData> AtFlat(uint64_t index);
    std::shared_ptr<XData> At(const std::vector<int> &indices);
};
} // namespace xspcomm::detail
#endif
