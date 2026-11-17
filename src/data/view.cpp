#include "xspcomm/xview.h"
#include "xspcomm/xport.h"
#include "xspcomm/detail/array_binding.h"
#include "xspcomm/detail/view_layout.h"
#include "fkyaml/node.hpp"
#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace xspcomm {
namespace detail {
namespace {

int64_t integer(const fkyaml::node &node, const char *key, int64_t low, int64_t high) {
    if (!node.contains(key) || !node[key].is_integer())
        throw std::invalid_argument(std::string("Layout requires integer ") + key);
    auto value = node[key].get_value<int64_t>();
    if (value < low || value > high)
        throw std::invalid_argument(std::string("Layout integer out of range: ") + key);
    return value;
}

std::shared_ptr<const Layout> parse(const fkyaml::node &value, unsigned depth = 0,
                                  const LayoutResolver &resolve = {}) {
    if (value.is_string() && resolve) return resolve(value.get_value<std::string>(), depth);
    if (depth >= 128 || !value.is_mapping())
        throw std::invalid_argument("Layout must be a bounded recursive type mapping");
    auto layout = std::make_shared<Layout>();
    layout->kind = value["kind"].get_value<std::string>();
    layout->packed = value["packed"].get_value<bool>();
    layout->width = uint32_t(integer(value, "width", 0, INT32_MAX));
    if (value.contains("signed")) layout->is_signed = value["signed"].get_value<bool>();
    if (value.contains("four_state")) layout->four_state = value["four_state"].get_value<bool>();
    if (layout->kind == "bits") {
        if (!layout->packed) throw std::invalid_argument("An XData leaf must be packed");
        if (value.contains("left") || value.contains("right")) {
            layout->left = int(integer(value, "left", INT32_MIN, INT32_MAX));
            layout->right = int(integer(value, "right", INT32_MIN, INT32_MAX));
            layout->has_range = true;
            if (layout->Size() != layout->Bits())
                throw std::invalid_argument("Bit-vector range does not match its width");
        }
    } else if (layout->kind == "array") {
        layout->left = int(integer(value, "left", INT32_MIN, INT32_MAX));
        layout->right = int(integer(value, "right", INT32_MIN, INT32_MAX));
        if (layout->Size() > uint64_t(INT32_MAX))
            throw std::invalid_argument("Array dimension exceeds int range");
        layout->children.push_back(parse(value["element"], depth + 1, resolve));
        const auto &element = *layout->children.front();
        if (layout->packed && (!element.packed ||
            layout->Size() * element.Bits() != layout->width))
            throw std::invalid_argument("Packed array element widths do not match its width");
    } else if (layout->kind == "struct") {
        const auto &fields = value["fields"];
        if (!fields.is_sequence() || fields.empty())
            throw std::invalid_argument("Struct layout requires fields");
        std::set<std::string> seen;
        std::vector<std::pair<uint64_t, uint64_t>> ranges;
        for (const auto &field : fields) {
            auto name = field["name"].get_value<std::string>();
            if (name.empty() || !seen.insert(name).second)
                throw std::invalid_argument("Struct field names must be nonempty and unique");
            auto child = parse(field["type"], depth + 1, resolve);
            uint32_t offset = 0;
            if (layout->packed) {
                offset = uint32_t(integer(field, "offset", 0, INT32_MAX));
                if (!child->packed || uint64_t(offset) + child->Bits() > layout->width)
                    throw std::invalid_argument("Packed struct field exceeds parent width");
                ranges.emplace_back(offset, offset + child->Bits());
            }
            layout->names.push_back(std::move(name));
            layout->offsets.push_back(offset);
            layout->children.push_back(std::move(child));
        }
        if (layout->packed) {
            std::sort(ranges.begin(), ranges.end());
            uint64_t end = 0;
            for (const auto &[first, last] : ranges) {
                if (first != end) throw std::invalid_argument("Packed struct fields overlap or leave gaps");
                end = last;
            }
            if (end != layout->width) throw std::invalid_argument("Packed struct width mismatch");
        }
    } else {
        throw std::invalid_argument("Unknown layout kind: " + layout->kind);
    }
    if (layout->kind != "bits" && layout->packed && !layout->width)
        throw std::invalid_argument("Packed aggregate width must be positive");
    if (!layout->packed && layout->width)
        throw std::invalid_argument("Unpacked container has no whole-value bit width");
    for (const auto &child : layout->children) layout->depth = std::max(layout->depth, child->depth + 1);
    if (depth + layout->depth > 128)
        throw std::invalid_argument("Layout must be a bounded recursive type mapping");
    return layout;
}

struct DataRef {
    XData *data;
    std::shared_ptr<XData> owner;
    uint32_t width;
    std::weak_ptr<const int> lifetime;
    explicit DataRef(XData &data) : data(&data), width(data.mWidth), lifetime(data.ObserveLifetime()) {}
    explicit DataRef(std::shared_ptr<XData> data) : DataRef(*data) { owner = std::move(data); }
    XData &Get() const {
        if (lifetime.expired()) throw std::runtime_error("View's XData has been destroyed");
        if (data->mWidth != width) throw std::runtime_error("View's XData width has changed");
        return *data;
    }
};

using Bindings = std::map<std::string, DataRef>;
using NativeBindings = std::map<std::string, std::shared_ptr<ArrayBinding>>;

void collect(XPort &port, const Layout &layout, const std::string &path, Bindings &bindings) {
    if (layout.packed) {
        if (!port.Has(path)) throw std::invalid_argument("Layout leaf missing from XPort: " + path);
        auto &data = port.Get(path);
        if ((data.mWidth ? data.mWidth : 1) != layout.Bits())
            throw std::invalid_argument("Layout leaf width does not match XData: " + path);
        bindings.emplace(path, DataRef(data));
    } else if (layout.kind == "struct") {
        for (size_t i = 0; i < layout.names.size(); ++i)
            collect(port, *layout.children[i], path + "." + layout.names[i], bindings);
    } else {
        const int step = layout.left <= layout.right ? 1 : -1;
        for (int64_t index = layout.left;; index += step) {
            collect(port, *layout.children.front(), path + "[" + std::to_string(index) + "]", bindings);
            if (index == layout.right) break;
        }
    }
}

} // namespace

std::shared_ptr<const Layout> ParseLayout(const std::string &description, const LayoutResolver &resolve, unsigned depth) {
    return parse(fkyaml::node::deserialize(description), depth, resolve);
}

struct ViewNode {
    std::shared_ptr<const Layout> layout;
    std::shared_ptr<const Bindings> bindings;
    std::string path;
    std::shared_ptr<DataRef> backing;
    uint32_t offset = 0;
    std::shared_ptr<XData> data;
    std::map<uint64_t, std::shared_ptr<ViewNode>> children;
    std::shared_ptr<ArrayBinding> indexed;
    uint64_t indexed_first = 0, indexed_count = 0;
    std::shared_ptr<const NativeBindings> native;
    std::string native_path;
    std::vector<int> native_indices;
    // True at an independently bound whole value; false within a packed value.
    bool whole_value = true;

