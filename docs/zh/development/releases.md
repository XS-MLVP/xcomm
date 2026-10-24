# 发行维护

xspcomm 的 Python 版本取自仓库 `vX.Y.Z` 标签；CMakeLists.txt 中单独维护原生 ABI。普通 PR/push CI 验证与发行相同的打包矩阵。

## 自动流程

1. 面向 master 的 PR 必须且只能带一个 `release:patch/minor/major` 标签。
2. 合并后 Version policy 计算版本，在 master 上为合并结果打标签并发出 `version-tagged`。
3. Release Python distributions 验证已有标签，构建一份源码包与四个 Linux x86-64 wheel（GLIBC 2.17/2.28 × CPython 3.11/3.12），检查安装后的绑定和 C++ SDK，将五个文件附到 GitHub Release，再发布到 PyPI。

文档修改也进入此流程。直接 push 不会自动创建版本。工作流本身不阻止合并：应配置 Version policy / release-label 及 wheel 检查为必要状态，并要求 PR 保持最新；启用 merge queue 前需补充 merge_group 检查。

PyPI 使用 GitHub OIDC 可信发布和 `pypi` 环境，首次发行前需完成下方配置。Picker 依赖新版 xcomm 时，先发行 xcomm。恢复失败发行时，使用**同一个已有标签**重新运行 Release Python distributions；漏掉合并事件时使用 Version policy 的手动 bump，不移动已发布标签。

## PyPI 配置

使用计划持有项目的 PyPI 账户登录，验证邮箱并启用双因素认证，在[账户 Publishing 页面](https://pypi.org/manage/account/publishing/)添加待创建项目的 GitHub 发布者：

| 字段 | 值 |
| --- | --- |
| PyPI 项目名 | `uc-xcomm` |
| Owner | `XS-MLVP` |
| Repository | `xcomm` |
| Workflow filename | `release.yml` |
| Environment | `pypi` |

首次成功上传时创建项目；登记待创建的发布者不会预留包名。若账户下已存在 `uc-xcomm`，在该项目的 Publishing 设置中登记相同字段。GitHub Secrets 无需存放 PyPI API token 或密码。创建仓库环境 `pypi`；若配置部署规则，需允许从 `master` 执行发行工作流。

带正确标签的 PR 合并后沿用现有自动打版本标签流程。未配置 PyPI 发布者时，GitHub Release 可以成功而 PyPI 任务失败；补齐配置后重跑失败任务即可。重试跳过已经上传的文件；已发布版本的内容需要修改时应创建新版本，不覆盖原文件。TestPyPI 可选，账户和发布者配置与正式 PyPI 独立。

## 构建与验证策略

PR CI 与发行共用 `build-python.yml`。先生成一份 sdist，再从不含 `.git` 的源码包构建全部 wheel；manylinux 镜像固定 digest，SWIG 由构建依赖提供。`auditwheel repair` 校验所选平台基线。修复后的 wheel 在对应 manylinux 容器中安装并运行 Python 检查，同时编译链接仅使用包内公开 SDK 的 C++ 客户端。同时使用 Ubuntu 宿主编译器编译运行 SDK 客户端，检查与现代 Picker/DUT 编译器的兼容性。发行前核对四种 wheel 标签、唯一源码包及其版本。

两种 wheel 基线统一使用 libstdc++ 的 C++11 ABI（`_GLIBCXX_USE_CXX11_ABI=1`）。固定的 manylinux2014 Devtoolset 10 虽然在静态运行库中包含两种 ABI，实现声明却被配置头关闭；发行准备脚本仅在一次性容器中启用这些声明，GLIBC 2.17 构建静态链接 libstdc++。GLIBC 仍为动态链接，由 auditwheel 校验。源码安装的编译器不受影响，用户也无需改用旧 C++ ABI。wheel 测试覆盖跨 SDK 边界的字符串、容器、回调和异常传播。

本地及 pip 源码构建默认 `XSPCOMM_NATIVE=ON`；cibuildwheel 配置显式关闭。CI 检查实际编译命令：发行包为 `-march=x86-64 -mtune=generic`，源码安装为 `-march=native -mtune=native`，并确认协程支持仍启用。CMake Release 优化与该开关独立。源码安装任务验证 CPython 3.11、3.12，使用 `swig>=4.2.1,<5` 范围内的最新版本，不额外增加 SWIG 版本矩阵，也不安装系统 SWIG。

## ABI、协议与安装边界

XSPCOMM_ABI_VERSION 设置 libxspcomm.so.N 的 SONAME，并通过 abi_version() 暴露。相对已发布 ABI 或 master 有不兼容布局/符号变化时评估递增；同一未发布分支的迭代共享一次迁移。升级后重建原生消费者和 DUT，更新 Picker 支持范围；Python 包范围不能替代 ABI 检查。

CoverageVersion 标识 point/bin/program 协议，与包版本和 ABI 分开。描述结构变化时同步前端 lowering 和[迁移说明](../reference/compatibility.md)。安装树只包含公共 SDK 及公共模板所需辅助头；matcher、coverage state、native memory 实现不应因发版而进入 SDK。

文档发布与代码发版分开：当前 [文档 CI](documentation.md) 仅构建预览和 Wiki 文件，不推送站点或远端 Wiki。
