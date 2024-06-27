%module(directors="1") xspcomm

%{
/*
#cgo CXXFLAGS: -fcoroutines
*/
%}

%go_import("math/big")
%include <typemaps.i>
%include <std_vector.i>

%typemap(gotype) (FreePtr) "[]byte"
%typemap(in) (FreePtr) {
    $1.array = $input.array;
}

%inline %{
#include <vector>
typedef struct{
    void *array;
} FreePtr;
void FreeSCVector(FreePtr p){
    delete((std::vector<unsigned char> *)p.array);
}
%}

%typemap(gotype) std::vector<unsigned char> "big.Int"
%typemap(gotype) std::vector<unsigned char>& "*big.Int"
%typemap(imtype) std::vector<unsigned char> "[]byte"
%typemap(imtype) std::vector<unsigned char>& "[]byte"
%typemap(goin) (std::vector<unsigned char>&) {
    $result = $input.Bytes()
}
%typemap(in) (std::vector<unsigned char>&) {
    // convert slice to std::vector<unsigned char>
    std::vector<unsigned char> vec((unsigned char*)$input.array,
                                   (unsigned char*)$input.array + (size_t)$input.len);
    $1 = &vec;
}
%typemap(out) std::vector<unsigned char> {
    // convett std::vector to slice
    auto ret = new std::vector<unsigned char>($1);
    $result.array = ret->data();
    $result.len = ret->size();
    $result.cap = ret->size();
}
%typemap(goout) std::vector<unsigned char> {
    $result.SetBytes($1)
    FreeSCVector($1);
}

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
}
