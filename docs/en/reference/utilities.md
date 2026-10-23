# Utilities and C++ coroutines

These APIs provide supporting infrastructure. For ordinary signals, event waits and coverage, start with [XData](xdata.md) and [XEngine](engine.md); native address helpers are usually unnecessary.

## Comparison and memory access

Headers: [compare_op.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/common/compare_op.h), [compare.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/common/compare.h), [memory_access.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/common/memory_access.h).

| API | Behavior and constraints |
| --- | --- |
| `CompareOp`; `compare::Compare<Op>(a,b)`; `compare::Compare(op,a,b)` | Shared EQ/NE/GT/GE/LT/LE dispatch; operand types determine semantics. |
| `compare::Order(a,b)` | Return -1/0/1 ordering. |
| `compare::CompareWords(words, read_a, read_b)` | Readers expose low words first; ordered comparison scans from the high word. |
| `compare::EqualWords(a_count,b_count,read_a,read_b)` | Equality with unsigned zero extension. |
| `compare::CompareSignedBytes(a,b,bytes)` | Equal-length little-endian two's-complement comparison. |
| `compare::WithinRange(...)` | Unsigned range tests for scalars or word readers. |
| `memory::Load<T>/Store<T>` | Fixed-size memcpy for trivially copyable types, without T-alignment requirements. Callers still provide valid memory ranges. |
| `memory::UIntBytes<N>::Load(data)` | Exact-width unsigned reads for N=1..8, used by adapters. |
| `memory::ByteWords`; `Count()`; indexing | Non-owning 32-bit word view of a little-endian buffer, including a partial final word. |

These shared algorithms do not make four-state XData comparison, signed pointer comparison and RangeCheck identical. Applications generally use the corresponding object methods.

## XByteBuffer and strings

Header: [memory.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/common/memory.h).

| API | Behavior and constraints |
| --- | --- |
| `XByteBuffer(bytes)` | Allocate owned storage; bytes must be positive. |
| `XByteBuffer(address,bytes)` | Borrow existing storage without owning or extending its lifetime. |
| `Copy()` | Allocate an independent copy, released by the C++ caller. |
| `SyncFrom(address,size)/SyncTo(address,size)` | Explicit copying; callers ensure size and both memory ranges are valid. |
| `SetZero/BaseAddr/Size/AsBytes/FromBytes` | Clear, query address/size, export bytes, or import up to capacity and return bytes written. |
| `operator==` | Compare length and content. |
| `CString()` or `CString(text)`; `Get/Set` | Own a string. |
| `CharAddress()` | Return c_str's address; modifying or destroying the string invalidates reliance on it. |
| `AssignFrom(pointer/address)` | Read a C string. |
| `AssignTo(pointer/address)` | Currently changes only a local pointer; does not copy into a destination buffer. |

GetFromU64Array/GetFromU32Array/GetFromU8Array and SetU64Array/SetU32Array/SetU8Array access native arrays. U64PtrAsU64/U32PtrAsU64/U8PtrAsU64 and U64AsU64Ptr/U64AsU32Ptr/U64AsU8Ptr convert pointers and addresses. They do not validate lifetimes or bounds.

## xfunction callbacks

[xcallback.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xcallback.h) defines xfunction<R,Args...>, adapting C++ callables to SWIG directors. C++ accepts lambdas/std::function and provides call, invocation operators, null queries and assignment.

set_force_callable forces director forwarding. Raw function-address bridges require matching signatures. A uint64 address neither validates the target nor owns it.

## C++ coroutines

[xcoroutine.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xcoroutine.h) requires ENABLE_XCOROUTINE. XCorutine/xcorutine and schedule_awit retain their existing source spellings.

| API | Behavior |
| --- | --- |
| `XCorutine<T>`; `xcorutine<T>` | Awaitable return type paired with XPromise. |
| `XStep(clock,n)`; `clock.AStep(n)` | Wait for a target cycle; clock scheduling resumes execution. |
| `XCondition(checker)`; `clock.ACondition(checker)` | Wait for a C++ callable to become true. |
| `XNext(n)`; `clock.ANext(n)` | Wait for the specified number of scheduler rounds. |
| `schedule_awit(handler_map)` | Scan and resume ready coroutines, usually from the clock. |
| `XPromise<T>`; `_XAWait/_PromiseBase` | Coroutine protocol and extension infrastructure. |

The clock schedules these native coroutines. Integration code manages cancellation, exceptions, and associated object lifetimes. Python asyncio and XReactor use their own scheduling entry points.

## Logging, versions and helpers

Header: [xutil.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xutil.h).

| API | Behavior |
| --- | --- |
| `version()`; `abi_version()`; `checkVersion()` | Package version, native ABI, and runtime/header version checks; separate from Coverage protocol version. |
| `get_log_level/set_log_level`; `LogLevel` | debug/info/warning/error/fatal. |
| `Info/Warn/Error/Debug/Fatal`; `Assert`; `DebugC`; `Traceback` | Logging and diagnostics; failed Fatal/Assert terminate the process. |
| `uTime()`; `fmtTime(microseconds,format)`; `fmtNow(format)` | Host time helpers. fmtTime takes microseconds; fmtNow currently passes seconds, so use fmtTime(uTime()) when exact time is needed. |
| `sFmt/sArrayHex/sWith/sLower/FmtSize` | Formatting, hex display, prefix tests, lowercase and size formatting. |
| `xRandom(a,b)`; `XSeed(seed)` | Random-number helpers based on rand. |
| `contians(vector,value)`; `removeSuffix(text,suffix)`; `fileExists(path)` | Search, suffix removal and file checks; contians is the existing spelling. |
| `big_shift/big_binstr/big_mask/sync_data_to` | Native word-buffer shifts, display, masks, and copying. |

common/callback_storage.h and common/lifetime.h support public templates and must be installed with the SDK. Ordinary applications do not need to depend on these helper types directly.
