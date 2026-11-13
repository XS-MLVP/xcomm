#include "xspcomm/xvpi_view.h"
#include "data/vpi_storage.h"
#include "fkyaml/node.hpp"
#include <climits>
#include <functional>
#include <set>

namespace xspcomm {
namespace {
using N = fkyaml::node;
using Context = detail::VpiContext;
using Handle = std::shared_ptr<detail::VpiOwnedHandle>;

bool is_array(int type) { return type == vpiRegArray || type == vpiNetArray || type == vpiMemory; }
bool is_struct(int type) { return type == vpiStructVar || type == vpiStructNet; }

struct Iterator {
    Context &context;
    vpiHandle value;
    Iterator(Context &context, int type, vpiHandle object)
        : context(context), value(context.iterate(type, object)) {}
    ~Iterator() { if (value) context.release(value); }
    vpiHandle Next() {
        if (!value) return nullptr;
        auto next = context.scan(value);
        if (!next) value = nullptr; // Exhaustion releases the VPI iterator.
        return next;
    }
};

std::pair<int, int> bounds(Context &c, vpiHandle object) {
    auto left = c.Own(c.handle(vpiLeftRange, object));
    auto right = c.Own(c.handle(vpiRightRange, object));
    s_vpi_value value{};
    value.format = vpiIntVal;
    c.get_value(left->value, &value);
    const int l = value.value.integer;
    c.get_value(right->value, &value);
    return {l, value.value.integer};
}

N discover(Context &c, vpiHandle object, unsigned depth = 0) {
    if (depth >= 128) throw std::invalid_argument("VPI aggregate depth exceeds 128");
    const auto type = c.get(vpiType, object);
    if (is_array(type)) {
        auto [left, right] = bounds(c, object);
        auto first = c.Own(c.by_index(object, left));
        return N{{"kind", "array"}, {"packed", false}, {"width", 0}, {"left", left}, {"right", right},
                 {"element", discover(c, first->value, depth + 1)}};
    }
    if (is_struct(type)) {
        if (c.get(vpiPacked, object) != 0)
            throw std::runtime_error("Packed VPI struct requires RTL field offsets from Picker");
        std::vector<N> fields;
        Iterator members(c, vpiMember, object);
        while (auto raw = members.Next()) {
            auto member = c.Own(raw);
            const auto text = c.get_str(vpiName, raw);
            if (!text) throw std::runtime_error("VPI member has no field name");
            const std::string name = text;
            fields.emplace_back(N{{"name", name}, {"type", discover(c, raw, depth + 1)}});
        }
        if (fields.empty()) throw std::runtime_error("Simulator does not expose unpacked VPI struct members");
        return N{{"kind", "struct"}, {"packed", false}, {"width", 0}, {"fields", fields}};
    }
    if (!detail::VpiLeafType(type)) throw std::runtime_error("Unsupported VPI object type: " + std::to_string(type));
    const int size = c.get(vpiSize, object);
    if (size <= 0) throw std::runtime_error("VPI value has no positive bit width");
    N result{{"kind", "bits"}, {"packed", true}, {"width", size == 1 ? 0 : size},
             {"signed", c.get(vpiSigned, object) == 1}, {"four_state", type != vpiBitVar}};
    if (c.get(vpiScalar, object) == 1) return result;
    std::vector<std::pair<int, int>> ranges;
    Iterator iter(c, vpiRange, object);
    while (auto raw = iter.Next()) {
        auto range = c.Own(raw);
        ranges.push_back(bounds(c, raw));
    }
    if (ranges.empty()) return result;
    result["width"] = 0;
    uint64_t bits = 1;
    for (auto it = ranges.rbegin(); it != ranges.rend(); ++it) {
        auto [left, right] = *it;
        const auto count = uint64_t(std::abs(int64_t(left) - right)) + 1;
        if (bits > uint64_t(INT_MAX) / count) throw std::invalid_argument("VPI packed dimensions overflow");
        bits *= count;
        result = N{{"kind", "array"}, {"packed", true}, {"width", bits}, {"left", left}, {"right", right},
                   {"signed", c.get(vpiSigned, object) == 1}, {"four_state", type != vpiBitVar}, {"element", result}};
    }
    if (bits != uint64_t(size)) throw std::runtime_error("VPI dimensions do not match whole packed width");
    return result;
}

// Extract the independent packed values. Packed subfields never get their own
// VPI handles/backends: XStruct/XArray slice the complete backing XData.
void bind_handles(Context &c, const N &layout, Handle object, const std::string &path,
                  std::map<std::string, std::pair<Handle, uint32_t>> &leaves, unsigned depth = 0) {
    if (depth >= 128) throw std::invalid_argument("VPI aggregate depth exceeds 128");
    const auto type = c.get(vpiType, object->value);
    if (layout["packed"].get_value<bool>()) {
        const auto width = layout["width"].get_value<int64_t>();
        if (width < 0 || width > INT_MAX) throw std::invalid_argument("Invalid RTL packed width");
        const bool packed_struct = is_struct(type) && c.get(vpiPacked, object->value) == 1;
        if ((!detail::VpiLeafType(type) && !packed_struct) ||
            c.get(vpiSize, object->value) != (width ? width : 1))
            throw std::runtime_error("VPI packed type/width does not match RTL layout: " + path);
        leaves.emplace(path, std::make_pair(std::move(object), uint32_t(width)));
    } else if (layout["kind"].get_value<std::string>() == "array") {
        if (!is_array(type)) throw std::runtime_error("VPI object is not an unpacked array: " + path);
        const auto [left, right] = bounds(c, object->value);
        if (left != layout["left"].get_value<int>() || right != layout["right"].get_value<int>())
            throw std::runtime_error("VPI array bounds do not match RTL layout: " + path);
        if (uint64_t(std::abs(int64_t(left) - right)) + 1 > uint64_t(INT_MAX))
            throw std::invalid_argument("VPI array dimension exceeds int range");
        for (int64_t index = left;; index += left <= right ? 1 : -1) {
            auto element = c.Own(c.by_index(object->value, PLI_INT32(index)));
            bind_handles(c, layout["element"], element, path + "[" + std::to_string(index) + "]", leaves, depth + 1);
            if (index == right) break;
        }
    } else {
        for (const auto &field : layout["fields"]) {
            std::string name = field["name"].get_value<std::string>();
            auto raw = is_struct(type) ? c.by_name(name.data(), object->value) : nullptr;
            if (!raw) {
                // Some simulators (including Verilator 5.048) expose an RTL
                // struct's declared member paths without identifying or
                // iterating the parent as a VPI struct. This remains safe:
                // every leaf is resolved by name and its VPI type/width and
                // array bounds are checked against Picker's RTL layout.
                const auto full = c.scope.empty() ? path + "." + name : c.scope + "." + path + "." + name;
                auto mutable_full = full;
                raw = c.by_name(mutable_full.data(), nullptr);
            }
            if (!raw) {
                const auto reason = is_struct(type) ? "does not expose VPI struct member: "
                                                    : "does not identify the VPI object as a struct or expose its RTL-declared member: ";
                throw std::runtime_error(std::string("Simulator ") + reason + path + "." + name);
            }
            bind_handles(c, field["type"], c.Own(raw), path + "." + name, leaves, depth + 1);
        }
    }
}

XView select(XView view, std::string suffix) {
    size_t position = 0;
    while (position < suffix.size()) {
        if (suffix[position] == '.') {
            const auto start = ++position;
            while (position < suffix.size() && suffix[position] != '.' && suffix[position] != '[') ++position;
            view = view.Struct()->Field(suffix.substr(start, position - start));
        } else if (suffix[position] == '[') {
            const auto close = suffix.find(']', position);
            if (close == std::string::npos) throw std::invalid_argument("Incomplete RTL array index");
            size_t used = 0;
            const auto text = suffix.substr(position + 1, close - position - 1);
            const auto value = std::stoll(text, &used);
            if (used != text.size() || value < INT_MIN || value > INT_MAX)
                throw std::invalid_argument("Invalid RTL array index");
            view = view.Array()->At(int(value));
            position = close + 1;
        } else throw std::invalid_argument("Invalid aggregate selection");
    }
    return view;
}
} // namespace

XVPI::XVPI() = default;
XVPI::~XVPI() { Close(); }
bool XVPI::IsOpen() const { return context && context->open; }
void XVPI::Close() {
    if (context) context->Close();
    views.clear();
}
void XVPI::Configure(uint64_t get, uint64_t get_value, uint64_t put_value,
                     uint64_t by_name, uint64_t by_index, uint64_t handle,
                     uint64_t iterate, uint64_t scan, uint64_t get_str,
                     uint64_t release, const std::string &scope,
                     const std::string &rtl_layouts) {
    if (context) throw std::logic_error("VPI session is already configured");
    const uint64_t functions[] = {get, get_value, put_value, by_name, by_index,
                                  handle, iterate, scan, get_str, release};
    for (auto address : functions) if (!address) throw std::runtime_error("Required VPI function is unavailable; enable --vpi");
    auto c = std::make_shared<Context>();
    c->get = reinterpret_cast<func_vpi_get>(get);
    c->get_value = reinterpret_cast<func_vpi_get_value>(get_value);
    c->put_value = reinterpret_cast<func_vpi_put_value>(put_value);
    c->by_name = reinterpret_cast<decltype(c->by_name)>(by_name);
    c->by_index = reinterpret_cast<decltype(c->by_index)>(by_index);
    c->handle = reinterpret_cast<decltype(c->handle)>(handle);
    c->iterate = reinterpret_cast<decltype(c->iterate)>(iterate);
    c->scan = reinterpret_cast<decltype(c->scan)>(scan);
    c->get_str = reinterpret_cast<func_vpi_get_str>(get_str);
    c->release = reinterpret_cast<decltype(c->release)>(release);
    c->scope = scope;
    const auto registry = N::deserialize(rtl_layouts);
    if (!registry.is_mapping()) throw std::invalid_argument("VPI RTL layouts must be a mapping");
    for (const auto &entry : registry.map_items())
        layouts.emplace(entry.key().get_value<std::string>(), N::serialize(entry.value()));
    context = std::move(c);
}

XView XVPI::Get(const std::string &name) {
    if (!context) throw std::runtime_error("VPI session is not configured; enable --vpi");
    context->Check();
    std::string path = name;
    auto strip_prefix = [&](const std::string &prefix) {
        if (!prefix.empty() && path.compare(0, prefix.size(), prefix) == 0 &&
            path.size() > prefix.size() && path[prefix.size()] == '.')
            path.erase(0, prefix.size() + 1);
    };
    strip_prefix(context->scope);
    if (context->scope.compare(0, 4, "TOP.") == 0)
        strip_prefix(context->scope.substr(4)); // Verilator's list omits TOP.
    if (auto found = views.find(path); found != views.end()) return found->second;
    // Prefer the closest RTL root, including packed fields which have no VPI
    // member reflection in Verilator. Reuse its already bound/cached leaves.
    std::string parent;
    for (const auto &[root_path, _] : layouts) {
        if (root_path.size() < path.size() && root_path.size() > parent.size() && path.compare(0, root_path.size(), root_path) == 0 &&
            (path[root_path.size()] == '.' || path[root_path.size()] == '[')) parent = root_path;
    }
    if (!parent.empty()) {
        auto result = select(Get(parent), path.substr(parent.size()));
        views.emplace(path, result);
        return result;
    }
    std::string full = context->scope.empty() ? path : context->scope + "." + path;
    auto raw = context->by_name(full.data(), nullptr);
    const auto declared = layouts.find(path);
    if (!raw) {
        if (declared == layouts.end()) return {};
        throw std::runtime_error("Simulator did not expose VPI object from RTL layout: " + path);
    }
    auto object = context->Own(raw);
    const N layout = declared == layouts.end() ? discover(*context, raw) : N::deserialize(declared->second);
    std::map<std::string, std::pair<Handle, uint32_t>> handles;
    bind_handles(*context, layout, object, path, handles);
    std::map<std::string, std::shared_ptr<XData>> signals;
    for (auto &[path, entry] : handles) {
        auto data = std::make_shared<XData>(entry.second, IOType::Input, "VPI:" + path);
        if (!data->BindVPI(entry.first->value, context->get, context->get_value, context->put_value, "VPI:" + path))
            throw std::runtime_error("Cannot bind VPI value: " + path);
        data->vpi_owner = entry.first;
        signals.emplace(path, std::move(data));
    }
    auto result = XView::FromSignals(signals, path, N::serialize(layout));
    views.emplace(path, result);
    return result;
}
} // namespace xspcomm
