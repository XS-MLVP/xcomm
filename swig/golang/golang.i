%module(directors="1") xspcomm

%{
/*
#cgo CXXFLAGS: -fcoroutines
*/
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

%typemap(gotype) std::vector<unsigned char>  "*big.Int"
%typemap(gotype) std::vector<unsigned char>& "*big.Int"
%typemap(imtype) std::vector<unsigned char>  "[]byte"
%typemap(imtype) std::vector<unsigned char>& "[]byte"
%typemap(goin) (std::vector<unsigned char>&) %{
    if $input.Sign() == -1 {
        // Invert
        bytes := $input.Bytes()
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
        xdata := make([]byte, arg1.W())
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
        $result = xdata
    } else{
        $result = ReverseBytes($input.Bytes())
    }
%}
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
%typemap(goout) std::vector<unsigned char> %{
    // Get()(big.int) 只返回正数
    $result = new(big.Int).SetBytes(ReverseBytes($1))
%}

// For VU8
%typemap(gotype) VU8 "*big.Int"
%typemap(imtype) VU8 "[]byte"
%typemap(out) VU8 %{
    // convett std::vector to slice
    auto ret = new std::vector<unsigned char>($1.array);
    $result.array = ret->data();
    $result.len = ret->size();
    $result.cap = ret->size();
%}
%typemap(goout) VU8 %{
    signPos := arg1.W() - 1
    index := signPos / 8
    offst := signPos % 8
    if ($1[index] & (byte(1) << offst)) != 0 {
        // Negative
        $1[index] |= ^((byte(1) << (offst + 1)) - 1)
        array_size := uint(len($1))
        for i := index + 1; i < array_size; i ++ {
            $1[i] = byte(0xff)
        }
        // value = ^value
        for i := range $1 {
		    $1[i] = ^$1[i]
	    }
        // result = - (value + 1)
        $result = new(big.Int).Neg(new(big.Int).Add(new(big.Int).SetBytes(ReverseBytes($1)), big.NewInt(1)))
    }else{
        // Positive
        $result = new(big.Int).SetBytes(ReverseBytes($1))
    }
%}
%inline %{
typedef struct
{
    std::vector<unsigned char> array;
} VU8;
%}

%rename(S64) xspcomm::XData::S;   //FIXME: orgin S => S64
%rename(U64) xspcomm::XData::U;

%include ../xcomm.i

%extend xspcomm::XData {
    XData(int32_t width, IOType itype, std::string name = ""){
        return new xspcomm::XData((uint32_t)width, itype, name);
    }
    void ReInit(int32_t width, IOType itype, std::string name = ""){
        return self->ReInit((uint32_t)width, itype, name);
    }
    void Set(std::vector<unsigned char>& data){
        self->SetVU8(data);
    }
    std::vector<unsigned char> Get(){
        return self->GetVU8();
    }
    VU8 Signed(){
        VU8 ret;
        ret.array = self->GetVU8();
        return ret;
    }
}

%insert(go_wrapper) %{
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

%}
