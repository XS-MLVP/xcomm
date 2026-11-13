#include "xspcomm/xview.h"
#include "xspcomm/xport.h"
#include "fkyaml/node.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace xspcomm {
namespace detail {
namespace {

struct Layout {
    std::string kind;
    bool packed = false, is_signed = false, four_state = true;
    uint32_t width = 0;
    int left = 0, right = 0;
    std::vector<std::string> names;
    std::vector<uint32_t> offsets;
    std::vector<std::shared_ptr<const Layout>> children;
    uint64_t Bits() const { return width ? width : 1; }
    uint64_t Size() const { return uint64_t(std::abs(int64_t(left) - right)) + 1; }
};

int64_t integer(const fkyaml::node &node, const char *key, int64_t low, int64_t high) {
    if (!node.contains(key) || !node[key].is_integer())
        throw std::invalid_argument(std::string("Layout requires integer ") + key);
    auto value = node[key].get_value<int64_t>();
    if (value < low || value > high)
        throw std::invalid_argument(std::string("Layout integer out of range: ") + key);
    return value;
}

std::shared_ptr<const Layout> parse(const fkyaml::node &value, unsigned depth = 0) {
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
    } else if (layout->kind == "array") {
        layout->left = int(integer(value, "left", INT32_MIN, INT32_MAX));
        layout->right = int(integer(value, "right", INT32_MIN, INT32_MAX));
        if (layout->Size() > uint64_t(INT32_MAX))
            throw std::invalid_argument("Array dimension exceeds int range");
        layout->children.push_back(parse(value["element"], depth + 1));
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
            auto child = parse(field["type"], depth + 1);
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

void collect(XPort &port, const Layout &layout, const std::string &path, Bindings &bindings) {
    if (layout.packed) {
        if (!port.Has(path)) throw std::invalid_argument("Layout leaf missing from XPort: " + path);
        auto &data = port.Get(path);
        if (data.mWidth != layout.width)
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

struct ViewNode {
    std::shared_ptr<const Layout> layout;
    std::shared_ptr<const Bindings> bindings;
    std::string path;
    std::shared_ptr<DataRef> backing;
    uint32_t offset = 0;
    std::shared_ptr<XData> data;
    std::map<uint64_t, std::shared_ptr<ViewNode>> children;

    ViewNode(std::shared_ptr<const Layout> layout, std::shared_ptr<const Bindings> bindings,
             std::string path, std::shared_ptr<DataRef> backing = nullptr, uint32_t offset = 0)
        : layout(std::move(layout)), bindings(std::move(bindings)), path(std::move(path)),
          backing(std::move(backing)), offset(offset) {
        if (this->layout->packed && !this->backing)
            this->backing = std::make_shared<DataRef>(this->bindings->at(this->path));
    }

    void CheckAlive() const {
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
        if (!backing) throw std::invalid_argument("Unpacked container has no whole XData value");
        auto &parent = backing->Get();
        if (!data) {
            if (offset == 0 && parent.mWidth == layout->width)
                data = backing->owner ? backing->owner : std::shared_ptr<XData>(&parent, [](XData *) {});
            else data = parent.SubDataRef(offset, layout->width, path);
        }
        return data;
    }

    std::shared_ptr<ViewNode> Child(uint64_t position) {
        auto found = children.find(position);
        if (found != children.end()) return found->second;
        std::shared_ptr<const Layout> child;
        std::string child_path;
        uint64_t child_offset = offset;
        if (layout->kind == "struct") {
            child = layout->children.at(position);
            child_path = path + "." + layout->names.at(position);
            if (layout->packed) child_offset += layout->offsets.at(position);
        } else {
            child = layout->children.front();
            const auto index = int64_t(layout->left) + int64_t(position) * (layout->left <= layout->right ? 1 : -1);
            child_path = path + "[" + std::to_string(index) + "]";
            if (layout->packed) child_offset += (layout->Size() - position - 1) * child->Bits();
        }
        auto result = std::make_shared<ViewNode>(child, bindings, child_path,
            layout->packed ? backing : nullptr, uint32_t(child_offset));
        children.emplace(position, result);
        return result;
    }
};

} // namespace detail

XView::XView(std::shared_ptr<detail::ViewNode> node) : node(std::move(node)) {}

XView XView::FromData(XData &data, const std::string &description) {
    auto layout = detail::parse(fkyaml::node::deserialize(description));
    if (!layout->packed || layout->width != data.mWidth)
        throw std::invalid_argument("Packed layout width must match its parent XData");
    auto root = std::make_shared<detail::ViewNode>(layout, nullptr, data.mName,
                                                  std::make_shared<detail::DataRef>(data));
    return XView(std::move(root));
}

XView XView::FromPort(XPort &port, const std::string &name, const std::string &description) {
    auto layout = detail::parse(fkyaml::node::deserialize(description));
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