    ViewNode(std::shared_ptr<const Layout> layout, std::shared_ptr<const Bindings> bindings,
             std::string path, std::shared_ptr<DataRef> backing = nullptr, uint32_t offset = 0,
              std::shared_ptr<ArrayBinding> indexed = nullptr, uint64_t first = 0, uint64_t count = 0,
              std::shared_ptr<const NativeBindings> native = nullptr, std::string native_path = "",
              std::vector<int> native_indices = {})
        : layout(std::move(layout)), bindings(std::move(bindings)), path(std::move(path)),
          backing(std::move(backing)), offset(offset), indexed(std::move(indexed)),
           indexed_first(first), indexed_count(count), native(std::move(native)),
           native_path(std::move(native_path)), native_indices(std::move(native_indices)) {
        if (this->layout->packed && this->native && !this->indexed) {
            this->indexed = this->native->at(this->native_path);
            // Scalar groups use a singleton dimension internally.
            this->indexed_first = this->native_indices.empty() ? 0 : this->indexed->FlatIndex(this->native_indices);
            this->indexed_count = 1;
        }
        if (this->layout->packed && !this->backing && !this->indexed)
            this->backing = std::make_shared<DataRef>(this->bindings->at(this->path));
    }

    void CheckAlive() const {
        if (indexed) { indexed->CheckBound(); return; }
        if (native) {
            for (const auto &[path, group] : *native) group->CheckBound();
            return;
        }
        auto check = [&](const DataRef &ref, const std::string &source) {
            try {
                // This checks parent/VPI lifetimes without sampling the signal
                // or requiring a writable backend. Read-only views are valid.
                (void)ref.Get()._check_writeable();
            } catch (const std::runtime_error &error) {
                throw std::runtime_error("View \"" + source + "\": " + error.what());
            }
        };
        if (backing) {
            check(*backing, path);
        } else {
            // Validate this subtree's existing bindings, not all sibling nodes.
            // No children, slices or backend callbacks are created here.
            for (auto it = bindings->lower_bound(path); it != bindings->end(); ++it) {
                const auto &source = it->first;
                if (source.compare(0, path.size(), path) != 0) break;
                if (source.size() == path.size() || source[path.size()] == '.' || source[path.size()] == '[')
                    check(it->second, source);
            }
        }
    }

