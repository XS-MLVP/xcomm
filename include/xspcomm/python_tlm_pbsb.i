%module(directors="1") tlm_pbsb

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

%typemap(in) (int argc, char **argv) {
    if (!PyTuple_Check($input)) {
        PyErr_SetString(PyExc_TypeError, "Expected a tuple");
        return NULL;
    }
    $1 = PyTuple_Size($input);
    $2 = (char **) malloc(($1+1)*sizeof(char *));
    for (int i = 0; i < $1; i++) {
        PyObject *o = PyTuple_GetItem($input, i);
        if (!PyUnicode_Check(o)) {
            PyErr_SetString(PyExc_TypeError, "Expected a tuple of strings");
            return NULL;
        }
        Py_ssize_t len = PyUnicode_GetLength(o);
        char * buff = (char *) malloc((1+len)*sizeof(char));
        strcpy(buff, PyUnicode_AsUTF8(o));
        $2[i] = buff;
    }
    $2[$1] = 0;
}

%typemap(freearg) (int argc, char **argv) {
    free($2);
}

%{
    #include "xspcomm/tlm_msg.h"
    #include "xspcomm/tlm_pbsb.h"
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

%include "xspcomm/tlm_msg.h"
%include "xspcomm/tlm_pbsb.h"

%define %d_callback(Name, Ret, T...)
%feature("director") xfunction<Ret,T>;
%ignore xfunction<Ret,T>::operator bool;
%ignore xfunction<Ret,T>::operator();
%enddef

%define %x_callback(Name, Ret, T...)
%template(Name) xfunction<Ret,T>;
%enddef

%d_callback(cb_void_tlm_msg, void, const xspcomm::tlm_msg &);
%include "xspcomm/xcallback.h"
%x_callback(cb_void_tlm_msg, void, const xspcomm::tlm_msg &);

%pythoncode%{
# -------------------------------------------------------------
# refine callback

def _tlm_sub_handler(fc):
    class Func(cb_void_tlm_msg):
        def __init__(self, func):
            cb_void_tlm_msg.__init__(self)
            self.func = func
            self.set_force_callable()
        def call(self, msg):
            return self.func(msg)
    return Func(fc).__disown__()

TLMSub_old_init__  = TLMSub.__init__
TLMSUB_old_SetHandler = TLMSub.SetHandler

def TLMSub_SetHandler(self, cb):
    return TLMSUB_old_SetHandler(self, _tlm_sub_handler(cb))

def TLMSub__init__(self: TLMSub, ch: str, cb=None):
    ob = TLMSub_old_init__(self, ch)
    if callable(cb):
       TLMSUB_old_SetHandler(self, _tlm_sub_handler(cb))
    return ob

TLMSub.__init__ = TLMSub__init__
TLMSub.SetHandler = TLMSub_SetHandler

old_tlm_vcs_init = tlm_vcs_init
def new_tlm_vcs_init(*args):
    return old_tlm_vcs_init(args)
tlm_vcs_init = new_tlm_vcs_init


%}
