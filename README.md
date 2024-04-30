#### xspcomm 介绍

xspcomm 为 picker 的公用数据定义与操作接口，包括接口读/写、时钟、协程、SWIG回调函数定义等。xspcomm以基础组件的方式被 DUT、MLVP、OVIP等上层应用或者库使用。xspcomm需要用到C++20的特征，建议使用g++ 11 以上版本， cmake 版本大于等于3.11。当通过SWIG导出Python接口时，需要 swig 版本大于等于 4.2.0。

**编译：**
通过make命令进行编译， 参数 BUILD_XSPCOMM_SWIG=python 可开启SWIG-Python支持 (目前swig接口支持python, javascript, java。同时开启多种语言：BUILD_XSPCOMM_SWIG=python,java,javascript)，编译完成后生成文件位于 build/lib：
```bash
lib/
├── include/xspcomm                # xspcomm 头文件
├── libxspcomm.so                    # xspcomm 动态连接库
└── python                       
    └── xspcomm                    # python模块（需要 make BUILD_XSPCOMM_SWIG=python）
        ├── __init__.py            # 模块入口文件
        ├── _pyxspcomm.so          # 二进制python库
        └── pyxspcomm.py           # SWIG生成的 python wrapper
```

**测试：**
make命令默认编译执行 tests/test_xdata.cpp。 若要运行 tests/tests_python.py 执行如下命令：
```bash
$make BUILD_XSPCOMM_SWIG=python
$make test_python
```

**打包与安装：（python wheel）**
默认打包so动态库和头文件，打包PYTHON需要开启 BUILD_XSPCOMM_SWIG=python
```bash
$pip install pipx
$cd xcomm
$BUILD_XSPCOMM_SWIG=python pipx run build      // 打包
```
打包完成后，会在dist目录生成 whl 安装包

安装测试
```bash
$pip install dist/pyxspcomm-0.0.1-cp310-cp310-manylinux_2_35_x86_64.whl
$python -m xspcomm.info --help      # 仅在开启 BUILD_XSPCOMM_SWIG=python 后才有该命令
Usage: python -m xspcomm.info [option]
--help:    print this help
--export:  print export cmd of XSPCOMM_ROOT, XSPCOMM_INCLUDE, XSPCOMM_LIB
--version: print python wrapper version
--root:    print xcomm root path
--include: print xcomm include path
--lib:     print xcomm lib path
--path:    print xcomm all path [default]
```

##### 一、基本数据操作

xspcomm 包含操作DUT（Design Under Test）的基本数据类型：**XData、XPort、XClock**三种数据定义。

1. **XData** 是对电路的IO接口的表示（即与电路引脚绑定），通过 DPI 读写电路的IO接口。支持 0，1，Z，X 四种状态写入与读取。定义位于xspcomm/xdaya.h

**XData 主要接口与成员变量：**

初始化
```c++
// 默认构造函数
XData();

// 重新初始化， width 表示引脚(Pin)宽度
//             itype 表示引脚(Pin)输入输出模式：Input、Output、InOut
//             name  表示引脚(Pin)名称
void ReInit(uint32_t width, IOType itype, std::string name = "");

// 同 ReInit
XData(uint32_t width, IOType itype, std::string name = "");
```

绑定DPI
```c++
void BindDPIRW(xfunction<void, void *> read, 
               xfunction<void, void *> write);
void BindDPIRW(xfunction<void, void *> read,
               xfunction<void, unsigned char> write);
```

设置引脚模式: WriteMode::Imme 立即写入, WriteMode::Rise 上升沿写入, WriteMode::Fall 下降沿写入。XData默认情况下为上升沿写入。
```c++
bool SetWriteMode(WriteMode mode);
```

赋值运算
```c++
XData a(128, IOType::Input);

a = 0x1234     // 直接赋值 int, unsigned int 等
a = "0b11011"  // 通过字符串进行二进制赋值
a = "0b1101z"  // 通过字符串进行二进制赋值，最后一位设置成高阻态
a = "0xfffff"  // 通过字符串进行八进制赋值
a = "::fffff"  // 直接赋值字符串ascii码
a = "X"        // 设置128位全为不定态

// 通过vector读写数据
uint8 buffer[128/8]
a.GetBits(buffer, sizeof(buffer));
a.SetBits(buffer, sizeof(buffer));

// 读取数据到buffer 或者 vector
std::vector<unsigned char> vector(buffer)
auto ret = a.GetVU8()
a.SetVU8(vector);

uint32_t x = a.W();    // 转 uint32
uint64_t x =a.U();     // 转 uint64
int64_t x = a.S();     // 转 int64
bool x = a.B();        // 转 bool
std::string a.String() // 转 string

XData b(128, IOType::Input);
a = b                  // 同类型赋值
```