    std::shared_ptr<XData> Data() {
        if (indexed) {
            auto leaf = indexed->AtFlat(indexed_first);
            if (!layout->packed) throw std::invalid_argument("Unpacked container has no whole XData value");
            if (uint64_t(offset) + layout->Bits() > (leaf->mWidth ? leaf->mWidth : 1))
                throw std::runtime_error("Array field exceeds its packed element: " + path);
            if (!data) {
                if (offset == 0 && (leaf->mWidth ? leaf->mWidth : 1) == layout->Bits() &&
                    (whole_value || leaf->type_layout == layout)) data = leaf;
                else data = leaf->SubDataRef(offset, layout->width, path);
                if (!whole_value || !data->type_layout) data->AssociateLayout(layout);
                if (data->mName.empty()) data->mName = path;
            }
            return data;
        }
        if (!backing) throw std::invalid_argument("Unpacked container has no whole XData value");
        auto &parent = backing->Get();
        if (!data) {
            if (offset == 0 && (parent.mWidth ? parent.mWidth : 1) == layout->Bits() &&
                (whole_value || parent.type_layout == layout))
                data = backing->owner ? backing->owner : std::shared_ptr<XData>(&parent, [](XData *) {});
            else data = parent.SubDataRef(offset, layout->width, path);
            if (!whole_value || !data->type_layout) data->AssociateLayout(layout);
        }
        return data;
    }

    std::shared_ptr<ViewNode> Child(uint64_t position) {
        auto found = children.find(position);
        if (found != children.end()) return found->second;
        std::shared_ptr<const Layout> child;
        std::string child_path;
        auto key = native_path;
        auto indices = native_indices;
        uint64_t child_offset = offset;
        if (layout->kind == "struct") {
            child = layout->children.at(position);
            child_path = path + "." + layout->names.at(position);
            if (layout->packed) child_offset += layout->offsets.at(position);
            else key += "." + layout->names.at(position);
        } else {
            child = layout->children.front();
            const auto index = int64_t(layout->left) + int64_t(position) * (layout->left <= layout->right ? 1 : -1);
            child_path = path + "[" + std::to_string(index) + "]";
            if (layout->packed) child_offset += (layout->Size() - position - 1) * child->Bits();
            else { key += "[]"; indices.push_back(int(index)); }
        }
        auto result = std::make_shared<ViewNode>(child, bindings, child_path,
            layout->packed ? backing : nullptr, uint32_t(child_offset), indexed,
            indexed ? indexed_first + (layout->packed ? 0 : position * (indexed_count / layout->Size())) : 0,
            indexed ? (layout->packed ? indexed_count : indexed_count / layout->Size()) : 0,
            native, std::move(key), std::move(indices));
        result->whole_value = !layout->packed;
        children.emplace(position, result);
        return result;
    }
};

} // namespace detail

