# 维护发行

xspcomm 的 Python 包版本取自本仓库 vX.Y.Z 标签；原生 ABI 在 CMakeLists.txt 中单独维护。普通 PR/push CI 生成开发 wheel，不创建稳定发行。

## 自动流程

1. 指向 master 的 PR 选择且仅选择 release:patch/minor/major。
2. 合并后 Version policy 根据标签计算版本，在 master 的已合并 PR 结果提交上创建 tag，并发送 version-tagged 事件。
3. Release Python wheel 校验已有 tag，构建并在干净环境安装 wheel，检查版本、信号/时钟和原生库，再附加到 GitHub Release。

每个带标签的合并 PR，包括纯文档修改，都会进入版本流程；直接 push 不自动生成版本。workflow 本身不会阻止合并，仓库应将 Version policy / release-label 和所需 wheel 检查设为 required，并要求 PR 同步最新基线。启用 merge queue 前，先为检查添加 merge_group 支持。

该流程不自动发布 PyPI。Picker 依赖新 xcomm 时，先完成 xcomm Release，再发布 Picker。恢复失败的发布时，对**相同已有 tag**手动重跑 Release Python wheel；缺失合并事件可使用 Version policy 的手动 bump，不能随意移动已发布 tag。

## 本地包检查

```sh
python3 -m venv /tmp/xspcomm-package-build
/tmp/xspcomm-package-build/bin/python -m pip install build
/tmp/xspcomm-package-build/bin/python -m build --wheel
python3 -m venv /tmp/xspcomm-package-test
/tmp/xspcomm-package-test/bin/python -m pip install --no-index --find-links=dist xspcomm
/tmp/xspcomm-package-test/bin/python -m pip check
/tmp/xspcomm-package-test/bin/python -c 'import xspcomm as x; print(x.version(), x.abi_version(), x.XTriggerEngine.CoverageVersion())'
```

构建需要 C++20、CMake、SWIG>=4.2 和对应 Python 开发头。发布检查还要核对 Python 包版本与原生 version() 一致，并检查动态库搜索路径不泄漏构建机路径。CI 从 sdist 重建 wheel，确保离开 .git 后仍保留版本；wheel 按 CPython 小版本构建。

## ABI、协议与安装边界

XSPCOMM_ABI_VERSION 设置 libxspcomm.so.N 的 SONAME，并通过 abi_version() 暴露。相对已发布 ABI 或 master 有不兼容布局/符号变化时评估递增；同一未发布分支的迭代共享一次迁移。升级后重建原生消费者和 DUT，更新 Picker 支持范围；Python 包范围不能替代 ABI 检查。

CoverageVersion 标识 point/bin/program 协议，与包版本和 ABI 分开。描述结构变化时同步前端 lowering 和[迁移说明](../reference/compatibility.md)。安装树只包含公共 SDK 及公共模板所需辅助头；matcher、coverage state、native memory 实现不应因发版而进入 SDK。

文档发布与代码发版分开：当前 [文档 CI](documentation.md) 仅构建预览和 Wiki 文件，不推送站点或远端 Wiki。
