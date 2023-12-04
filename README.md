#### xcomm 介绍

xcomm 为 xspecker 的公用数据定义与操作接口，包括接口读/写、时钟、协程、SWIG回调函数定义等。xcomm以基础组件的方式被 DUT、MLVP、OVIP等上层应用或者库使用。xcomm需要用到C++20的特征，建议使用g++ 11 以上版本， cmake 版本大于等于3.11。当通过SWIG导出Python接口时，需要 swig 版本大于等于 4.2.0。

**编译：**
通过make命令进行编译， 参数 BUILD_SWIG=ON 可开启SWIG-Python支持，编译完成后生成文件位于 build/lib：
```
lib/
├── include/xcomm                # xcomm 头文件
├── libxcomm.so                  # xcomm 动态连接库
└── python                       
    └── xcomm                    # python模块（需要 make BUILD_SWIG=ON）
        ├── __init__.py          # 模块入口文件
        ├── _pyxcomm.so          # 二进制python库
        └── pyxcomm.py           # SWIG生成的 python wrapper
```

**测试：**
make命令默认编译执行 tests/test_xdata.cpp。 若要运行 tests/tests_python.py 执行如下命令：
```
$make BUILD_SWIG=ON
$make test_python
```

##### 一、基本数据操作

xcomm 包含操作DUT（Design Under Test）的基本数据类型：**XData、XPort、XClock**三种数据定义。

1. **XData** 电路的IO接口数据，通过 DPI 读写电路的IO接口。定义位于xcomm/xdaya.h

**XData 主要接口与成员变量：**
初始化
```
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
```
void BindDPIRW(xfunction<void, void *> read, 
               xfunction<void, void *> write);
void BindDPIRW(xfunction<void, void *> read,
               xfunction<void, unsigned char> write);
```

赋值运算
```
XData a(128, IOType::Input);

a = 0x1234     // 直接赋值 int, unsigned int 等
a = "0b11011"  // 通过字符串进行二进制赋值
a = "0xfffff"  // 通过字符串进行八进制赋值
a = "::fffff"  // 直接赋值字符串ascii码

uint32_t x = a.W();    // 转 uint32
uint64_t x =a.U();     // 转 uint64
int64_t x = a.S();     // 转 int64
bool x = a.B();        // 转 bool
std::string a.String() // 转 string

XData b(128, IOType::Input);
a = b                  // 同类型赋值
```

成员变量
```
std::string mName;   // XData 名字
IOType mIOType;      // 输入输出类型
uint32_t mWidth;     // 位宽（bit）
```

2. **XPort** XData的封装，可对多个XData进行操作

XPort 主要接口与成员变量：
```

```

3. **XClock** 基于XPort，对电路的时钟封装，用于驱动电路

XClock 主要接口与成员变量：
```
```


##### 二、异步（协程）编程

TBD

##### 三、SWIG回调（python）

TBD

##### 四、其他可用接口

头文件 xcomm/xutil.h 提供以下基本功能
```
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