XView::XView(std::shared_ptr<detail::ViewNode> node) : node(std::move(node)) {}

XView XView::FromData(XData &data, const std::string &description) {
    return FromDataLayout(data, detail::ParseLayout(description));
}

XView XView::FromDataLayout(XData &data, std::shared_ptr<const detail::Layout> layout) {
    if (!layout->packed || layout->Bits() != (data.mWidth ? data.mWidth : 1))
        throw std::invalid_argument("Packed layout width must match its parent XData");
    (void)data._check_writeable();
    if (data.type_layout == layout) {
        if (data.type_view && data.type_view->layout == layout) return XView(data.type_view);
    }
    auto root = std::make_shared<detail::ViewNode>(layout, nullptr, data.mName,
                                                  std::make_shared<detail::DataRef>(data));
    if (data.type_layout == layout) data.type_view = root;
    return XView(std::move(root));
}

XView XView::FromNative(uint64_t base, const std::string &description,
                       const std::vector<unsigned long long> &strides) {
    return FromNativeLayout(base, detail::ParseLayout(description), strides);
}

XView XView::FromNativeLayout(uint64_t base, std::shared_ptr<const detail::Layout> layout,
                            const std::vector<unsigned long long> &strides) {
    if (!base) throw std::invalid_argument("Native view base is null");
    auto value = layout;
    std::vector<int> left, right;
    while (!value->packed) {
        if (value->kind != "array")
            throw std::invalid_argument("Native unpacked structs require independent member bindings");
        left.push_back(value->left);
        right.push_back(value->right);
        value = value->children.front();
    }
    if (left.empty()) {
        if (!strides.empty()) throw std::invalid_argument("Packed native value cannot have array strides");
        const uint64_t bytes = value->Bits() <= 8 ? 1 : value->Bits() <= 16 ? 2 : ((value->Bits() + 31) / 32) * 4;
        if (base > UINT64_MAX - (bytes - 1)) throw std::overflow_error("Native view address overflow");
        auto data = std::make_shared<XData>(layout->width, IOType::InOut);
        data->AssociateLayout(layout);
        data->BindNativeData(base);
        data->AsImmWrite();
        auto root = std::make_shared<detail::ViewNode>(layout, nullptr, "",
                                                      std::make_shared<detail::DataRef>(data));
        return XView(std::move(root));
    }
    auto binding = std::make_shared<detail::ArrayBinding>(value->width, left, right, true);
    binding->BindNativeData(base, strides);
    return XView(std::make_shared<detail::ViewNode>(layout, nullptr, "", nullptr, 0,
                                                   binding, 0, binding->Size()));
}

XView XView::FromNativeBindings(uint64_t base, const std::string &description,
                               const std::string &bindings) {
    return FromNativeBindingsLayout(base, detail::ParseLayout(description), bindings);
}

