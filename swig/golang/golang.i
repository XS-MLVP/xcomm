%module(directors="1") xspcomm

%{
/*
#cgo CXXFLAGS: -fcoroutines
*/
%}

%insert(cgo_comment) %{
#cgo LDFLAGS: ${SRCDIR}/golangxspcomm.so
%}

%go_import("math/big", "fmt")
%include <typemaps.i>
%include <std_vector.i>

%typemap(gotype) (FreePtr) "[]byte"
%typemap(in) (FreePtr) %{
    $1.array = $input.array;
%}

%inline %{
#include <vector>
typedef struct{
    void *array;
} FreePtr;
void FreeSCVector(FreePtr p){
    delete((std::vector<unsigned char> *)p.array);
}
%}

%typemap(gotype) std::vector<unsigned char>  "[]byte"
%typemap(gotype) std::vector<unsigned char>& "[]byte"
%typemap(in) (std::vector<unsigned char>&) %{
    // convert slice to std::vector<unsigned char>
    std::vector<unsigned char> vec((unsigned char*)$input.array,
                                   (unsigned char*)$input.array + (size_t)$input.len);
    $1 = &vec;
%}
%typemap(out) std::vector<unsigned char> %{
    // convett std::vector to slice
    auto ret = new std::vector<unsigned char>($1);
    $result.array = ret->data();
    $result.len = ret->size();
    $result.cap = ret->size();
%}

%rename(S64) xspcomm::XData::S;   //FIXME: orgin S => S64
%rename(U64) xspcomm::XData::U;
%rename(AsImmWriteGo) xspcomm::XData::AsImmWrite;
%rename(AsRiseWriteGo) xspcomm::XData::AsRiseWrite;
%rename(AsFallWriteGo) xspcomm::XData::AsFallWrite;
%rename(AsBiIOGo) xspcomm::XData::AsBiIO;
%rename(AsInIOGo) xspcomm::XData::AsInIO;
%rename(AsOutIOGo) xspcomm::XData::AsOutIO;
%rename(FlipIOTypeGo) xspcomm::XData::FlipIOType;
%rename(InvertGo) xspcomm::XData::Invert;
%rename(SubDataRefGo) xspcomm::XData::SubDataRef;
%rename(SubDataRefRawGo) xspcomm::XData::SubDataRefRaw;

%rename(StepRisC) xspcomm::XClock::StepRis(u_int64_t, u_int64_t, std::string);
%rename(StepFalC) xspcomm::XClock::StepFal(u_int64_t, u_int64_t, std::string);
%rename(StepRisX) xspcomm::XClock::StepRis(xfunction<void, u_int64_t, void *>, void *args, std::string);
%rename(StepFalX) xspcomm::XClock::StepFal(xfunction<void, u_int64_t, void *>, void *args, std::string);

%rename(SetGo) xspcomm::XData::Set;
%rename(XClockGo) xspcomm::XClock;
%rename(XDataGo) xspcomm::XData;

%include ../xcomm.i


