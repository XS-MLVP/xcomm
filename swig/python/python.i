%module(directors="1") pyxspcomm

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

%{
    #include "xspcomm/thirdcall.h"
%}
%include "xspcomm/thirdcall.h"
%include ../xcomm.i

%pythoncode%{
# PYTHON code
%}
