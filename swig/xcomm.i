
%{
#include "xspcomm/xclock.h"
#include "xspcomm/xdata.h"  
#include "xspcomm/xarray.h"
#include "xspcomm/xview.h"
#include "xspcomm/xvpi_view.h"
#include "xspcomm/xport.h"
#include "xspcomm/xcallback.h"
#include "xspcomm/xsignal_cfg.h"
#include "xspcomm/xmonitor.h"
#include "xspcomm/xtrigger.h"
#include <stdexcept>
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
#if !defined(SWIGGO) && !defined(SWIGLUA) && !defined(SWIGJAVASCRIPT)
%include std_shared_ptr.i
%shared_ptr(xspcomm::XData)
%shared_ptr(xspcomm::XDataArray)
%shared_ptr(xspcomm::XStruct)
%shared_ptr(xspcomm::XArray)
%shared_ptr(xspcomm::XVPI)
#endif

%ignore xspcomm::XPort::SelectPins(std::initializer_list<std::string>);

namespace std {
   %template(XIntVector) vector<int>;
   %template(XUInt64Vector) vector<unsigned long long>;
   %template(StringVector) vector<string>;
   %template(XDataVector) vector<std::shared_ptr<xspcomm::XData>>;
   %template(DictStrBool) map<string, bool>;
}

%include exception.i
%exception {
    try { $action }
    catch (const std::invalid_argument &error) {
        SWIG_exception(SWIG_ValueError, error.what());
    }
    catch (const std::out_of_range &error) {
        SWIG_exception(SWIG_IndexError, error.what());
    }
    catch (const std::exception &error) {
        SWIG_exception(SWIG_RuntimeError, error.what());
    }
}
%include "xspcomm/runtime/types.h"
%include "xspcomm/xclock.h"
#if defined(SWIGGO) || defined(SWIGLUA) || defined(SWIGJAVASCRIPT)
// These SWIG targets do not provide std_shared_ptr.i. Keep shared node/storage
// ownership in C++; expose owned container handles and borrowed cached XData.
%ignore xspcomm::XData::StructView;
%ignore xspcomm::XData::ArrayView;
%ignore xspcomm::XView::FromData;
%ignore xspcomm::XView::FromPort;
%ignore xspcomm::XView::Data;
%ignore xspcomm::XView::Struct;
%ignore xspcomm::XView::Array;
%ignore xspcomm::XView::PackedData;
%ignore xspcomm::XStruct::Field;
%ignore xspcomm::XStruct::Data;
%ignore xspcomm::XStruct::Struct;
%ignore xspcomm::XStruct::Array;
%ignore xspcomm::XStruct::PackedData;
%ignore xspcomm::XArray::At;
%ignore xspcomm::XArray::Data;
%ignore xspcomm::XArray::Struct;
%ignore xspcomm::XArray::Array;
%ignore xspcomm::XArray::PackedData;
%ignore xspcomm::XVPI::Get;
#endif
%include "xspcomm/xdata.h"
%include "xspcomm/xarray.h"
%include "xspcomm/xview.h"
%include "xspcomm/xvpi_view.h"
#if defined(SWIGGO) || defined(SWIGLUA) || defined(SWIGJAVASCRIPT)
%include "view_raw.i"
#endif
%include "xspcomm/xport.h"
%include "xspcomm/xutil.h"
%include "xspcomm/xsignal_cfg.h"
%include "xspcomm/monitor/callback.h"
%include "xspcomm/common/compare_op.h"
%include "xspcomm/monitor/condition.h"
%include "xspcomm/monitor/range.h"
%include "xspcomm/common/memory.h"
%include "xspcomm/xexpr.h"
%include "xspcomm/monitor/expr.h"
%include "xspcomm/monitor/fsm.h"
%include "xspcomm/xmonitor.h"
%include "xspcomm/xpattern.h"
%include "xspcomm/xcoverage.h"
%rename(XTriggerEngine) xspcomm::XEngine;
%include "xspcomm/xengine.h"
%include "xspcomm/xtrigger.h"
%exception;

namespace std {
   %template(XBackendHitVector) vector<xspcomm::XBackendHit>;
   %template(XCoveragePointVector) vector<xspcomm::XCoveragePoint>;
   %template(XCoverageBinVector) vector<xspcomm::XCoverageBin>;
   %template(XUInt32Vector) vector<unsigned int>;
   %template(XSequenceStepVector) vector<xspcomm::XSequenceStep>;
   %template(XFsmTransitionVector) vector<xspcomm::XFsmTransition>;
}

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
%inline %{
   xfunction<Ret,T> Name##_from_addr(uint64_t addr) {
       return (Ret(*)(T...))addr;
   }
   xfunction<Ret,T> Name##_from_func(Ret (* func)(T)) {
      return func;
   }
%}
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

// XConditionCheck
%d_callback(cb_bool_XData_XData_uint64, bool, xspcomm::XData*, xspcomm::XData*, uint64_t);
%d_callback(cb_bool_uint64_uint64_uint64, bool, uint64_t, uint64_t, uint64_t);

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

// XConditionCheck
%x_callback(cb_bool_XData_XData_uint64, bool, xspcomm::XData*, xspcomm::XData*, uint64_t);
%x_callback(cb_bool_uint64_uint64_uint64, bool, uint64_t, uint64_t, uint64_t);