XView XView::FromNativeBindingsLayout(uint64_t base, std::shared_ptr<const detail::Layout> layout,
                                    const std::string &bindings) {
    if (!base) throw std::invalid_argument("Native view base is null");
    using N = fkyaml::node;
    auto declarations = N::deserialize(bindings);
    if (!declarations.is_sequence()) throw std::invalid_argument("Native bindings must be a list");
    std::map<std::string, N> groups;
    for (const auto &group : declarations) {
        if (!groups.emplace(group["path"].get_value<std::string>(), group).second)
            throw std::invalid_argument("Duplicate native field binding");
    }
    auto native = std::make_shared<detail::NativeBindings>();
    std::function<void(const detail::Layout &, std::string, std::vector<int>, std::vector<int>)> collect;
    collect = [&](const detail::Layout &type, std::string path, std::vector<int> left, std::vector<int> right) {
        if (!type.packed) {
            if (type.kind == "array") {
                left.push_back(type.left); right.push_back(type.right);
                collect(*type.children.front(), path + "[]", left, right);
            } else for (size_t i = 0; i < type.names.size(); ++i)
                collect(*type.children[i], path + "." + type.names[i], left, right);
            return;
        }
        const auto found = groups.find(path);
        if (found == groups.end()) throw std::invalid_argument("Missing native field binding: " + path);
        const auto &group = found->second;
        const auto width = group["rtl_width"].get_value<uint64_t>();
        const auto bytes = width <= 8 ? 1 : width <= 16 ? 2 : ((width + 31) / 32) * 4;
        if (width != type.Bits() || group["mem_bytes"].get_value<uint64_t>() != bytes)
            throw std::invalid_argument("Native field width/storage size mismatch: " + path);
        const auto &dimensions = group["array_dimensions"];
        const auto &steps = group["array_strides"];
        if (!dimensions.is_sequence() || dimensions.size() != left.size() || !steps.is_sequence() || steps.size() != left.size())
            throw std::invalid_argument("Native field rank mismatch: " + path);
        std::vector<unsigned long long> strides;
        for (size_t d = 0; d < left.size(); ++d) {
            if (dimensions[d].get_value<int64_t>() != std::abs(int64_t(left[d]) - right[d]) + 1)
                throw std::invalid_argument("Native field shape mismatch: " + path);
            strides.push_back(steps[d].get_value<uint64_t>());
        }
        const auto offset = group["offset"].get_value<uint64_t>();
        if (base > UINT64_MAX - offset) throw std::overflow_error("Native field address overflow");
        if (left.empty()) { left.push_back(0); right.push_back(0); strides.push_back(bytes); }
        auto binding = std::make_shared<detail::ArrayBinding>(type.width, left, right, true);
        binding->BindNativeData(base + offset, strides);
        native->emplace(path, std::move(binding));
        groups.erase(found);
    };
    collect(*layout, "", {}, {});
    if (!groups.empty()) throw std::invalid_argument("Unexpected native field binding");
    return XView(std::make_shared<detail::ViewNode>(layout, nullptr, "", nullptr, 0, nullptr, 0, 0, native));
}

XView XView::FromPort(XPort &port, const std::string &name, const std::string &description) {
    return FromPortLayout(port, name, detail::ParseLayout(description));
}

XView XView::FromPortLayout(XPort &port, const std::string &name,
                          std::shared_ptr<const detail::Layout> layout) {
    auto bindings = std::make_shared<detail::Bindings>();
    detail::collect(port, *layout, name, *bindings);
    auto root = std::make_shared<detail::ViewNode>(layout, bindings, name);
    return XView(std::move(root));
}

XView XView::FromSignals(
    const std::map<std::string, std::shared_ptr<XData>> &signals,
    const std::string &name, const std::string &description) {
    XPort port;
    for (const auto &[path, data] : signals) {
        if (!data) throw std::invalid_argument("Null aggregate leaf: " + path);
        port.Add(path, *data);
    }
    auto layout = detail::parse(fkyaml::node::deserialize(description));
    auto bindings = std::make_shared<detail::Bindings>();
    detail::collect(port, *layout, name, *bindings);
    for (auto &[path, ref] : *bindings) ref.owner = signals.at(path);
    auto root = std::make_shared<detail::ViewNode>(layout, bindings, name);
    return XView(std::move(root));
}

void XView::Require(const char *kind) const {
    if (!node)
        throw std::invalid_argument(std::string("Cannot convert empty XView: requested=") + kind);
    if (*kind && node->layout->kind != kind)
        throw std::invalid_argument("Cannot convert view \"" + node->path + "\": actual=" +
                                    node->layout->kind + ", requested=" + kind);
    node->CheckAlive();
}

