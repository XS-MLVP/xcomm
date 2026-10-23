# 公共 API 索引

本索引按职责列出入口，具体参数、返回值、生命周期和限制见对应模块页。方法族中的多个重载放在一起解释；构造、析构和运算符在所属对象文档中说明。

| 模块 | 公开类型/入口 | 详细说明 |
| --- | --- | --- |
| 信号 | XData、PinBind、IOType、WriteMode、XDataBackendKind、xsvLogic、xsvLogicVecVal | [XData](xdata.md) |
| 端口 | XPort | [XPort](xport.md) |
| 时钟 | XClock、FastMode、XStep | [XClock](xclock.md) |
| 表达式 | ExprEngine、ExprOp、ExprNode、NewBits、EvalBits、Collect | [ExprEngine](expressions.md) |
| 原生运行时 | XEngine / XTriggerEngine | [XEngine](engine.md) |
| 运行结果 | XPhase、XHitKind、XStopReason、XRegistrationHandle、XBackendHit、XRunResult | [XEngine](engine.md) |
| 匹配描述 | XConditionMode、XSequenceStepKind、XSequenceStep、XFsmTransition、XTriggerProgramKind、XTriggerProgram | [Trigger](triggers.md) |
| 覆盖率 | XCoveragePointKind、XCoverageBinKind、XCoveragePoint、XCoverageBin、XCoverageSnapshot | [Coverage](coverage.md) |
| 检查器 | XStepCallback、XEcho、XConditionCheck、XExprCheck、XFsmMonitor、XRangeCheck | [Monitor](monitors.md) |
| 布局 | XSignalCFG、s_xsignal_cfg、s_xsignal_meta | [配置](signal-config.md) |
| 内存/字符串 | XByteBuffer、CString、数组与指针辅助函数 | [公共工具](utilities.md) |
| 通用比较 | CompareOp、compare::Compare/Order/CompareWords/EqualWords/CompareSignedBytes/WithinRange | [公共工具](utilities.md) |
| 精确内存访问 | memory::Load/Store、UIntBytes、ByteWords | [公共工具](utilities.md) |
| 回调与协程 | xfunction、XCorutine/xcorutine、XPromise、XCondition、XNext、schedule_awit | [公共工具](utilities.md) |
| TLM | tlm_msg、TLMSub、TLMPub、tlm_pbsb_run、step、tlm_vcs_init/tlm_vcs_step | [TLM](tlm.md) |
| VPI ABI | vpiHandle、s_vpi_value/s_vpi_vecval/s_vpi_time、func_vpi_get/get_value/put_value/get_str | [XData](xdata.md)，定义见 [xvpi.h](https://github.com/XS-MLVP/xcomm/blob/v1.0.0/include/xspcomm/xvpi.h) |
| 版本/日志 | version、abi_version、checkVersion、日志及格式化工具 | [公共工具](utilities.md) |
| Python 扩展 | value、BindDPIName、RawStep、asyncio 辅助、info、thirdcall | [语言绑定](bindings.md) |

## XEngine 方法速查

| 方法 | 功能 |
| --- | --- |
| AcquireClock、ReleaseClock | 控制权 |
| ArmEdge、ArmClockCycles、ArmValueEq、ArmValueEqBytes、ArmValueChange、ArmSample、ArmExpr、ArmSequence、ArmFsm | [普通事件注册](triggers.md) |
| ExprNewConst、ExprNewSignal、ExprNewUnary、ExprNewBinary、ExprNewCompare、ExprNewCompareSigSig、ExprNewCompareSigConstBytes、ExprNewCompareConstBytesSig、ExprNewMaskedCompareSigConstBytes | [engine 内部表达式](engine.md) |
| RunUntil、SamplePhase | 执行与相位采样 |
| Disarm、Rearm、RearmSample、ActiveCount、Capacity、ClearExecutionState、Clear | 注册及缓存管理 |
| CoverageVersion、AttachCoverage、CoverageSnapshot、ResetCoverage、CoverageExecutionCount | [原生 Coverage](coverage.md) |

## 头文件选择

普通应用可用 xcomm.h 汇总核心 API；需要检查器时使用 xmonitor.h。减少依赖时直接包含所属头文件：xdata.h、xport.h、xclock.h、xexpr.h、xengine.h、xpattern.h、xcoverage.h、xsignal_cfg.h，以及 monitor/*、tlm/*、common/*。

runtime/types.h 仅定义运行协议数据类型；它不要求引入整个时钟和运行时实现。xtrigger.h 提供历史入口别名。

ExprNode 的 lhs/rhs/imm/sig 等成员是图节点的底层表示；一般通过 New* 方法创建节点，不手工编辑其历史字段。XData/XClock 的以下划线开头辅助方法、TEST_* 函数、private src 头文件及 SWIG director 存储类不作为应用 API 目录推广。

## v1.0 宽表达式与回收入口

`NewBitsConst/NewBool/NewBitsSignal/NewBits/EvalBits`、`NodeCount/ConstantCount/Collect` 属于 ExprEngine。XEngine 提供 `ExprNew*` 对应包装和 `ExpressionNodeCount/ExpressionConstantCount/CollectExpressions`。Python 的 root 映射使用 XIntVector。见[表达式参考](expressions.md)与[实际使用指南](../guides/typed-expressions.md)。
