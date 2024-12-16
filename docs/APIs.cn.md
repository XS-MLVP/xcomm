
## API 列表

### 通用C++ API（Python、Java、Scala、Golang 中都有对应的同名方法）

在每个支持的编程语言中都有对应的方法名称和类似的参数，部分参数类型根据语言有所变化，例如在C++中的std::vector<unsigned char>在Java中对应 byte[]。

**在下列API中，上标<sup>*</sup>的为常用 API**

-------------
#### XData 类

|编号|API名称|作用|参数说明|举例|
|-|-------------|----------|----------|----------|
|1|XData *SubDataRef(uint32_t start, uint32_t width, std::string name="")|拆分XData，例如把一个32位XData中的第7-10为<br>创建成为一个独立XData|name：名称，start：开始位，width：位宽|auto sub = a.SubDataRef(0, 4, "sub_pin")|
|2|WriteMode GetWriteMode()|获取XData的写模式，写模式有三种：Imme立即写，|2<br>Rise上升沿写，Fall下降沿写|-|-|
|3|bool SetWriteMode(WriteMode mode)|设置XData的写模式|写模式：Imme立即写，|2Rise上升沿写，<br>Fall下降沿写|a.SetWriteMode(WriteMode::Imme)|
|4<sup>*</sup>|bool DataValid()|检测数据是否有效（Value中含有X或者Z态返回false，<br>否者true）|-|-|
|5<sup>*</sup>|uint32_t W()|获取XData的位宽（如果为0，表示XData为logic或<br>者为Vec）|-||2-|
|6<sup>*</sup>|uint64_t U()|将XData的低64位转为uint64（无符号）|-|-|
|7<sup>*</sup>|int64_t S()|将XData的低64位转为int64（有符号）|-|-|
|8<sup>*</sup>|bool B()|将XData的低64位转为Bool类型|-|-|
|9<sup>*</sup>|std::string String()|将XData转位16进制的字符串类型|-|eg: "0x123ff"，如果出现?，表现对应的4bit中有x或z态|
|10|bool Equal(XData &xdata)|判断2个XData是否相等|XData类型|-|
|11<sup>*</sup>|XData &Set(T &data)|对XData进行赋值|T类型支持：XData, string, u/int, |2<br>u/int64, std::vector<unsigned char>|a.Set(0x123)|
|12<sup>*</sup>|std::vector<unsigned char> GetBytes()|以vector<uchar>格式获取XData中的数|2据|-|-|
|13|bool Connect(XData &xdata)|连接2个XData，只有In和Out类型的可以连接，当Out|2数据<br>发生变化时，会自动写入In|-|-|
|14|bool IsInIO()|判断XData是否为In类型，改类型可读可写|-|-|
|15|bool IsOutIO()|判断XData是否为Out类型，改类型只可读|-|-|
|16|bool IsBiIO()|判断XData是否为Bi类型，改类型可读可写|-|-|
|17|bool IsImmWrite()|判断XData是否为Imm写入模式|-|-|
|18|bool IsRiseWrite()|判断XData是否为Rise写入模式|-|-|
|19|bool IsFallWrite()|判断XData是否为Fall写入模式|-|-|
|20|XData &AsImmWrite()|更改XData的写模式为Imm|-|-|
|21|XData &AsRiseWrite()|更改XData的写模式为Rise|-|-|
|22|XData &AsFallWrite()|更改XData的写模式为Fall|-|-|
|23|XData &AsBiIO()|更改XData为Bi类型|-|-|
|24|XData &AsInIO()|更改XData为In类型|-|-|
|25|XData &AsOutIO()|更改XData为Out类型|-|-|
|26|XData &FlipIOType()|将XData的IO类型进行取反，例如In变为Out|-|-|
|27|XData &Invert()|将XData中的数据进行取反|-|-|
|28|PinBind &At(int index)|获取第i位，对第i位进行读写操作|目标位的index| a.At|2(2) = 1, a[2] = 1, uint8 bit = a.At(2)|
|29<sup>*</sup>|std::string AsBinaryString()|将XData的数据变为二进制字符串|-|eg: "1001011"|


