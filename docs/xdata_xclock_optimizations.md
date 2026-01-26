# XData / XClock / XPort Optimization Notes (1,2,3,5)

## Scope
Only optimization items 1 / 2 / 3 / 5 are covered.
No changes for divider algorithm (4) or runtime fast_mode advice (6).

---

## 1) XData::_update_shadow lower cost when no callbacks
Location: src/xdata.cpp : XData::_update_shadow
Current: Every read/write does shadow compare/copy even when no callbacks.
Idea:
- When has_on_change_cbs == false, skip per-element compare and callback logic.
- Keep shadow baseline consistent to avoid missing the first change after callbacks are added.

Risk:
- If shadow is not updated while no callbacks, registering a callback later can mis-detect changes.

Mitigation:
- On OnChange registration, refresh shadow (or copy current data to shadow) as the baseline.

Expected gain:
- Significant when many signals have no callbacks and are read/written frequently.

---

## 2) XData::_need_write use memcmp/memcpy
Location: src/xdata.cpp : XData::_need_write / _update_last_write
Current: Element-by-element compare and copy for vec data.
Idea:
- Use memcmp for change detection on xsvLogicVecVal arrays.
- Use memcpy to update last_pVecData on change.

Risk:
- Requires contiguous array layout (xsvLogicVecVal array) and correct vecSize.

Expected gain:
- Reduced branch/loop overhead for wide buses and frequent writes.

---

## 3) XPort hot-iteration vector
Location: src/xport.cpp / include/xspcomm/xport.h
Current: WriteOnRise/Fall/ReadFresh iterate std::map every time.
Idea:
- Keep map for lookups.
- Add a std::vector<XData*> for hot loops (Write/Read/SetZero/String).
- Maintain vector in Add/Del/SelectPins/Connect.

Risk:
- Must keep map and vector in sync.
- Sub-port and SelectPins paths must build correct vectors.

Expected gain:
- Lower cache-miss and tree-iteration overhead when port count is large.

---

## 5) Callback assert cost moved to registration
Location: src/xclock.cpp : _call_back / _add_cb
Current: Assert(func != nullptr) on every callback invocation.
Idea:
- Validate in _add_cb (registration), not every call.
- Or keep runtime assert only in debug builds.

Risk:
- Release builds lose runtime protection (registration still checks).

Expected gain:
- Small but measurable reduction when callbacks are many/frequent.

---

## Correctness Fix: Reset last_write state on ReInit
Location: src/xdata.cpp : XData::ReInit, XData::~XData
Reason:
- ReInit does not reset last_is_write/last_pVecData, which can leave stale
  pointers and sizes after width changes.
- With memcmp/memcpy optimization, stale last_pVecData can cause OOB access.

Fix:
- In ReInit: set last_is_write = false, last_mLogicData = 0; free and null
  last_pVecData if allocated.
- In destructor: free last_pVecData to avoid leak.

Behavior impact:
- No functional change; prevents stale state and memory leak.

---

## Correctness Fix: ReInit memory management
Location: src/xdata.cpp : XData::ReInit / ~XData
Reason:
- ReInit allocates pVecData/__pVecData/pinbind_vec with calloc but does not free
  old allocations on repeated ReInit (leak).
- Destructor used delete for calloc memory (UB).
Fix:
- ReInit frees old pVecData/__pVecData/pinbind_vec (after deleting PinBind*).
- Destructor frees pVecData/__pVecData/pinbind_vec (after deleting PinBind*).
Behavior impact:
- No functional change; prevents leaks and UB on repeated ReInit.

---

## Correctness Fix: validate checks all bval words
Location: src/xdata.cpp : XData::_update_shadow
Reason:
- validate only checks pVecData[0].bval, missing X/Z in higher words.
Fix:
- Check pVecData[i].bval in the loop; any nonzero makes validate=false.
Behavior impact:
- Callback validity flag becomes correct for wide vectors.
