#include "xspcomm/common/lifetime.h"

namespace xspcomm::detail {

Lifetime::Lifetime() : token(std::make_shared<const int>(0)) {}
Lifetime::Lifetime(const Lifetime &) : Lifetime() {}

} // namespace xspcomm::detail
