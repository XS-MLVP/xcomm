# Maintaining the xspcomm Python wheel

The Python package version comes from this repository's `vX.Y.Z` Git tags.
The scikit-build-core/setuptools-scm configuration ignores unrelated tags.
Ordinary push and PR CI build development wheels and install them in a fresh
virtual environment; they do not create a stable release.

Give every PR targeting `master` exactly one of `release:patch`,
`release:minor`, or `release:major`. Create these labels in GitHub, then add
**Version policy / release-label** to the required status checks in Settings →
Rules → Rulesets (or branch protection). The workflow alone does not block a
merge. Do not use merge queue until the required check also supports
`merge_group`. Require the wheel CI checks and an up-to-date PR as well;
otherwise tagging after merge does not imply the merged source passed CI.

After a labeled PR merges, the `master` push starts **Version policy**. It
reads the PR label, computes and pushes the next `vX.Y.Z` tag, then emits a
`version-tagged` event. **Release Python wheel** subscribes to that event;
other artifact publishers may subscribe independently. Every merged PR,
including docs-only changes, creates a version tag. Direct pushes do not
create one. The release workflow builds and installs the wheel, checks native and Python versions and
the baseline signal/clock API, then attaches it to a GitHub release. It does not change the
tag. Version selection and publication accept only merged PR result commits on
master's first-parent line; unrelated feature tags do not advance the release
version. If publishing fails, rerun **Release Python wheel** manually with the
same tag. **Version policy** has a manual bump input for recovering from a
missed merge event. This does not publish to PyPI automatically. Release xcomm
before a Picker release that depends on it.

To build locally, use a Python interpreter for which a wheel is wanted:

```sh
python3 -m venv /tmp/xspcomm-package-build
/tmp/xspcomm-package-build/bin/python -m pip install build
/tmp/xspcomm-package-build/bin/python -m build --wheel

python3 -m venv /tmp/xspcomm-package-test
/tmp/xspcomm-package-test/bin/python -m pip install --no-index --find-links=dist xspcomm
/tmp/xspcomm-package-test/bin/python -m pip check
```

Building requires a C++ compiler, CMake, SWIG >= 4.2 and Python development
headers. A matching wheel installs without local compilation; an sdist builds
the extension locally. CI rebuilds a wheel from sdist to check that it retains
the Git-derived version without `.git` in the archive. The SWIG extension uses
a CPython-specific wheel tag,
not `abi3`.

The native C++ ABI number is `XSPCOMM_ABI_VERSION` in `CMakeLists.txt`. It is
independent of the Python package version and sets the `libxspcomm.so.N`
SONAME. Increase it only for an incompatible native ABI change. The library
also exports `xspcomm.abi_version()`, so wheel tests can check the loaded ABI.
If it changes, update Picker's supported xcomm range and rebuild generated DUT
extensions; a Python version specifier alone cannot verify C++ ABI identity.

## Migrating to the native trigger engine

The native trigger engine changes the C++ layouts of `XClock`, `XData`, and
`ExprNode`, which introduced native ABI 2. Complete per-bin trigger programs now
reorganize `XCoverageBin` and checker/runtime layouts, so the current native ABI is 4 (`libxspcomm.so.4`). Rebuild
Picker-generated DUT extensions and other native bindings against these headers
and library, and update the consumer's ABI requirement together. Keep each
generated DUT and its runtime on a matching native ABI.

`XTriggerEngine.CoverageVersion() == 5` identifies the complete trigger-bin
descriptor protocol. It is separate from the native library ABI and the Python
package version. The Python `XPin` wrapper is removed; use `XData` directly instead.

Picker 2.0.1 restricts xspcomm to `<0.2`. Before releasing Picker with an
xspcomm `0.2.x` package, update its supported dependency range and rebuild its
bundled native runtime.

The PR #30 review migration introduces `XEngine` with the existing
`XTriggerEngine` entry alias, explicit coverage kinds and `bin.program`.
Use the `0.3.0.dev1` preview for ABI 4 / descriptor protocol 5; `0.3.0.dev0`
identified the earlier ABI 3 / protocol 4 preview. Update frontend lowering and
rebuild native clients together.
