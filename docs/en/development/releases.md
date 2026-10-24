# Maintaining releases

The xspcomm Python version comes from this repository's vX.Y.Z tags. CMakeLists.txt tracks the native ABI separately. Ordinary PR/push CI produces development wheels.

## Automated flow

1. Give each PR targeting master exactly one release:patch/minor/major label.
2. After merge, Version policy calculates the version, tags the merged PR result on master and emits version-tagged.
3. Release Python distributions validates the existing tag, builds one sdist and four Linux x86-64 wheels (GLIBC 2.17/2.28 × CPython 3.11/3.12), checks the installed bindings and C++ SDK, attaches all five files to a GitHub Release, then publishes them to PyPI.

Every labeled merged PR, including documentation-only changes, enters this flow. Direct pushes do not automatically create versions. Workflows alone do not block merges: configure Version policy / release-label and required wheel checks as required statuses, and require an up-to-date PR. Add merge_group support to checks before enabling merge queue.

PyPI publication uses GitHub OIDC Trusted Publishing with the `pypi` environment; complete the one-time setup below before the first release. Release xcomm before Picker when Picker depends on the new version. Recover failed publication by manually rerunning Release Python distributions with **the same existing tag**. Use Version policy's manual bump for a missed merge event; do not move published tags casually.

## PyPI setup

For the first publication, sign in to the intended PyPI owner account (with a verified email and two-factor authentication), open [account Publishing](https://pypi.org/manage/account/publishing/), and add a pending GitHub publisher:

| Field | Value |
| --- | --- |
| PyPI project name | `uc-xcomm` |
| Owner | `XS-MLVP` |
| Repository | `xcomm` |
| Workflow filename | `release.yml` |
| Environment | `pypi` |

A pending publisher creates the project on the first successful upload; it does not reserve the name. If `uc-xcomm` already exists under your account, add the same publisher in the project's Publishing settings. No PyPI API token or password is stored in GitHub secrets. Create the GitHub repository environment `pypi`; any deployment rules must allow the release workflow to run from `master`.

Merging a correctly labeled PR preserves the existing automatic version-tag flow. Until the PyPI publisher is configured, the GitHub release can succeed while the PyPI job fails; configure the publisher and rerun that failed job. Existing uploaded files are skipped on retries. Do not overwrite a published version with changed artifacts: make a new version instead. TestPyPI is optional and has separate accounts and publisher settings.

## Build and validation policy

`build-python.yml` is shared by PR CI and releases. It builds the sdist once, then builds every wheel from that archive without `.git`, using pinned manylinux image digests and the declared SWIG build dependency. `auditwheel repair` enforces the selected platform policy. Each repaired wheel is installed and checked inside the corresponding manylinux container, including a C++ client linked against the packaged public SDK. The SDK is also compiled and run with the Ubuntu host compiler to catch differences between the build toolchain and modern Picker/DUT toolchains. The release job requires all four wheel tags and exactly one sdist with matching versions before publishing.

Both wheel baselines use libstdc++'s C++11 ABI (`_GLIBCXX_USE_CXX11_ABI=1`). The pinned manylinux2014 Devtoolset 10 header disables dual ABI even though its static runtime includes both implementations. A release-only preparation script enables those declarations in the disposable container, and the GLIBC 2.17 build links libstdc++ statically. GLIBC remains dynamic and is checked by auditwheel. This does not modify source-install toolchains or require users to compile with the old C++ ABI. Wheel tests exercise strings, containers, callbacks and exception propagation across the SDK boundary.

`XSPCOMM_NATIVE` defaults to `ON` for local and pip source builds. The cibuildwheel configuration explicitly sets it `OFF` for distributed wheels. CI checks the actual compile commands (`-march=x86-64 -mtune=generic` for release wheels, `-march=native -mtune=native` for source installs) and requires coroutine support to remain enabled. CMake Release optimization is independent of this option. The source-install jobs test CPython 3.11 and 3.12 with the latest SWIG allowed by `swig>=4.2.1,<5`, without an additional SWIG version matrix or a system SWIG installation.

## Local package checks

```sh
python3 -m venv /tmp/xspcomm-package-build
/tmp/xspcomm-package-build/bin/python -m pip install build
/tmp/xspcomm-package-build/bin/python -m build --wheel
python3 -m venv /tmp/xspcomm-package-test
/tmp/xspcomm-package-test/bin/python -m pip install --no-index --find-links=dist uc-xcomm
/tmp/xspcomm-package-test/bin/python -m pip check
/tmp/xspcomm-package-test/bin/python -c 'import xspcomm as x; print(x.version(), x.abi_version(), x.XTriggerEngine.CoverageVersion())'
```

Builds require C++20, CMake, SWIG>=4.2 and matching Python development headers. Release checks also compare package metadata with native version() and ensure library search paths do not leak build-machine locations. CI rebuilds from sdist to verify version preservation without .git. Wheels are built for a specific CPython minor version.

## ABI, protocol and installation boundaries

XSPCOMM_ABI_VERSION sets the libxspcomm.so.N SONAME and is exposed by abi_version(). Evaluate an increment for incompatible layout/symbol changes against master or a published ABI. Iterations on one unpublished branch share a transition. Rebuild native consumers and DUTs and update Picker's supported range; a Python package range cannot replace ABI checks.

CoverageVersion identifies the point/bin/program protocol independently of package version and ABI. Descriptor changes need matching frontend lowering and [migration notes](../reference/compatibility.md). Install only the public SDK and helpers required by public templates; release packaging should not expose matcher, coverage-state or native-memory implementation headers.

Documentation publication is separate: the current [documentation CI](documentation.md) builds previews and Wiki files without pushing a site or remote Wiki.
