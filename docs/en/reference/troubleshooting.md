# Troubleshooting

First separate installation/linking, signal data, clock driving and matcher descriptors. Identify the imported package so you do not debug a different checkout:

```sh
python3 -c 'import xspcomm as x; print(x.__file__, x.version(), x.abi_version(), x.XTriggerEngine.CoverageVersion())'
python3 -m pip show xspcomm
```

For v1.0.0, expect version 1.0.0 / ABI 3 / Coverage 5.

| Symptom | Check | Action |
| --- | --- | --- |
| Import failure, undefined symbol, missing libxspcomm.so.3 | Python minor version, platform, library paths and DUT build ABI | Install a matching wheel and rebuild the DUT in that environment; avoid mixing SDKs |
| Manual Step raises an ownership error | An engine has called AcquireClock | Drive through that engine; Clear/ReleaseClock before resuming manual stepping |
| ReleaseClock fails | Fired but undisarmed registrations | Disarm each handle or Clear, then check that ActiveCount is zero |
| Coverage counts stay zero | Execution/sampling, phase, gate, point order and program completion | Reduce to the [minimal event/coverage example](../guides/events-coverage.md); distinguish matching from classification |
| SamplePhase(DriveStable) fails | Current phase is not FallingStable | Sample at a legal phase; SamplePhase does not advance the clock |
| Wide expression loses high bits | U64, Eval or legacy NewSignal/CompileExpr | Use Python U/value, byte access or typed EvalBits; compare wide predicates explicitly with zero |
| Typed construction/evaluation fails | Width/sign/Bool types, constant length, zero divisor or negative shift | Cast explicitly and check [type rules](expressions.md) |
| A root points to a different node after Collect | Retained roots were not all remapped | Update retained roots with the returned mapping and use them within their creating engine |
| Native read returns the right value but remains unknown | Existing bval and two-state storage | Check [BindNativeData semantics](xdata.md); use a fresh known signal and retain its storage |
| A write does not immediately reach the DUT | Write mode, direction and refresh edge | Advance to the intended edge; use ImmSet only when immediate driving fits the DUT timing |
| Random crash after native binding | Lifetime, offset/stride/width and array bounds | Match the layout, retain storage/proxies and reduce to one signal |
| XConditionCheck never fires | BindXClock was used without Attach | BindXClock selects stop targets; Attach schedules sampling |
| Python await never resumes | Engine execution mixed with legacy Step events | Choose one scheduler; engine waits use its hit protocol |
| TLM module missing | Standalone SystemC/UVMC bridge was not built | Build the [TLM environment](tlm.md); the core wheel does not provide the full simulator stack |

## Reporting an issue

Include package/ABI/protocol versions, platform, Python/compiler versions, minimal code, the complete error and expected result. For timing issues, record initial phase, Step/RunUntil arguments, gate/abort and registration/cleanup order. For wide values, include width, signedness and aval/bval bytes. Provide public minimal RTL or a standalone XData reproducer when a DUT is involved.

Standard exceptions can be caught, but some legacy Assert/Fatal failures terminate the process. Run crash reproducers in a separate process to preserve diagnostic logs.
