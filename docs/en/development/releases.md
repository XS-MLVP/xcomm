# Maintaining releases

The xspcomm Python version comes from this repository's vX.Y.Z tags. CMakeLists.txt tracks the native ABI separately. Ordinary PR/push CI produces development wheels.

## Automated flow

1. Give each PR targeting master exactly one release:patch/minor/major label.
2. After merge, Version policy calculates the version, tags the merged PR result on master and emits version-tagged.
3. Release Python wheel validates the existing tag, builds and installs a wheel in a clean environment, checks versions, signals/clocks and native linking, then attaches it to a GitHub Release.

Every labeled merged PR, including documentation-only changes, enters this flow. Direct pushes do not automatically create versions. Workflows alone do not block merges: configure Version policy / release-label and required wheel checks as required statuses, and require an up-to-date PR. Add merge_group support to checks before enabling merge queue.

This flow does not publish to PyPI automatically. Release xcomm before Picker when Picker depends on the new version. Recover failed publication by manually rerunning Release Python wheel with **the same existing tag**. Use Version policy's manual bump for a missed merge event; do not move published tags casually.

## Local package checks

```sh
python3 -m venv /tmp/xspcomm-package-build
/tmp/xspcomm-package-build/bin/python -m pip install build
/tmp/xspcomm-package-build/bin/python -m build --wheel
python3 -m venv /tmp/xspcomm-package-test
/tmp/xspcomm-package-test/bin/python -m pip install --no-index --find-links=dist xspcomm
/tmp/xspcomm-package-test/bin/python -m pip check
/tmp/xspcomm-package-test/bin/python -c 'import xspcomm as x; print(x.version(), x.abi_version(), x.XTriggerEngine.CoverageVersion())'
```

Builds require C++20, CMake, SWIG>=4.2 and matching Python development headers. Release checks also compare package metadata with native version() and ensure library search paths do not leak build-machine locations. CI rebuilds from sdist to verify version preservation without .git. Wheels are built for a specific CPython minor version.

## ABI, protocol and installation boundaries

XSPCOMM_ABI_VERSION sets the libxspcomm.so.N SONAME and is exposed by abi_version(). Evaluate an increment for incompatible layout/symbol changes against master or a published ABI. Iterations on one unpublished branch share a transition. Rebuild native consumers and DUTs and update Picker's supported range; a Python package range cannot replace ABI checks.

CoverageVersion identifies the point/bin/program protocol independently of package version and ABI. Descriptor changes need matching frontend lowering and [migration notes](../reference/compatibility.md). Install only the public SDK and helpers required by public templates; release packaging should not expose matcher, coverage-state or native-memory implementation headers.

Documentation publication is separate: the current [documentation CI](documentation.md) builds previews and Wiki files without pushing a site or remote Wiki.
