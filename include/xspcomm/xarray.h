#ifndef XSPCOMM_XARRAY_H
#define XSPCOMM_XARRAY_H

#include "xspcomm/xdata.h"
#include <map>

namespace xspcomm {

// Fixed unpacked array metadata and a shared indexed backend. Leaves stay XData.
class XDataArray {
    std::shared_ptr<detail::XDataArrayStorage> storage;
    std::map<uint64_t, std::shared_ptr<XData>> elements;
public:
    XDataArray(uint32_t width, const std::vector<int> &left, const std::vector<int> &right);
    uint64_t Size() const;
    std::vector<int> Left() const;
    std::vector<int> Right() const;
    std::vector<int> Shape() const;
    uint64_t FlatIndex(const std::vector<int> &indices) const;
    std::vector<int> Indices(uint64_t index) const;

    // DPI ABI: void get(int, scalar*/vector*); void set(int, scalar/vector*).
    void BindDPIPtr(uint64_t read, uint64_t write);
    // Base is the element at the minimum RTL index in every dimension. Strides
    // are in bytes, outermost first; an empty vector requests contiguous storage.
    void BindNativeData(uint64_t base, const std::vector<unsigned long long> &strides = {});
    uint64_t Address(uint64_t index) const;
    std::vector<unsigned long long> Strides() const;

    // Borrow an existing leaf, or lazily create/cache a leaf owned by this view.
    // Leaves retain shared backend state, but not the borrowed model/library.
    void BindElement(XData &element, uint64_t index) const;
    std::shared_ptr<XData> AtFlat(uint64_t index);
    std::shared_ptr<XData> At(const std::vector<int> &indices);
};

} // namespace xspcomm
#endif
