"""Port-only microbenchmark; run sequentially with each build's PYTHONPATH.

Example:
    PYTHONPATH=build/python python3 tests/bindings/python/bench_xdata_value.py

Input writes are staged (the default Rise mode). This is not a DUT throughput
benchmark and does not skip or measure simulation evaluation. Read and write
results are checked; no performance thresholds are used by the test suite.
"""
import argparse
import ctypes
import json
import platform
import statistics
import time

import xspcomm as xs


def measure(function, count, repeats):
    function()
    samples = []
    for _ in range(repeats):
        start = time.perf_counter_ns()
        function()
        samples.append((time.perf_counter_ns() - start) / count)
    return {"ns_median": statistics.median(samples), "ns_samples": samples}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iterations", type=int, default=20000)
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()
    if args.iterations < 1 or args.repeats < 1:
        parser.error("iterations and repeats must be positive")
    print(json.dumps({"xspcomm": xs.__file__, "python": platform.python_version(),
                      "platform": platform.platform(), "iterations": args.iterations,
                      "repeats": args.repeats}), flush=True)
    for width in [8, 32, 64, 128, 256]:
        mask = (1 << width) - 1
        high_bit = (1 << (width - 1)) if width > 64 else 0
        values = [(high_bit | index) & mask for index in range(args.iterations)]
        memory = (ctypes.c_uint32 * ((width + 31) // 32))()
        expected_read = ((1 << (width - 1)) | 7) & mask
        raw = expected_read.to_bytes(ctypes.sizeof(memory), "little")
        ctypes.memmove(ctypes.addressof(memory), raw, len(raw))
        source = xs.XData(width, xs.XData.In)
        source.BindNativeData(ctypes.addressof(memory))
        output = xs.XData(width, xs.XData.Out)
        output.BindNativeData(ctypes.addressof(memory))

        def write_value():
            for value in values:
                source.value = value

        def read_value():
            result = 0
            for _ in values:
                result = output.value
            return result

        write_result = measure(write_value, args.iterations, args.repeats)
        read_result = measure(read_value, args.iterations, args.repeats)
        assert source.value == values[-1]
        assert read_value() == expected_read
        for operation, result in [("staged_write", write_result), ("read", read_result)]:
            print(json.dumps({"width": width, "operation": operation, **result}), flush=True)


if __name__ == "__main__":
    main()