成员变量
```c++
std::string mName;   // XData 名字
IOType mIOType;      // 输入输出类型
uint32_t mWidth;     // 位宽（bit）
```

2. **XPort** 是对XData的封装，可对多个XData进行操作

**XPort 主要接口与成员变量：**

初始化
```c++
XPort(std::string prefix = "");
```

主要方法
```c++
int PortCount();                                      // 引脚个数
bool Add(std::string pin, xspcomm::XData &pin_data);  // 添加引脚
bool Del(std::string pin);                            // 删除引脚
bool Connect(XPort &target);                          // 和另外一个port进行连接
XPort &NewSubPort(std::string subprefix);             // 创建子port
xspcomm::XData &operator[](std::string key);          // 按key获取引脚
xspcomm::XData &Get(std::string key, 
                    bool raw_key = false); // 同上
XPort &Flip();                             // port中所有引脚值进行反转
XPort &AsBiIO();                           // 把port中所有引脚设置为BiIO模式
XPort &WriteOnRise();                      // 通过DPI写入所有引脚的值（在clock的上升沿被调用）
XPort &WriteOnFall();                      // 通过DPI写入所有引脚的值（在clock的下降沿被调用）
XPort &ReadFresh(xspcomm::WriteMode m);    // 刷新引脚读书（通过DPI）
XPort &SetZero();                          // 设置所有引脚的值为 0
```

成员变量
```c++
std::string prefix;                                   // 引脚（XData）名称前缀
std::map<std::string, xspcomm::XData *> port_list;    // 引脚列表
```

3. **XClock** 是对电路的时钟的封装，用于驱动电路的clock，并在时钟的上升或者下降沿提供对应的回调入口

XClock 主要接口与成员变量：

初始化
```c++
// stepfunc 为 DUT后端提供的电路推进方法，例如 verilaor 的 step_eval 等
// xfunction<ret, Args...> 功能等价于 std::function<ret(Args...)>，主用于SWIG导出
XClock();
XClock(xfunction<int, bool> stepfunc,
       std::initializer_list<xspcomm::XData *> clock_pins  = {},
       std::initializer_list<xspcomm::XPort *> ports = {});
void ReInit(xfunction<int, bool> stepfunc,
       std::initializer_list<xspcomm::XData *> clock_pins  = {},
       std::initializer_list<xspcomm::XPort *> ports = {});

```

主要方法
```c++
void Add(xspcomm::XData &d);          // 添加 clk 引脚
void Add(xspcomm::XPort &d);          // 添加 port
void Step(int s = 1);                 // 推进时钟
void RunStep(int s = 1);              // 同上
void Reset();                         // 重置

// 添加上升沿回调
void StepRis(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
             std::string desc = "");
// 添加下降沿回调
void StepFal(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
             std::string desc = "");
// 异步方法
XStep AStep(int i = 1);                                   // 等待 i 个 step
XCondition ACondition(std::function<bool(void)> checker); // 条件等待
XNext ANext(int n = 1);                                   // 等待 i 个调度
void eval();                   // 推动电路执行，不更新波形（仅用于组合逻辑，慎用）
void eval_t();                 // 推动电路执行，更新波形（不建议使用）
```

##### 二、异步（协程）编程

xspcomm在clock类中提供AStep(int i = 1)、ACondition(std::function<bool(void)> checker)、ANext(int n = 1) 三个协程方法，可用于异步编程，示例伪代码如下：

```c++
#include "xspcomm/xclock"
#include "xspcomm/xcoroutine.h"

...

// 接收数据
xcoroutine<bool> send(XClock &clk, XPort &port){
    // 等待port中的 a_valid 信号拉高
    co_await clck.ACondition([&port](){return port["a_valid"]==1;});
    // 返回结果
    co_return port["data] == 0xffff;
}

// 循环接收，打印结果
xcoroutine<> infinite_loop(XClock &clk, XPort &port){
    while(true){
        auto ret = co_await send(clk, port);
        printf("send result is 0xffff: %s\n", ret ? "Yes":"No");
        // 等待下一个时钟
        co_await clk.AStep();
    }
}

int main(){
    ...
    // 开启协程
    infinite_loop(clk, port);
    // 推动1000个时钟（协程默认会在Step过程中进行调度）
    clk.Step(1000);
    return 0;
}

```

