%module(directors="1") pyxcomm

%{
#include "xcomm/xclock.h"
#include "xcomm/xdata.h"  
#include "xcomm/xport.h"
#include "xcomm/xcallback.h"
%}

%feature("director") xfunction;

%apply unsigned long long {u_int64_t}
%apply unsigned int {u_uint32_t}
%apply unsigned short {u_uint16_t}
%apply unsigned char {u_uint8_t}

%apply unsigned long long {uint64_t}
%apply unsigned int {uint32_t}
%apply unsigned short {uint16_t}
%apply unsigned char {uint8_t}

%apply long long {i_int64_t}
%apply int {i_int32_t}
%apply short {i_int16_t}
%apply char {i_int8_t}

%apply long long {int64_t}
%apply int {int32_t}
%apply short {int16_t}
%apply char {int8_t}

%include std_string.i
%include std_map.i
%include std_vector.i

%typemap(in) std::vector<unsigned char>& (std::vector<unsigned char> temp) {
    if (!PyBytes_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expected a bytes object");
        return NULL;
    }
    char* buffer;
    Py_ssize_t length;
    PyBytes_AsStringAndSize($input, &buffer, &length);
    temp = std::vector<unsigned char>(buffer, buffer + length);
    $1 = &temp;
}

%typemap(out) std::vector<unsigned char> {
    PyObject* bytes = PyBytes_FromStringAndSize((char*)$1.data(), $1.size());
    $result = bytes;
}

%include "xcomm/xclock.h"
%include "xcomm/xdata.h"
%include "xcomm/xport.h"

%constant void (*_TEST_DPI_LR)(xsvLogic *v) = xcomm::TEST_DPI_LR;
%constant void (*_TEST_DPI_LW)(xsvLogic v) = xcomm::TEST_DPI_LW;
%constant void (*_TEST_DPI_VR)(xsvLogicVecVal *v) = xcomm::TEST_DPI_VR;
%constant void (*_TEST_DPI_VW)(xsvLogicVecVal *v) = xcomm::TEST_DPI_VW;

// Note: need include callback after director
%include "xcomm/xcallback.h"

%define %x_callback(Name, Ret, T...)
%template(Name) xfunction<Ret,T>;
%enddef

// XData
%x_callback(cb_void_bool_XDatap_u43_voidp, void, bool, xcomm::XData*, u_int32_t, void *);
%x_callback(cb_void_xsvLogicp, void, xcomm::xsvLogic *);
%x_callback(cb_void_xsvLogic, void, xcomm::xsvLogic);
%x_callback(cb_void_xsvlogicVecValp, void, xcomm::xsvLogicVecVal *);

// XClock
%x_callback(cb_int_bool, int, bool);

%x_callback(cb_void_u64_voidp, void, u_int64_t, void *); // StepRis, StepFal