std::string XView::Kind() const { Require(""); return node->layout->kind; }
bool XView::IsPacked() const { Require(""); return node->layout->packed; }
bool XView::IsSigned() const { Require(""); return node->layout->is_signed; }
bool XView::IsFourState() const { Require(""); return node->layout->four_state; }
std::shared_ptr<XData> XView::Data() const { Require("bits"); return node->Data(); }
std::shared_ptr<XStruct> XView::Struct() const {
    Require("struct");
    return std::shared_ptr<XStruct>(new XStruct(node));
}
std::shared_ptr<XArray> XView::Array() const {
    Require("array");
    return std::shared_ptr<XArray>(new XArray(node));
}
std::shared_ptr<XData> XView::PackedData() const {
    Require("");
    if (!node->layout->packed)
        throw std::invalid_argument("View \"" + node->path + "\" is unpacked and has no whole XData value");
    return node->Data();
}
XView::operator std::shared_ptr<XData>() const { return Data(); }
XView::operator std::shared_ptr<XStruct>() const { return Struct(); }
XView::operator std::shared_ptr<XArray>() const { return Array(); }

std::shared_ptr<XStruct> XData::StructView(const std::string &layout) {
    return XView::FromData(*this, layout).Struct();
}
std::shared_ptr<XArray> XData::ArrayView(const std::string &layout) {
    return XView::FromData(*this, layout).Array();
}

void XData::AssociateLayout(std::shared_ptr<const detail::Layout> layout) {
    if (!layout || !layout->packed || layout->Bits() != (mWidth ? mWidth : 1))
        throw std::invalid_argument("XData layout must describe a packed value of the same width");
    if (type_layout != layout) {
        type_view.reset();
        type_layout = std::move(layout);
    }
}
void XData::SetLayout(const std::string &layout) { AssociateLayout(detail::ParseLayout(layout)); }
std::shared_ptr<XStruct> XData::StructView() {
    if (!type_layout) throw std::invalid_argument("XData has no RTL layout; provide a layout explicitly");
    if (type_layout->kind != "struct")
        throw std::invalid_argument("Cannot convert view \"" + mName + "\": actual=" +
            type_layout->kind + ", requested=struct");
    return XView::FromDataLayout(*this, type_layout).Struct();
}
std::shared_ptr<XArray> XData::ArrayView() {
    if (!type_layout) throw std::invalid_argument("XData has no RTL layout; provide a layout explicitly");
    // A plain vector remains a bits node by default. Only this explicit call
    // interprets its retained declaration as one array dimension.
    if (type_layout->kind == "bits" && type_layout->has_range) {
        if (type_view && type_view->layout->kind == "array") return XView(type_view).Array();
        auto bit = std::make_shared<detail::Layout>();
        bit->kind = "bits"; bit->packed = true; bit->four_state = type_layout->four_state;
        auto array = std::make_shared<detail::Layout>(*type_layout);
        array->kind = "array"; array->width = uint32_t(type_layout->Bits());
        array->children = {bit}; array->depth = 2;
        auto view = XView::FromDataLayout(*this, std::move(array));
        type_view = view.node;
        return view.Array();
    }
    return XView::FromDataLayout(*this, type_layout).Array();
}

XStruct::XStruct(std::shared_ptr<detail::ViewNode> node) : node(std::move(node)) {}
bool XStruct::IsPacked() const { return node->layout->packed; }
bool XStruct::Has(const std::string &name) const {
    const auto &names = node->layout->names;
    return std::find(names.begin(), names.end(), name) != names.end();
}
std::vector<std::string> XStruct::Names() const { return node->layout->names; }
XView XStruct::Field(const std::string &name) {
    const auto &names = node->layout->names;
    auto found = std::find(names.begin(), names.end(), name);
    if (found == names.end()) throw std::out_of_range("Struct field not found: " + name);
    return XView(node->Child(size_t(found - names.begin())));
}
std::shared_ptr<XData> XStruct::Data(const std::string &name) { return Field(name).PackedData(); }
std::shared_ptr<XStruct> XStruct::Struct(const std::string &name) { return Field(name).Struct(); }
std::shared_ptr<XArray> XStruct::Array(const std::string &name) { return Field(name).Array(); }
std::shared_ptr<XData> XStruct::PackedData() { return XView(node).PackedData(); }

