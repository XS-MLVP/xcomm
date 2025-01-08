## Introduction

[中文文档](/README.cn.md)


xspcomm is the common data definition and operation interface for picker, including interface read/write, clock, coroutine, SWIG callback function definition, etc. xspcomm is used as a basic component by upper-level applications or libraries such as DUT, MLVP, OVIP. xspcomm requires features of C++20, it is recommended to use g++ version 11 or above, and cmake version greater than or equal to 3.11. When exporting Python interface through SWIG, swig version greater than or equal to 4.2.0 is required.

**Compile:**
Compile using the make command. Enable support for Python and other languages by specifying the BUILD_XSPCOMM_SWIG parameter (currently, the SWIG interface supports Python, JavaScript, Java, Scala, Go and Lua). Upon completion of compilation, the generated files will be located in the build directory:
```bash

make BUILD_XSPCOMM_SWIG=python,scala,java,golang,lua

# results 
build/python/
└── xspcomm             # python module
    ├── __init__.py
    ├── _pyxspcomm.so -> _pyxspcomm.so.0.0.1
    ├── _pyxspcomm.so.0.0.1
    ├── info.py
    └── pyxspcomm.py
build/scala/
└── xspcomm-scala.jar   # scala package
build/java/
└── xspcomm-java.jar    # java package
build/lua
├── luaxspcomm.so -> luaxspcomm.so.0.0.1
├── luaxspcomm.so.0.0.1
└── xspcomm.lua
build/golang/
└── src                 # golang module
    └── xspcomm
        ├── golangxspcomm.so -> golangxspcomm.so.0.0.1
        ├── golangxspcomm.so.0.0.1
        └── xspcomm.go
```

**Testing:**
The make command compiles and executes tests/test_xdata.cpp by default. To run tests/tests_python.py, execute the following commands:
```bash
$make BUILD_XSPCOMM_SWIG=python
$make test_python
```

**Packaging and Installation: (python wheel)**
By default, the so dynamic library and header files are packaged. To package PYTHON, you need to enable BUILD_XSPCOMM_SWIG=python
```bash
$pip install pipx
$cd xcomm
$BUILD_XSPCOMM_SWIG=python pipx run build      // Packaging
```
After packaging, a whl installation package will be generated in the dist directory

Install and test
```bash
$pip install dist/pyxspcomm-0.0.1-cp310-cp310-manylinux_2_35_x86_64.whl
$python -m xspcomm.info --help # This command is only available when
                               #  BUILD_XSPCOMM_SWIG=python is enabled
Usage: python -m xspcomm.info [option]
--help:    print this help
--export:  print export cmd of XSPCOMM_ROOT, XSPCOMM_INCLUDE, XSPCOMM_LIB
--version: print python wrapper version
--root:    print xcomm root path
--include: print xcomm include path
--lib:     print xcomm lib path
--path:    print xcomm all path [default]
```

## Usage of xspcomm
### I. Basic Data Operations

xspcomm includes basic data types for operating DUT (Design Under Test): **XData, XPort, XClock**.

1. **XData** represents the IO interface of the circuit (i.e., bound with the circuit pin), and reads and writes the IO interface of the circuit through DPI. It supports writing and reading of 0, 1, Z, X four states. The definition is located in xspcomm/xdaya.h.

**Main interfaces and member variables of XData:**

Initialization
```c++
// Default constructor
XData();

// Re-initialization, width represents the width of the pin
//   itype represents the input/output mode of the pin: Input, Output, InOut
//   name represents the name of the pin
void ReInit(uint32_t width, IOType itype, std::string name = "");

// Same as ReInit
XData(uint32_t width, IOType itype, std::string name = "");
```

Bind DPI
```c++
void BindDPIRW(xfunction<void, void *> read, 
               xfunction<void, void *> write);
void BindDPIRW(xfunction<void, void *> read,
               xfunction<void, unsigned char> write);
```

Set pin mode: WriteMode::Imme for immediate write, WriteMode::Rise for write on rising edge, WriteMode::Fall for write on falling edge. XData defaults to write on rising edge.
```c++
bool SetWriteMode(WriteMode mode);
```

