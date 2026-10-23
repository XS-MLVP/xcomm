# 开发、测试与贡献

先用[架构图](../explanation/architecture.md)判断修改属于信号、表达式、匹配、统计还是语言包装。应用 API 在 include/xspcomm；纯实现留在 src。私有类型的测试放在原生单元测试中。

## 增量构建与原生测试

在 xcomm 仓库根目录执行；需要 C++20 编译器、CMake>=3.15，绑定另需相应语言环境：

```sh
BUILD_XSPCOMM_SWIG=python cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
PYTHONPATH=build/python python3 tests/bindings/python/test_python.py
```

默认 `make` 进入 rbuild，会先清理 build/dist；保留增量产物时用上面的 CMake 命令。build/python 应使用同次构建的原生库。Python 绑定需要 SWIG>=4.2 和 Python 开发头文件。

当前原生 CTest 注册 11 个目标：

| 测试 | 主要验证 |
| --- | --- |
| test_xdata | 信号值、宽度、写入/内存边界 |
| test_xclock | 时钟推进、回调、控制权 |
| test_xtrigger | 注册、运行和事件协议 |
| test_xcoverage | engine 集成下的 Coverage |
| test_xmonitor | 检查器和挂接寿命 |
| test_xexpr | 表达式、typed 算术、回收 |
| test_xvpi | VPI 适配 |
| test_xfsm | 文本 FSM monitor |
| test_trigger_matcher | 私有匹配核心 |
| test_expr_allocations | 热表达式求值分配约束 |
| test_coverage_core | 独立统计策略与执行状态 |

## 按改动选择验证

信号/内存改动覆盖窄对象、非对齐、完整宽度及重绑定；时钟改动覆盖手动与 engine 接管、异常退出和对象销毁顺序；Coverage 改动核对分类、gate/abort、同次采样 Cross、计数布局和 matcher 共享。宽运算同时验证符号、非整 word 宽度、除零/移位和回收后 root 映射。

语言包装改动运行对应 tests/bindings；公共头改动用已安装 SDK 构建最小 C++ 客户端，避免源码 include 路径掩盖缺失安装头。TLM 集成在 tests/integration/tlm，需真实 SystemC/UVMC/模拟器；没有运行的环境应明确列为未验证。

## 提交与接口变更

PR 说明触发问题、最终行为、性能影响依据和实际检查范围。新 API 同时补[中英文参考](documentation.md)、完整示例、生命周期和错误语义。返回 bool、抛异常和 Fatal 不能在文档中混为一谈。

接口布局变化按[发布指南](releases.md)评估 ABI；Coverage 描述变化单独评估协议。未发布分支的一组改动共享一次 ABI 迁移，不按提交次数递增。PR 指向 master 时需要且只需要一个 release:patch/minor/major 标签。