##### 三、python 编程

SWIG生成的python模块名称为 xspcomm，包含XData、XPort、XClock等基础类。同C++一样支持的异步方法有：XClock.AStep，XClock.ACondition，XClock.ANext。新增加异步方法 XClock.RunStep 用于驱动xclock，功能同同步模式下的 XClock.Step。

```python
from xspcomm import *
from asyncio import run, create_task

async def test_async():
    clk = XClock(lambda a: 0)
    clk.StepRis(lambda c : print("lambda ris: ", c))
    task = create_task(clk.RunStep(30))
    print("test      AStep:", clk.clk)
    await clk.AStep(3)
    print("test ACondition:", clk.clk)
    await clk.ACondition(lambda: clk.clk == 20)
    print("test        cpm:", clk.clk)
    await task

if __name__ == "__main__":
    print("version: %s" % version())
    run(test_async())

```

##### 四、通过 TLM 连通 UVM 与 Python

本项目提供基于 UVMC 的方式进行 UVM 和 Python（C++）的通信

以VCS为例，DUT示例编译骤如下：

```bash
...
# 1 通过swig生成python wrapper
$swig -D'MODULE_NAME="tlm_pbsb"' -python -c++ -DUSE_VCS -I${XSP_COMM_INCLUDE} \
-o tlmps.cpp ${XSP_COMM_INCLUDE}/xspcomm/python_tlm_pbsb.i

# 2 编译通信模块
$SYSCAN -full64 -cflags -DUSE_VCS -cflags -I{PYTHON_INCLUDE} \
-cflags -I${XSP_COMM_INCLUDE} ${XSP_COMM_INCLUDE}/xspcomm/tlm_pbsb.cpp tlmps.cpp

# 3 以slave模式编译 DUT
$VLOGAN -full64 +incdir+common tlm.sv +define+UVM_OBJECT_MUST_HAVE_CONSTRUCTOR
$VCS -e VcsMain -full64 sv_main ${VCS_HOME}/linux64/lib/vcs_tls.o -slave

# 4 重命名
$mv simv _tlm_pbsb.so
$mv simv.daidir _tlm_pbsb.so.daidir
...
```

编译完成后，可以通过 import 加载dut，具体例子请参考：tests/tlm

##### 五、其他可用接口

头文件 xspcomm/xutil.h 提供以下基本功能
```c++
LogLevel get_log_level()          // 获取当前日志 level
void set_log_level(LogLevel val)  // 设置当前日志 level
Info(fmt, ...)                    // 日志 Info
Warn(fmt, ...)                    // 日志 Warn
Error(fmt, ...)                   // 日志 Error
Debug(fmt, ...)                   // 日志 Debug
debug(c, fmt, ...)                // 日志 Debug，但c为true时打印日志
Fatal(fmt, ...)                   // 日志 Fatal, 打印日志，退出执行
Assert(c, fmt, ...)               // 断言，但c为false时，打印日志，退出执行

// 格式化字符串，eg: auto str = sFmt("Hello %s", "World!")
std::string sFmt(const std::string &format, Args... args)

// 数组转 hex （string 格式）
inline std::string sArrayHex(unsigned char *buff, int size)

// 判断string是否时 prefix开头
inline bool sWith(const std::string &str, const std::string &prefix)

//  判断vector是否含有 a
inline bool contians(std::vector<T> v, T a)

// 字符串转小写
inline std::string sLower(std::string input)

// 单位转换： int i 转 KB, MB, GB
inline std::string FmtSize(u_int64_t s)

// 编译优化指导
likely(cond)
unlikely(cond)

// 获取随机数
inline u_int64_t xRandom(u_int64_t a, u_int64_t b)

// 设置随机种子
inline void XSeed(unsigned int seed)

// 判断头文件版本与库版本是否匹配
inline bool checkVersion()

// 循环 n 次，，eg: FOR_COUNT(10){i++;}
FOR_COUNT(n) 

```

xinstance.h 仅仅用于实例化模板类，无其他作用。
