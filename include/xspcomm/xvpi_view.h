#ifndef XSPCOMM_XVPI_VIEW_H
#define XSPCOMM_XVPI_VIEW_H

#include "xspcomm/xview.h"

namespace xspcomm {
namespace detail { struct VpiContext; }

// One function table and handle/cache owner per DUT/library instance. Leaves
// use VPI, never native addresses or generated per-element callbacks.
class XVPI {
    std::shared_ptr<detail::VpiContext> context;
    std::map<std::string, XView> views;
    std::map<std::string, std::string> layouts;
public:
    XVPI();
    ~XVPI();
    uint64_t CSelf() { return reinterpret_cast<uint64_t>(this); }
    bool IsOpen() const;
    void Close();
    XView Get(const std::string &name);
    void Configure(uint64_t get, uint64_t get_value, uint64_t put_value,
                   uint64_t by_name, uint64_t by_index, uint64_t handle,
                   uint64_t iterate, uint64_t scan, uint64_t get_str,
                   uint64_t release, const std::string &scope,
                   const std::string &rtl_layouts = "{}");
#ifndef SWIG
    XVPI(const XVPI &) = delete;
    XVPI &operator=(const XVPI &) = delete;
#endif
};
} // namespace xspcomm
#endif
