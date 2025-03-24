
%{
#include "xspcomm/xclock.h"
#include "xspcomm/xdata.h"  
#include "xspcomm/xport.h"
#include "xspcomm/xcallback.h"
#include "xspcomm/xsignal_cfg.h"
#include "xspcomm/xcomuse.h"
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

// pointers
%apply unsigned long long * { uint64_t * }
%apply unsigned int * {uint32_t *}
%apply unsigned short * {uint16_t *}
%apply unsigned char * {uint8_t *}
%apply unsigned long long [] { uint64_t [] }
%apply unsigned int [] {uint32_t [] }
%apply unsigned short [] {uint16_t [] }
%apply unsigned char [] {uint8_t [] }

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
#if !defined(SWIGGO) && !defined(SWIGLUA)
%include std_shared_ptr.i
%shared_ptr(xspcomm::XData)
#endif

%ignore xspcomm::XPort::SelectPins(std::initializer_list<std::string>);

namespace std {
   %template(StringVector) vector<string>;
   %template(XDataVector) vector<xspcomm::XData*>;
   %template(DictStrBool) map<string, bool>;
}

%include "xspcomm/xclock.h"
%include "xspcomm/xdata.h"
%include "xspcomm/xport.h"
%include "xspcomm/xutil.h"
%include "xspcomm/xsignal_cfg.h"
%include "xspcomm/xcomuse.h"

%constant void (*_TEST_DPI_LR)(xspcomm::xsvLogic *v) = xspcomm::TEST_DPI_LR;
%constant void (*_TEST_DPI_LW)(xspcomm::xsvLogic v) = xspcomm::TEST_DPI_LW;
%constant void (*_TEST_DPI_VR)(xspcomm::xsvLogicVecVal *v) = xspcomm::TEST_DPI_VR;
%constant void (*_TEST_DPI_VW)(xspcomm::xsvLogicVecVal *v) = xspcomm::TEST_DPI_VW;

%ignore xspcomm::XData::value;

// callbacks constraints:
// (1) use d_callback define cb before %include "xspcomm/xcallback.h"
// (2) use x_callback define cb after %include "xspcomm/xcallback.h"

%define %d_callback(Name, Ret, T...)
%feature("director") xfunction<Ret,T>;
%ignore xfunction<Ret,T>::operator bool;
%ignore xfunction<Ret,T>::operator();
%enddef

%define %x_callback(Name, Ret, T...)
%template(Name) xfunction<Ret,T>;
%enddef

// d_callback defines
// XData
%d_callback(cb_void_bool_XDatap_u43_voidp, void, bool, xspcomm::XData*, u_int32_t, void *);
%d_callback(cb_void_xsvLogicp, void, xspcomm::xsvLogic *);
%d_callback(cb_void_xsvLogic, void, xspcomm::xsvLogic);
%d_callback(cb_void_xsvlogicVecValp, void, xspcomm::xsvLogicVecVal *);

// XClock
%d_callback(cb_int_bool, int, bool);
%d_callback(cb_void_u64_voidp, void, u_int64_t, void *); // StepRis, StepFal

// ComUseCondCheck
%d_callback(cb_bool_XData_XData, bool, xspcomm::XData*, xspcomm::XData*);
%d_callback(cb_bool_uint64_uint64, bool, uint64_t, uint64_t);

// Note: need include callback after director
%include "xspcomm/xcallback.h"

// x_callback defines
// XData
%x_callback(cb_void_bool_XDatap_u43_voidp, void, bool, xspcomm::XData*, u_int32_t, void *);
%x_callback(cb_void_xsvLogicp, void, xspcomm::xsvLogic *);
%x_callback(cb_void_xsvLogic, void, xspcomm::xsvLogic);
%x_callback(cb_void_xsvlogicVecValp, void, xspcomm::xsvLogicVecVal *);

// XClock
%x_callback(cb_int_bool, int, bool);
%x_callback(cb_void_u64_voidp, void, u_int64_t, void *); // StepRis, StepFal

// ComUseCondCheck
%x_callback(cb_bool_XData_XData, bool, xspcomm::XData*, xspcomm::XData*);
%x_callback(cb_bool_uint64_uint64, bool, uint64_t, uint64_t);