Assignment operation
```c++
XData a(128, IOType::Input);

a = 0x1234     // Direct assignment int, unsigned int, etc.
a = "0b11011"  // Binary assignment through string
a = "0b1101z"  // Binary assignment through string, the last bit is set to high impedance
a = "0xfffff"  // Octal assignment through string
a = "::fffff"  // Direct assignment of string ascii code
a = "X"        // Set all 128 bits to undefined state

// Read and write data through vector
uint8 buffer[128/8]
a.GetBits(buffer, sizeof(buffer));
a.SetBits(buffer, sizeof(buffer));

// Read data to buffer or vector
std::vector<unsigned char> vector(buffer)
auto ret = a.GetBytes()
a.SetBytes(vector);

uint32_t x = a.W();    // Convert to uint32
uint64_t x =a.U();     // Convert to uint64
int64_t x = a.S();     // Convert to int64
bool x = a.B();        // Convert to bool
std::string a.String() // Convert to string

XData b(128, IOType::Input);
a = b                  // Assignment of the same type
```

Member variables
```c++
std::string mName;   // XData name
IOType mIOType;      // Input/output type
uint32_t mWidth;     // Bit width
```

2. **XPort** is a wrapper for XData, which can operate multiple XData

**Main interfaces and member variables of XPort:**

Initialization
```c++
XPort(std::string prefix = "");
```

Main methods
```c++
int PortCount();                                      // Number of pins
bool Add(std::string pin, xspcomm::XData &pin_data);  // Add pin
bool Del(std::string pin);                            // Delete pin
bool Connect(XPort &target);                          // Connect with another port
XPort &NewSubPort(std::string subprefix);             // Create subport
xspcomm::XData &operator[](std::string key);          // Get pin by key
xspcomm::XData &Get(std::string key, 
                    bool raw_key = false); // Same as above
XPort &Flip();            // Flip all pin values in the port
XPort &AsBiIO();          // Set all pins in the port to BiIO mode
XPort &WriteOnRise();     // Write all pin values through DPI (called on clock's rising edge)
XPort &WriteOnFall();    // Write all pin values through DPI (called on clock's falling edge)
XPort &ReadFresh(xspcomm::WriteMode m);    // Refresh pin reading (through DPI)
XPort &SetZero();                          // Set all pin values to 0
```

Member variables
```c++
std::string prefix;                                   // Prefix of pin (XData) name
std::map<std::string, xspcomm::XData *> port_list;    // Pin list
```

3. **XClock** is a wrapper for the circuit's clock, used to drive the circuit's clock, and provides corresponding callback entries at the rising or falling edge of the clock

Main interfaces and member variables of XClock:

Initialization
```c++
// stepfunc is the circuit advancement method
// provided by DUT backend, such as verilaor's step_eval, etc.
// xfunction<ret, Args...> is functionally equivalent
// to std::function<ret(Args...)>, mainly used for SWIG export
XClock();
XClock(xfunction<int, bool> stepfunc,
       std::initializer_list<xspcomm::XData *> clock_pins  = {},
       std::initializer_list<xspcomm::XPort *> ports = {});
void ReInit(xfunction<int, bool> stepfunc,
       std::initializer_list<xspcomm::XData *> clock_pins  = {},
       std::initializer_list<xspcomm::XPort *> ports = {});

```

Main methods
```c++
void Add(xspcomm::XData &d);          // Add clk pin
void Add(xspcomm::XPort &d);          // Add port
void Step(int s = 1);                 // Advance clock
void RunStep(int s = 1);              // Same as above
void Reset();                         // Reset

// Add rising edge callback
void StepRis(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
             std::string desc = "");
// Add falling edge callback
void StepFal(xfunction<void, u_int64_t, void *> func, void *args = nullptr,
             std::string desc = "");
// Asynchronous methods
XStep AStep(int i = 1);                                   // Wait for i steps
XCondition ACondition(std::function<bool(void)> checker); // Conditional wait
XNext ANext(int n = 1);                                   // Wait for i scheduling
void eval();   // Drive circuit execution, do not update waveform (only for combinational logic)
void eval_t(); // Drive circuit execution, update waveform (not recommended)
```

### II. Asynchronous (Coroutine) Programming

The `xspcomm` provides three coroutine methods in the `clock` class: `AStep(int i = 1)`, `ACondition(std::function<bool(void)> checker)`, `ANext(int n = 1)`, which can be used for asynchronous programming. Here is an example pseudocode:

