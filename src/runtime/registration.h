#ifndef XSPCOMM_RUNTIME_REGISTRATION_H
#define XSPCOMM_RUNTIME_REGISTRATION_H

#include "xspcomm/xengine.h"
#include "trigger/registration.h"

namespace xspcomm {

namespace detail { class CoverageState; }

// Keep registrations in the engine's existing contiguous storage.
struct XEngine::Watcher : detail::TriggerRegistration {
    std::shared_ptr<detail::CoverageState> coverage;

    detail::PatternView CoverageSource() const {
        if (kind == Kind::Sequence || kind == Kind::Fsm)
            return detail::PatternView::From(program);
        return {};
    }
};

} // namespace xspcomm

#endif
