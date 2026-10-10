#include "xspcomm/xengine.h"
#include "xspcomm/xexpr.h"
#include "xspcomm/detail/coverage/state.h"

#include <algorithm>
#include <stdexcept>

namespace xspcomm {

void XEngine::CheckCoverageHandle(XRegistrationHandle handle) const
{
    if (!handle.IsValid() || handle.slot >= watchers.size() ||
        !watchers[handle.slot].occupied || watchers[handle.slot].generation != handle.generation ||
        !watchers[handle.slot].coverage) throw std::invalid_argument("stale coverage handle");
}

void XEngine::AttachCoverage(XRegistrationHandle handle,
    const std::vector<XCoverageItem> &items, const std::vector<XCoverageBin> &bins,
    int gate, int abort, bool raise_illegal, size_t diagnostic_capacity,
    bool overlap, size_t max_active, bool diagnostics)
{
    if (!handle.IsValid() || handle.slot >= watchers.size() ||
        !watchers[handle.slot].occupied || watchers[handle.slot].generation != handle.generation ||
        watchers[handle.slot].coverage) throw std::invalid_argument("invalid coverage registration");
    auto &watcher = watchers[handle.slot];
    watcher.coverage = std::make_shared<detail::CoverageState>(
        handle.generation, *expr_engine, CoverageSource(watcher), items, bins,
        gate, abort, raise_illegal, diagnostic_capacity, overlap, max_active, diagnostics);
}

void XEngine::ResetCoverage(XRegistrationHandle handle, bool counters)
{
    CheckCoverageHandle(handle);
    auto &watcher = watchers[handle.slot];
    watcher.ResetPattern();
    watcher.last_condition = false;
    watcher.coverage->ClearHistory();
    if (counters) watcher.coverage->ResetCounters();
}

XCoverageSnapshot XEngine::CoverageSnapshot(XRegistrationHandle handle, bool progress) const
{
    CheckCoverageHandle(handle);
    const auto &watcher = watchers[handle.slot];
    return watcher.coverage->Snapshot(clock->GetHalfTick(), progress);
}

size_t XEngine::CoverageExecutionCount(XRegistrationHandle handle) const
{
    CheckCoverageHandle(handle);
    return watchers[handle.slot].coverage->ExecutionCount();
}

} // namespace xspcomm