#### XPort 类

|编号|API名称|作用|参数说明|举例|
|-|-------------|----------|----------|----------|
|1|XPort(std::string prefix = "")|创建XPort|prefix为改|1port对应的前缀|auto port = XPort("sinka.tile_link_")|
|2|int PortCount()|获取端口中的Pin数量（即绑定的XData个|1数）|-|-|
|3|bool Add(std::string pin, xspcomm::XData &pin_data)||1添加Pin|pin为名称，pin_data为电路对应的XData数据|p.Add|1("reset", dut.reset)|
|4|bool Del(std::string pin)|删除Pin|-|-|
|5|bool Connect(XPort &target)|链接2个Port，返回是否链接|1成功|-|-|
|6|XPort &NewSubPort(std::string subprefix)|创建子Port||1以subprefix开头的所有Pin构成子Port|-|
|7|xspcomm::XData &Get(std::string key, bool raw_key = |1false)|获取XData|key为Add时给的名称|-|
|8|XPort &FlipIOType()|让Port中所有XData的IO类型进行反转|-|--|
|9|XPort &AsBiIO()|让所有XData类IO类型为Bi|-|-|
|10|XPort &SetZero()|设置Port中的所有XData为0|-|-|
|11|&XPort &SelectPins(std::vector<std::string> pins);|创建子XPort|当前port中的引脚名称|-|

#### XClock 类

|编号|API名称|作用|参数说明|举例|
|-|-------------|----------|----------|----------|
|1|XClock &Add(XData &d)|将Clock和时钟进行绑定，具有别名：AddPin(XData &d)|d：被添加的XData|clock.Add(dut.clk)|
|2|XClock &Add(XPort &p)|将Clock和XData进行绑定|p：被添加的Port|clock.Add(dut.port)|
|3|void RefreshComb()|推进电路状态，不推进时间，不dump波形|-|-|
|4|void RefreshCombT()|推进电路状态，推进时间，dump波形|-|-|
|5|void Step(int s = 1)|推进电路s个时钟周期|-|-|
|6<sup>*</sup>|void StepRis(xfunction<void, u_int64_t, void *> func,<br> void |-*args = nullptr)|设置上升沿回调函数|func为回调函数，args为自定义参数|-|
|7<sup>*</sup>|void StepFal(xfunction<void, u_int64_t, void *> func,<br> void |-*args = nullptr)|设置下降沿回调函数|func为回调函数，args为自定义参数|-|
|8|void FreqDivWith(int div, XClock *clk, int shift=0)|对时钟进行分频|div: 分频数，目标clock频率将是本clock的div倍;<br>clk：目标Clock；<br>shift：频率偏移，偏移shift个半周期||
|9|void FreqDivDelete(XClock *clk)|删除对应分频率|||

### 特有API或者操作

根据编程语言特征或者编程习惯进行的定制 API

-------------

### C++ 

#### XClock 类 （异步API需要C++ 20 支持）
|编号|API名称|作用|参数说明|举例|
|-|-------------|----------|----------|----------|
|1<sup>*</sup>|XStep AStep(int i = 1)|异步等待i个时钟周期|-|co_wait |-clk.AStep(1)|
|2<sup>*</sup>|XCondition ACondition(std::function<bool(void)> |-checker)|异步等待checker返回为true|-|co_wait clk.|-ACondition([dut]()=>{return dut.rest == 1}) // 异步等|-待reset信号|
|3<sup>*</sup>|XNext ANext(int n = 1)|异步等待i个时钟周期|-|-|

#### XData 赋值与读取

C++可以进行赋值运算符重载，因此除了Set，U，S等操作外可以进行赋值取值，也可以通过“=”进行赋值和读取。

```c++
XData pin(32, XData.In);
pin = 1000;
pin = "0xff123";
pin = "0b110011";
pin = -1;                 // 所有bit赋值全1
int data = pin;
pin[15] = 1;              // 位操作
int bit_value = pin[15]
```