%insert(go_wrapper) %{

type XData interface {
    XDataGo
    Set(a ...interface{}) XData
    Get() *big.Int
    S() *big.Int
    U() *big.Int
    AsImmWrite() XData
    AsRiseWrite() XData
    AsFallWrite() XData
    AsBiIO() XData
    AsInIO() XData
    AsOutIO() XData
    FlipIOType() XData
    Invert() XData
}

type SwigcptrXData struct {
    XDataGo
}

func XDataFromVPI(a ...interface{}) XData {
    ret := SwigcptrXData{}
    ret.XDataGo = XDataGoFromVPI(a...)
    return ret
}

func (p SwigcptrXData) SubDataRef(a ...interface{}) XData {
    ret := SwigcptrXData{}
    ret.XDataGo = p.XDataGo.SubDataRefRawGo(a...)
    return ret
}

func (p SwigcptrXData) AsImmWrite() XData {
    p.XDataGo.AsImmWriteGo()
    return p
}

func (p SwigcptrXData) AsRiseWrite() XData {
    p.XDataGo.AsRiseWriteGo()
    return p
}

func (p SwigcptrXData) AsFallWrite() XData {
    p.XDataGo.AsFallWriteGo()
    return p
}

func (p SwigcptrXData) AsBiIO() XData {
    p.XDataGo.AsBiIOGo()
    return p
}

func (p SwigcptrXData) AsInIO() XData {
    p.XDataGo.AsInIOGo()
    return p
}

func (p SwigcptrXData) AsOutIO() XData {
    p.XDataGo.AsOutIOGo()
    return p
}

func (p SwigcptrXData) FlipIOType() XData {
    p.XDataGo.FlipIOTypeGo()
    return p
}

func (p SwigcptrXData) Invert() XData {
    p.XDataGo.InvertGo()
    return p
}

func (p SwigcptrXData) BindDPIPtr(r uint64, w uint64){
    p.XDataGo.BindDPIPtr(r, w)
}

func NewXData(a ...interface{}) XData {
    ret := SwigcptrXData{}
    argc := len(a)
    if argc == 2 {
        ret.XDataGo = NewXDataGo(uint(a[0].(int)), a[1])
    } else if argc == 3 {
        ret.XDataGo = NewXDataGo(uint(a[0].(int)), a[1], a[2])
    }else{
        ret.XDataGo = NewXDataGo(a...)
    }
    return ret
}

func (p SwigcptrXData) Get() *big.Int {
    if p.XDataGo.W() <= 64 {
        data := big.NewInt(1)
        data.SetUint64(p.XDataGo.U64())
        return data
    }
    return new(big.Int).SetBytes(ReverseBytes(p.XDataGo.GetBytes()))
}

func (p SwigcptrXData) U() *big.Int {
    return p.Get()
}

func (p SwigcptrXData) S() *big.Int {
    if p.XDataGo.W() <= 64 {
        return big.NewInt(p.XDataGo.S64())
    }
    bytes := p.XDataGo.GetBytes()
    signPos := p.XDataGo.W() - 1
    index := signPos / 8
    offst := signPos % 8
    if (bytes[index] & (byte(1) << offst)) != 0 {
        // Negative
        bytes[index] |= ^((byte(1) << (offst + 1)) - 1)
        array_size := uint(len(bytes))
        for i := index + 1; i < array_size; i ++ {
            bytes[i] = byte(0xff)
        }
        // value = ^value
        for i := range bytes {
            bytes[i] = ^bytes[i]
        }
        // result = - (value + 1)
        return new(big.Int).Neg(new(big.Int).Add(new(big.Int).SetBytes(ReverseBytes(bytes)), big.NewInt(1)))
    }else{
        // Positive
        return new(big.Int).SetBytes(ReverseBytes(bytes))
    }
}

func (p SwigcptrXData) Set(a ...interface{}) XData {
    argc := len(a)
    if argc > 0{
        big_value, ok := a[0].(*big.Int)
        if ok {
            // big to bytes
            if big_value.Sign() == -1 {
                // Invert
                bytes := big_value.Bytes()
                for i := range bytes {
                        bytes[i] = ^bytes[i]
                }
                // Plus 1
                for i := len(bytes) - 1; i >= 0; i-- {
                        bytes[i]++
                        if bytes[i] != 0 {
                            break
                        }
                    }
                bytes = ReverseBytes(bytes)
                // Copy to XData array
                xdata := make([]byte, p.XDataGo.W())
                for j := range xdata {
                    xdata[j] = byte(0xff)
                }
                // Find min length to copy
                len_b := len(bytes)
                len_x := len(xdata)
                copy_len := len_b
                if copy_len > len_x {
                    copy_len = len_x
                }
                // Copy
                for j := 0; j < copy_len; j++ {
                    xdata[j] = bytes[j]
                }
               p.XDataGo.SetBytes(xdata)
            } else{
               p.XDataGo.SetBytes(ReverseBytes(big_value.Bytes()))
            }
            return p
        }
    }
    p.XDataGo.SetGo(a...)
    return p
}

func ReverseBytes(data []byte) []byte {
    result := make([]byte, len(data))
    for i, b := range data {
        result[len(data)-1-i] = b
    }
    return result
}

func EchoBytes(bytes []byte) {
 for _, b := range bytes {
        fmt.Printf("%08b ", b)
    }
    fmt.Println()
}

type StepFunc struct{
    Cb_int_bool
    call_back func(bool) int
}

type StepCb struct{
    Cb_void_u64_voidp
    call_back func(uint64)
}

func (self *StepCb) Call(cycle uint64, parg uintptr) {
    self.call_back(cycle)
}

func (self *StepFunc) Call(dump bool) int {
    return self.call_back(dump)
}

func NewStepCb(f func(uint64)) Cb_void_u64_voidp {
  ret := StepCb{call_back: f}
  cb := NewDirectorCb_void_u64_voidp(&ret)
  cb.Set_force_callable()
  return cb
}

func NewStepFunc(f func(bool) int) Cb_int_bool {
  ret := StepFunc{call_back: f}
  cb := NewDirectorCb_int_bool(&ret)
  cb.Set_force_callable()
  return cb
}

type XClock interface {
    XClockGo
}

type SwigcptrXClock struct{
    XClockGo
}

func NewXClock(a ...interface{}) XClock {
    ret := SwigcptrXClock {}
    var p XClockGo
    argc := len(a)
    if f, ok := a[0].(uint64); ok {
        p = NewXClockGo(f, a[1].(uint64))
    }else if argc == 1 {
        p = NewXClockGo(NewStepFunc(a[0].(func(bool) int)))
    }else if argc == 2 {
        p = NewXClockGo(NewStepFunc(a[0].(func(bool) int)), a[1])
    }else if argc == 3 {
        p = NewXClockGo(NewStepFunc(a[0].(func(bool) int)), a[1], a[2])
    }else{
        p = NewXClockGo(a...)
    }
    ret.XClockGo = p
    return ret
}


func (p SwigcptrXClock) ReInit(a ...interface{}) {
    argc := len(a)
    if argc == 1 {
        p.XClockGo.ReInit(NewStepFunc(a[0].(func(bool) int)))
        return
    }
    if argc == 2 {
        p.XClockGo.ReInit(NewStepFunc(a[0].(func(bool) int)), a[1])
        return
    }
    if argc == 3 {
        p.XClockGo.ReInit(NewStepFunc(a[0].(func(bool) int)), a[1], a[2])
        return
    }
    panic("No match for overloaded function call")
}


func (p SwigcptrXClock) StepRis(a ...interface{}) {
    argc := len(a)
    if f, ok := a[0].(func(uint64)); ok {
        p.XClockGo.StepRisX(NewStepCb(f), 0, "")
        return
    }
    if argc == 1 {
        p.XClockGo.StepRisC(a[0].(uint64), 0, "")
        return
    }
    if argc == 2 {
        p.XClockGo.StepRisC(a[0].(uint64), a[1].(uint64), "")
        return
    }
    if argc == 3 {
        p.XClockGo.StepRisC(a[0].(uint64), a[1].(uint64), a[2].(string))
        return
    }
    panic("No match for overloaded function call")
}

func (p SwigcptrXClock) StepFal(a ...interface{}) {
    argc := len(a)
    if f, ok := a[0].(func(uint64)); ok {
        p.XClockGo.StepFalX(NewStepCb(f), 0, "")
        return
    }
    if argc == 1 {
        p.XClockGo.StepFalC(a[0].(uint64), 0, "")
        return
    }
    if argc == 2 {
        p.XClockGo.StepFalC(a[0].(uint64), a[1].(uint64), "")
        return
    }
    if argc == 3 {
        p.XClockGo.StepFalC(a[0].(uint64), a[1].(uint64), a[2].(string))
        return
    }
    panic("No match for overloaded function call")
}

%}
