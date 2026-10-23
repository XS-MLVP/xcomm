# TLM: transactions and SystemC/UVM bridges

Public headers are [tlm/message.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/tlm/message.h) and [tlm/pubsub.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/tlm/pubsub.h). TLM exchanges transaction messages instead of moving each transaction through XData's bit/vector access.

Build this integration separately with SystemC/UVMC and a simulator environment. The Python bridge is commonly named tlm_pbsb, depending on MODULE_NAME at build time.

## tlm_msg

| API/field | Behavior |
| --- | --- |
| `tlm_msg()`; copy constructor | Create or copy a message. The default constructor initializes cmd/resp_status/option; start with it, then set data. |
| `from_bytes(data)`; `as_bytes()` | Set or read payload bytes. |
| `clone()` | Allocate a separate copy; C++ callers release it. |
| `cmd` | TLM_READ_COMMAND, TLM_WRITE_COMMAND, TLM_IGNORE_COMMAND |
| `resp_status` | OK/INCOMPLETE or GENERIC/ADDRESS/COMMAND/BURST/BYTE_ENABLE errors |
| `option` | TLM_MIN_PAYLOAD, TLM_FULL_PAYLOAD, TLM_FULL_PAYLOAD_ACCEPTED |
| `delay/time_unit`; `data` | Delay, time unit and byte payload |

Pointer-range and vector constructors also exist. Set cmd/resp_status/option explicitly before crossing the bridge, since constructor initialization differs.

## Publish and subscribe

| API | Behavior |
| --- | --- |
| `TLMPub(channel)`; `Connect()` | Create and connect a publisher |
| `SendMsg(msg)`; `GetChannel()` | Send a transaction or query its channel |
| `TLMSub(channel)`; `SetHandler(callback)`; `Connect()` | Create a subscriber, install a handler and connect |
| Python `TLMSub(channel, callback)` | Convenience constructor |
| `MakeChannel/UMakeChannel` | Add/remove publisher .pub and subscriber .sub channel suffixes |
| `tlm_pbsb_run(time, unit="ns")` | Advance the SystemC runtime |
| `step(time, scale=1, unit="ns")` | Advance time in the bridge environment |
| `tlm_vcs_init(argc,argv)`; `tlm_vcs_step(delay)` | Initialize/advance VCS in USE_VCS builds; Python init accepts string arguments |

Keep endpoints, handlers and the bridge runtime alive during use. TLM time advancement and XClock half-cycle/phase advancement are separate protocols; an integration must choose its top-level drive entry.

## Build and examples

Integration sources are in [integrations/tlm](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/integrations/tlm), installed as share/xspcomm/tlm in the SDK. Build python.i and pubsub.cpp there; public types remain under include/xspcomm/tlm, without compiling old implementation files from include.

See [tests/integration/tlm/test.py](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/tests/integration/tlm/test.py) and its adjacent Makefile/VCS files for a complete environment. Importing the core wheel does not validate this bridge; running the integration requires the external VCS/SystemC tools.