### Python

#### Python XClock 类

|编号|API名称|作用|参数说明|举例|
|-|-------------|----------|----------|----------|
|1<sup>*</sup>|async AStep(cycle: int)|异步推进cycle个时钟|执行的周期|-个数|await dut.AStep(5)
|2<sup>*</sup>|async ACondition(condition)|异步等待conditon()为|-true|可被调用的对象|await dit.ACondition(lambda : dut.|-reste == 1)|
|3<sup>*</sup>|async ANext()|异步推进一个时钟周期，等同AStep(1)|-|-|
|4<sup>*</sup>|async RunStep(cycle: int)|持续推进时钟cycle个时钟，用于最外层|-|-|

#### XData 赋值与读取

Python 无法进行运算符重载，但是可以重载 \__setattr__ 和 \__getattribute__，因此可以达到与C++类似效果。在Python中int为“大整数”类型，因此XData也默认支持大整数。

```python

pin = XData(32, XData.In)
pin.value = 1000
pin.value = -1
pin.value = "0x123fff"
data = pin.value       # .value 默认返回无符号数，若需要有符号数，用 data = pin.S()
pin[15] = 0            # 位操作
data = pin[15]
```


### Java，Scala

由于Scala兼容Java，因此Scala直接复用了Java的XData实现

#### XData类

|编号|API名称|作用|参数说明|举例|
|-|-------------|----------|----------|----------|
|1<sup>*</sup>|void Set(BigInteger v)| 赋值大整数，类似 python 中的 |pin.value = x|-|-|
|2<sup>*</sup>|BigInteger Get()| 获取大整数，返回正数，类似 python 中|的 x = pin.value|-|-|
|3<sup>*</sup>|BigInteger S()| 获取大整数，有符号类型，类似 python 中|的 x = pin.S()|-|-|
|4<sup>*</sup>|BigInteger U64()| 获取64位大整数，|-|-|
|5<sup>*</sup>|long S64()| long类型数据（long对应C++ int64）|-|-|


#### XData 赋值与读取

对于Scala语言，可以进行 “:=”重载，因此scala可以通过以下方式进行赋值。
```scala
var pin = new XData(32, IOType.Input)
pin := 123
pin := "0x123"
pin := "0b100101"
```

### Golang
#### XData 类

|编号|API名称|作用|参数说明|举例|
|-|-------------|----------|----------|----------|
|1<sup>*</sup>|Set(value:*big.Int)|赋值大整数|-|-|
|2<sup>*</sup>|Get() *big.Int|获取大整数，返回正数|-|-|
|3<sup>*</sup>|S() *big.Int|获取大整数，有符号类型|-|-|
|4<sup>*</sup>|U64() uint64| 获取64位无符号数|-|-|
|5<sup>*</sup>|S64() int64| 获取64位有符号数|-|-|


#### 由于不支持运算符重载，Java，Golang 的赋值、位操作等需要使用Get、Set、U、S、U64、S64、At等函数

``` go
pin := NewXData(129, IOType_Input)
pin.Set(123)            // 赋值
pin.Set("0x123")
pin.Set("0b10001")
pin.Get()               // 获取无符号大整数
pin.U64()               // 获取 uint64
pin.S64()               // 获取 int64
pin.S()                 // 获取有符号大整数
pin.U()                 // 同Get
pin.At(15).Get()        // 位操作：读取值
pin.At(15).Set(0)       // 位操作：赋值
```

**注**：XData(x, io_type) 可以看作位宽为x bit的“大整数”，其赋值和取值都可以看作 x bit 数与其他数据类型（int32、int64、bigInt、string等）的转换，因此存在溢出、无符号、有符号、位扩展等情况的处理。因此，对于Java/Scala/Golang这些支持BigInt类型的语言，当 x 小于等于64位时，建议用 S64() 或 U64() 进行读取，减小内部转换开销。
