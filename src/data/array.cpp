#include "data/array_storage.h"
#include "data/native_memory.h"
#include <limits>
#include <stdexcept>

namespace xspcomm {
namespace {
uint64_t checked_add(uint64_t a, uint64_t b) {
    if (a > UINT64_MAX - b) throw std::overflow_error("Array address overflow");
    return a + b;
}
uint64_t checked_multiply(uint64_t a, uint64_t b) {
    if (b && a > UINT64_MAX / b) throw std::overflow_error("Array size/stride overflow");
    return a * b;
}
uint64_t native_bytes(uint32_t width) {
    return width <= 8 ? 1 : width <= 16 ? 2 : ((uint64_t(width) + 31) / 32) * 4;
}
}

detail::ArrayStorage::ArrayStorage(uint32_t width, const std::vector<int> &left,
                                   const std::vector<int> &right)
    : width(width), left(left), right(right) {
    if (left.empty() || left.size() != right.size())
        throw std::invalid_argument("Array bounds must have the same nonzero rank");
    for (size_t d = 0; d < left.size(); ++d) {
        const auto length = std::abs(int64_t(left[d]) - right[d]) + 1;
        if (length > std::numeric_limits<int>::max())
            throw std::invalid_argument("Array dimension exceeds int range");
        shape.push_back(int(length));
        size = checked_multiply(size, uint64_t(length));
    }
}

void detail::ArrayStorage::CheckIndex(uint64_t index) const {
    if (index >= size) throw std::out_of_range("Array element index out of range");
}

uint64_t detail::ArrayStorage::Address(uint64_t index) const {
    CheckIndex(index);
    if (kind != XDataBackendKind::MemDirect)
        throw std::logic_error("Array has no native storage");
    uint64_t offset = 0;
    for (size_t d = shape.size(); d-- > 0;) {
        auto position = index % uint64_t(shape[d]);
        index /= uint64_t(shape[d]);
        if (left[d] > right[d]) position = uint64_t(shape[d]) - 1 - position;
        offset += position * strides[d]; // Validated, including the maximum extent, at bind time.
    }
    return base + offset;
}

void detail::ArrayStorage::Read(uint64_t location, void *value) const {
    if (kind == XDataBackendKind::DPI) {
        reinterpret_cast<void (*)(int, void *)>(getter)(int(location), value);
        return;
    }
    const auto *address = reinterpret_cast<const void *>(location);
    if (width == 0) *static_cast<xsvLogic *>(value) = memory::Load<xsvLogic>(address);
    else if (width <= 8) VecMemoryIO<uint8_t>::Read(static_cast<xsvLogicVecVal *>(value), address, 1);
    else if (width <= 16) VecMemoryIO<uint16_t>::Read(static_cast<xsvLogicVecVal *>(value), address, 1);
    else VecMemoryIO<uint32_t>::Read(static_cast<xsvLogicVecVal *>(value), address, (uint64_t(width) + 31) / 32);
}

void detail::ArrayStorage::Write(uint64_t location, const void *value) const {
    if (kind == XDataBackendKind::DPI) {
        if (!setter) return;
        if (width == 0) reinterpret_cast<void (*)(int, xsvLogic)>(setter)(int(location), *static_cast<const xsvLogic *>(value));
        else reinterpret_cast<void (*)(int, const void *)>(setter)(int(location), value);
        return;
    }
    auto *address = reinterpret_cast<void *>(location);
    if (width == 0) memory::Store<xsvLogic>(address, *static_cast<const xsvLogic *>(value));
    else if (width <= 8) VecMemoryIO<uint8_t>::Write(address, static_cast<const xsvLogicVecVal *>(value), 1);
    else if (width <= 16) VecMemoryIO<uint16_t>::Write(address, static_cast<const xsvLogicVecVal *>(value), 1);
    else VecMemoryIO<uint32_t>::Write(address, static_cast<const xsvLogicVecVal *>(value), (uint64_t(width) + 31) / 32);
}

namespace detail {
ArrayBinding::ArrayBinding(uint32_t width, const std::vector<int> &left, const std::vector<int> &right)
    : storage(std::make_shared<ArrayStorage>(width, left, right)) {}

uint64_t ArrayBinding::Size() const { return storage->size; }
std::vector<int> ArrayBinding::Left() const { return storage->left; }
std::vector<int> ArrayBinding::Right() const { return storage->right; }
std::vector<int> ArrayBinding::Shape() const { return storage->shape; }
std::vector<unsigned long long> ArrayBinding::Strides() const { return storage->strides; }
void ArrayBinding::CheckBound() const {
    if (storage->kind == XDataBackendKind::Unknown) throw std::logic_error("Array backend is not bound");
}

uint64_t ArrayBinding::FlatIndex(const std::vector<int> &indices) const {
    if (indices.size() != storage->shape.size()) throw std::invalid_argument("Array index rank mismatch");
    uint64_t flat = 0;
    for (size_t d = 0; d < indices.size(); ++d) {
        auto position = (int64_t(indices[d]) - storage->left[d]) * (storage->left[d] <= storage->right[d] ? 1 : -1);
        if (position < 0 || position >= storage->shape[d]) throw std::out_of_range("RTL array index out of range");
        flat = flat * uint64_t(storage->shape[d]) + uint64_t(position);
    }
    return flat;
}

std::vector<int> ArrayBinding::Indices(uint64_t index) const {
    storage->CheckIndex(index);
    std::vector<int> indices(storage->shape.size());
    for (size_t d = indices.size(); d-- > 0;) {
        auto position = int64_t(index % uint64_t(storage->shape[d]));
        index /= uint64_t(storage->shape[d]);
        indices[d] = int(storage->left[d] + position * (storage->left[d] <= storage->right[d] ? 1 : -1));
    }
    return indices;
}

void ArrayBinding::BindDPIPtr(uint64_t read, uint64_t write) {
    if (storage->kind != XDataBackendKind::Unknown) throw std::logic_error("Array backend is already bound");
    if (!read || Size() > uint64_t(std::numeric_limits<int>::max()))
        throw std::invalid_argument("DPI array requires a getter and int-sized element indices");
    storage->getter = read;
    storage->setter = write;
    storage->kind = XDataBackendKind::DPI;
}

void ArrayBinding::BindNativeData(uint64_t base, const std::vector<unsigned long long> &strides) {
    if (storage->kind != XDataBackendKind::Unknown) throw std::logic_error("Array backend is already bound");
    if (!base) throw std::invalid_argument("Native array base is null");
    auto layout = strides;
    if (!layout.empty() && layout.size() != storage->shape.size())
        throw std::invalid_argument("Native array stride rank mismatch");
    if (layout.empty()) {
        layout.resize(storage->shape.size());
        uint64_t extent = native_bytes(storage->width);
        for (size_t d = layout.size(); d-- > 0;) {
            layout[d] = extent;
            extent = checked_multiply(extent, uint64_t(storage->shape[d]));
        }
    }
    uint64_t extent = native_bytes(storage->width);
    for (size_t d = layout.size(); d-- > 0;) {
        if (storage->shape[d] > 1 && layout[d] < extent)
            throw std::invalid_argument("Native array elements overlap");
        extent = checked_add(extent, checked_multiply(uint64_t(storage->shape[d] - 1), layout[d]));
    }
    checked_add(base, extent - 1);
    storage->base = base;
    storage->strides = std::move(layout);
    storage->kind = XDataBackendKind::MemDirect;
}

uint64_t ArrayBinding::Address(uint64_t index) const { return storage->Address(index); }

void ArrayBinding::BindElement(XData &element, uint64_t index) const {
    storage->CheckIndex(index);
    if (storage->kind == XDataBackendKind::Unknown) throw std::logic_error("Array backend is not bound");
    if (element.mWidth != storage->width) throw std::invalid_argument("Array element width mismatch");
    element.bitRead = nullptr;
    element.bitWrite = nullptr;
    element.vecRead = nullptr;
    element.vecWrite = nullptr;
    element.array_storage = storage;
    element.array_location = storage->kind == XDataBackendKind::MemDirect ? storage->Address(index) : index;
    element.backend_kind = storage->kind;
    element.readonly_backend = storage->kind == XDataBackendKind::DPI && !storage->setter;
    element.last_is_write = false;
    element.update_read();
}

std::shared_ptr<XData> ArrayBinding::AtFlat(uint64_t index) {
    storage->CheckIndex(index);
    auto found = elements.find(index);
    if (found != elements.end()) return found->second;
    auto element = std::make_shared<XData>(storage->width,
        storage->kind == XDataBackendKind::DPI && !storage->setter ? IOType::Output : IOType::InOut);
    BindElement(*element, index);
    elements.emplace(index, element);
    return element;
}

std::shared_ptr<XData> ArrayBinding::At(const std::vector<int> &indices) { return AtFlat(FlatIndex(indices)); }

} // namespace detail
} // namespace xspcomm
