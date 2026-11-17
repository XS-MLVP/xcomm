#ifndef XSPCOMM_DETAIL_VIEW_LAYOUT_H
#define XSPCOMM_DETAIL_VIEW_LAYOUT_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xspcomm::detail {

// Immutable semantic type information. It contains no model addresses, backend
// callbacks or owned signal values, and is shared by all instances of a type.
struct Layout {
    std::string kind;
    bool packed = false, is_signed = false, four_state = true;
    bool has_range = false;
    uint32_t width = 0;
    int left = 0, right = 0;
    unsigned depth = 1;
    std::vector<std::string> names;
    std::vector<uint32_t> offsets;
    std::vector<std::shared_ptr<const Layout>> children;
    uint64_t Bits() const { return width ? width : 1; }
    uint64_t Size() const { return uint64_t(left >= right ? int64_t(left) - right : int64_t(right) - left) + 1; }
};

using LayoutResolver = std::function<std::shared_ptr<const Layout>(const std::string &, unsigned)>;
std::shared_ptr<const Layout> ParseLayout(const std::string &description,
                                         const LayoutResolver &resolve = {}, unsigned depth = 0);

} // namespace xspcomm::detail
#endif
