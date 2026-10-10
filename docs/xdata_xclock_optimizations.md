# XData / XClock / XPort Optimization Notes (1,2,3,5)

## Scope
Only optimization items 1 / 2 / 3 / 5 are covered.
No changes for divider algorithm (4) or runtime fast_mode advice (6).

---

## 1) XData::_update_shadow lower cost when no callbacks
Location: src/data/data.cpp : XData::_update_shadow
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
Location: src/data/data.cpp : XData::_need_write / _update_last_write
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
Location: src/core/port.cpp / include/xspcomm/xport.h
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
Location: src/core/clock.cpp : _call_back / _add_cb
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
Location: src/data/data.cpp : XData::ReInit, XData::~XData
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
Location: src/data/data.cpp : XData::ReInit / ~XData
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
Location: src/data/data.cpp : XData::_update_shadow
Reason:
- validate only checks pVecData[0].bval, missing X/Z in higher words.
Fix:
- Check pVecData[i].bval in the loop; any nonzero makes validate=false.
Behavior impact:
- Callback validity flag becomes correct for wide vectors.

---

## Follow-up: BindNativeData X/Z semantics

Location: src/data/backends.cpp : XData::BindNativeData

Current behavior:

- Native vector reads update only `aval` and preserve `bval`; writes store only `aval`.
- Fresh buffers have zeroed `bval` from `calloc`, but existing X/Z masks can survive binding and refresh.

Reproducer:

- Set an 8-bit XData to all X, then bind a `uint8_t` containing `0x5A`.
- `U() == 0x5A`, but `XMask() == 0xFF` and `DataValid() == false`.
- The same stale-mask behavior was reproduced at 16 and 64 bits.

TODO:

- [ ] Decide whether binding and refreshing two-state native storage should clear an existing `bval` mask.
- [ ] Define how X/Z writes map to two-state storage and check scalar/vector consistency.
- [ ] Add regressions for rebinding, X/Z writes after binding, and scalar/vector behavior.