XArray::XArray(std::shared_ptr<detail::ViewNode> node) : node(std::move(node)) {}
std::shared_ptr<XArray> XArray::FromBinding(std::shared_ptr<detail::ArrayBinding> binding,
    uint32_t width, const std::string &name, std::shared_ptr<const detail::Layout> element_layout) {
    auto left = binding->Left(), right = binding->Right();
    if (left.size() >= 128 || width > uint32_t(INT32_MAX))
        throw std::invalid_argument("Array rank or element width exceeds supported layout limits");
    if (!element_layout) {
        auto bits = std::make_shared<detail::Layout>();
        bits->kind = "bits"; bits->packed = true; bits->width = width;
        element_layout = std::move(bits);
    }
    if (!element_layout->packed || element_layout->Bits() != (width ? width : 1))
        throw std::invalid_argument("Array element layout width mismatch");
    auto layout = std::move(element_layout);
    for (size_t d = left.size(); d-- > 0;) {
        auto parent = std::make_shared<detail::Layout>();
        parent->kind = "array";
        parent->left = left[d]; parent->right = right[d];
        parent->children.push_back(layout);
        layout = std::move(parent);
    }
    auto root = std::make_shared<detail::ViewNode>(layout, nullptr, name, nullptr, 0,
                                                  binding, 0, binding->Size());
    return XView(std::move(root)).Array();
}
std::shared_ptr<XArray> XArray::FromDPI(uint32_t width, const std::vector<int> &left,
    const std::vector<int> &right, uint64_t read, uint64_t write) {
    auto binding = std::make_shared<detail::ArrayBinding>(width, left, right);
    binding->BindDPIPtr(read, write);
    return FromBinding(std::move(binding), width, "");
}
std::shared_ptr<XArray> XArray::FromNative(uint32_t width, const std::vector<int> &left,
    const std::vector<int> &right, uint64_t base, const std::vector<unsigned long long> &strides) {
    auto binding = std::make_shared<detail::ArrayBinding>(width, left, right);
    binding->BindNativeData(base, strides);
    return FromBinding(std::move(binding), width, "");
}
void XArray::BindElement(XData &element, uint64_t index) const {
    if (!node->indexed) throw std::logic_error("Array has no indexed binding");
    if (index >= node->indexed_count) throw std::out_of_range("Native array index out of range");
    node->indexed->BindElement(element, node->indexed_first + index);
}
bool XArray::IsPacked() const { return node->layout->packed; }
int XArray::Left() const { return node->layout->left; }
int XArray::Right() const { return node->layout->right; }
uint64_t XArray::Size() const { return node->layout->Size(); }
std::vector<int> XArray::Shape() const {
    std::vector<int> shape;
    auto layout = node->layout;
    while (layout->kind == "array" && layout->packed == IsPacked()) {
        shape.push_back(int(layout->Size()));
        layout = layout->children.front();
    }
    return shape;
}
std::vector<int> XArray::LeftBounds() const {
    std::vector<int> result;
    auto layout = node->layout;
    while (layout->kind == "array" && layout->packed == IsPacked()) {
        result.push_back(layout->left);
        layout = layout->children.front();
    }
    return result;
}
std::vector<int> XArray::RightBounds() const {
    std::vector<int> result;
    auto layout = node->layout;
    while (layout->kind == "array" && layout->packed == IsPacked()) {
        result.push_back(layout->right);
        layout = layout->children.front();
    }
    return result;
}
XView XArray::At(int index) {
    const auto position = (int64_t(index) - Left()) * (Left() <= Right() ? 1 : -1);
    if (position < 0 || uint64_t(position) >= Size())
        throw std::out_of_range("RTL array index out of range");
    return XView(node->Child(uint64_t(position)));
}
std::shared_ptr<XData> XArray::Data(int index) { return At(index).PackedData(); }
std::shared_ptr<XStruct> XArray::Struct(int index) { return At(index).Struct(); }
std::shared_ptr<XArray> XArray::Array(int index) { return At(index).Array(); }
std::shared_ptr<XData> XArray::PackedData() { return XView(node).PackedData(); }

} // namespace xspcomm
