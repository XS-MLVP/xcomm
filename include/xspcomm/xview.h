#ifndef XSPCOMM_XVIEW_H
#define XSPCOMM_XVIEW_H

#include "xspcomm/xdata.h"
#include <memory>
#include <map>
#include <string>
#include <vector>

namespace xspcomm {

class XPort;
class XStruct;
class XArray;
namespace detail { struct ViewNode; class ArrayBinding; }
class XSignalCFG;

// A lightweight value handle for a dynamically selected signal or container.
// Copies share the original node, never copy data or register callbacks.
class XView {
    std::shared_ptr<detail::ViewNode> node;
    explicit XView(std::shared_ptr<detail::ViewNode> node);
    void Require(const char *kind) const;
    friend class XStruct;
    friend class XArray;
public:
    XView() = default;
    // Layout is the recursive JSON type description emitted by Picker.
    static XView FromData(XData &data, const std::string &layout);
    static XView FromPort(XPort &port, const std::string &name, const std::string &layout);
#ifndef SWIG
    // Backend factories retain independent leaves for the lifetime of the tree.
    static XView FromSignals(
        const std::map<std::string, std::shared_ptr<XData>> &signals,
        const std::string &name, const std::string &layout);
    // A typed destination checks the runtime kind before accepting the handle.
    // Keep shared ownership/identity; do not implicitly flatten packed objects.
    operator std::shared_ptr<XData>() const;
    operator std::shared_ptr<XStruct>() const;
    operator std::shared_ptr<XArray>() const;
    explicit operator bool() const { return HasValue(); }
#endif
    // Presence is separate from source liveness, which conversions also check.
    bool HasValue() const { return bool(node); }
    std::string Kind() const; // "bits", "struct", or "array"; rejects an empty handle
    bool IsPacked() const;
    bool IsSigned() const;
    bool IsFourState() const;
    std::shared_ptr<XData> Data() const; // only a bits node
    std::shared_ptr<XStruct> Struct() const;
    std::shared_ptr<XArray> Array() const;
    std::shared_ptr<XData> PackedData() const; // explicit whole-value access
};

// Independent, recursively named struct container. Packed structs refer to one
// XData; unpacked structs refer to independent fields and have no whole value.
class XStruct {
    std::shared_ptr<detail::ViewNode> node;
    explicit XStruct(std::shared_ptr<detail::ViewNode> node);
    friend class XView;
public:
    bool IsPacked() const;
    bool Has(const std::string &name) const;
    std::vector<std::string> Names() const; // RTL declaration order
    XView Field(const std::string &name);
    std::shared_ptr<XData> Data(const std::string &name);
    std::shared_ptr<XStruct> Struct(const std::string &name);
    std::shared_ptr<XArray> Array(const std::string &name);
    std::shared_ptr<XData> PackedData();
};

// One declared array dimension. The element can be XData, XStruct or another
// XArray; packed and unpacked arrays use the same checked RTL indexing.
class XArray {
    std::shared_ptr<detail::ViewNode> node;
    explicit XArray(std::shared_ptr<detail::ViewNode> node);
    friend class XView;
    friend class XSignalCFG;
    static std::shared_ptr<XArray> FromBinding(std::shared_ptr<detail::ArrayBinding> binding,
                                             uint32_t width, const std::string &name);
    void BindElement(XData &element, uint64_t index) const;
public:
    // Homogeneous unpacked arrays use the same recursive interface as aggregate
    // arrays. The model memory / DPI library must outlive all resulting leaves.
    static std::shared_ptr<XArray> FromDPI(uint32_t width, const std::vector<int> &left,
        const std::vector<int> &right, uint64_t read, uint64_t write);
    static std::shared_ptr<XArray> FromNative(uint32_t width, const std::vector<int> &left,
        const std::vector<int> &right, uint64_t base,
        const std::vector<unsigned long long> &strides = {});
    bool IsPacked() const;
    int Left() const;
    int Right() const;
    uint64_t Size() const;
    std::vector<int> Shape() const;
    std::vector<int> LeftBounds() const;
    std::vector<int> RightBounds() const;
    XView At(int index);
    std::shared_ptr<XData> Data(int index);
    std::shared_ptr<XStruct> Struct(int index);
    std::shared_ptr<XArray> Array(int index);
    std::shared_ptr<XData> PackedData();
};

} // namespace xspcomm

#endif
