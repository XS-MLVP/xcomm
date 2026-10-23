# xcomm documentation

xcomm provides C++ components for hardware simulation: signal access, port collections, clock advancement, expression evaluation, temporal event matching, and functional coverage. The Python package is named `xspcomm`; bindings are also available for Java, Scala, Go, Lua, and JavaScript.

Start with [installation](guides/installation.md) and [your first signal and event](tutorials/first-simulation.md). The tutorial uses a small script to introduce wide signals, clocks, and event waits.

## Write tests and verification frameworks

| Task | Documentation |
| --- | --- |
| Wait for conditions, match sequences, and collect coverage | [Events and coverage](guides/events-coverage.md) |
| Calculate with values wider than 64 bits | [Typed expressions](guides/typed-expressions.md) |
| Execute language-level definitions in C++ | [Architecture and IR](explanation/architecture.md) |
| Diagnose runtime errors | [Troubleshooting](reference/troubleshooting.md) |
| Migrate existing code | [Interface migration](reference/compatibility.md) |

## Look up an API

| Component | Purpose |
| --- | --- |
| [XData](reference/xdata.md) | Four-state signals, values, bit slices, and wide data |
| [XPort](reference/xport.md) | Named signal collections and batch refresh |
| [XClock](reference/xclock.md) | Simulator execution callbacks, clocks, and edge callbacks |
| [ExprEngine](reference/expressions.md) | Expression construction and evaluation |
| [XEngine](reference/engine.md) | Simulation advancement, registered events, and coverage sampling |
| [Trigger](reference/triggers.md) | Condition, Sequence, and FSM matching |
| [Coverage](reference/coverage.md) | Points, bins, crosses, and hit counts |
| [Monitor](reference/monitors.md) | Checkers run through clock callbacks |
| [XSignalCFG](reference/signal-config.md) | Native signal binding from layout information |
| [Language bindings](reference/bindings.md), [TLM](reference/tlm.md) | Language interfaces and transaction communication |

See the [API index](reference/api-index.md) for types and headers. For implementation changes and optimization, see [development and testing](development/contributing.md) and [performance](explanation/performance.md). [Documentation maintenance](development/documentation.md) covers editing, site previews, and Wiki export.
