#ifndef XSPCOMM_COMMON_LIFETIME_H
#define XSPCOMM_COMMON_LIFETIME_H

#include <memory>

namespace xspcomm::detail {

// Copies have independent lifetimes, even when the containing object is copied.
class Lifetime {
    std::shared_ptr<const int> token;
public:
    Lifetime();
    Lifetime(const Lifetime &);
    Lifetime &operator=(const Lifetime &) { return *this; }
    std::weak_ptr<const int> Observe() const { return token; }
};

} // namespace xspcomm::detail

#endif
