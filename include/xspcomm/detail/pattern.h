#ifndef XSPCOMM_DETAIL_PATTERN_H
#define XSPCOMM_DETAIL_PATTERN_H

#include "xspcomm/xpattern.h"
#include <cstddef>
#include <vector>

namespace xspcomm {
class ExprEngine;
namespace detail {

struct PatternState {
    bool sequence_failed = false;
    bool sequence_expired = false;
    size_t sequence_index = 0;
    uint64_t sequence_age = 0;
    uint64_t sequence_held = 0;
    uint32_t fsm_current_state = 0;
};

// A non-owning view, valid only while evaluating the owning registration.
struct PatternView {
    const std::vector<XSequenceStep> *sequence = nullptr;
    const std::vector<XFsmTransition> *fsm = nullptr;
    uint32_t start_state = 0;
};

bool AdvanceSequence(const std::vector<XSequenceStep> &steps,
                     PatternState &state, ExprEngine &expr);
bool AdvanceFsm(const std::vector<XFsmTransition> &transitions,
                PatternState &state, ExprEngine &expr, uint32_t &terminal);

} // namespace detail
} // namespace xspcomm

#endif