```c++
#include "xspcomm/xclock"
#include "xspcomm/xcoroutine.h"

...

// Receive data
xcoroutine<bool> send(XClock &clk, XPort &port){
    // Wait for the a_valid signal in port to go high
    co_await clck.ACondition([&port](){return port["a_valid"]==1;});
    // Return result
    co_return port["data"] == 0xffff;
}

// Loop receive, print result
xcoroutine<> infinite_loop(XClock &clk, XPort &port){
    while(true){
        auto ret = co_await send(clk, port);
        printf("send result is 0xffff: %s\n", ret ? "Yes":"No");
        // Wait for the next clock
        co_await clk.AStep();
    }
}

int main(){
    ...
    // Start coroutine
    infinite_loop(clk, port);
    // Push 1000 clocks (coroutines will be scheduled by default during the Step process)
    clk.Step(1000);
    return 0;
}

```

### III. Python Programming

The Python module generated by SWIG is named `xspcomm`, which includes basic classes such as `XData`, `XPort`, `XClock`. It supports the same asynchronous methods as C++: `XClock.AStep`, `XClock.ACondition`, `XClock.ANext`. The new asynchronous method `XClock.RunStep` is added to drive `xclock`, which has the same function as `XClock.Step` in synchronous mode.

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

### IV. Connecting UVM and Python through TLM

This project provides a way to communicate between UVM and Python (C++) based on UVMC.

Taking VCS as an example, the compilation steps of the DUT example are as follows:

```bash
...
# 1 Generate python wrapper through swig
$swig -D'MODULE_NAME="tlm_pbsb"' -python -c++ -DUSE_VCS -I${XSP_COMM_INCLUDE} \
-o tlmps.cpp ${XSP_COMM_INCLUDE}/xspcomm/python_tlm_pbsb.i

# 2 Compile the communication module
$SYSCAN -full64 -cflags -DUSE_VCS -cflags -I{PYTHON_INCLUDE} \
-cflags -I${XSP_COMM_INCLUDE} ${XSP_COMM_INCLUDE}/xspcomm/tlm_pbsb.cpp tlmps.cpp

# 3 Compile DUT in slave mode
$VLOGAN -full64 +incdir+common tlm.sv +define+UVM_OBJECT_MUST_HAVE_CONSTRUCTOR
$VCS -e VcsMain -full64 sv_main ${VCS_HOME}/linux64/lib/vcs_tls.o -slave

# 4 Rename
$mv simv _tlm_pbsb.so
$mv simv.daidir _tlm_pbsb.so.daidir
...
```

After compilation, you can load the DUT through import. For specific examples, please refer to: tests/tlm

### V. Other Available Interfaces

The header file xspcomm/xutil.h provides the following basic functionalities:

```c++
LogLevel get_log_level()          // Get the current log level
void set_log_level(LogLevel val)  // Set the current log level
Info(fmt, ...)                    // Log Info
Warn(fmt, ...)                    // Log Warn
Error(fmt, ...)                   // Log Error
Debug(fmt, ...)                   // Log Debug
debug(c, fmt, ...)                // Log Debug, but print log when c is true
Fatal(fmt, ...)                   // Log Fatal, print log, then exit execution
Assert(c, fmt, ...)               // Assertion, print log and exit execution when c is false

// Format string, eg: auto str = sFmt("Hello %s", "World!")
std::string sFmt(const std::string &format, Args... args)

// Convert array to hex (string format)
inline std::string sArrayHex(unsigned char *buff, int size)

// Check if string starts with prefix
inline bool sWith(const std::string &str, const std::string &prefix)

// Check if vector contains a
inline bool contains(std::vector<T> v, T a)

// Convert string to lowercase
inline std::string sLower(std::string input)

// Unit conversion: convert int i to KB, MB, GB
inline std::string FmtSize(u_int64_t s)

// Compiler optimization guidance
likely(cond)
unlikely(cond)

// Get a random number
inline u_int64_t xRandom(u_int64_t a, u_int64_t b)

// Set random seed
inline void XSeed(unsigned int seed)

// Check if the header file version matches the library version
inline bool checkVersion()

// Loop n times, eg: FOR_COUNT(10){i++;}
FOR_COUNT(n) 

```

xinstance.h is only used for instantiating template classes and has no other functions.