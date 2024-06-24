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

%constant void (*DPI_TEST_LR)(void *v) = xspcomm::TEST_DPI_LR;
%constant void (*DPI_TEST_LW)(const unsigned char v) = xspcomm::TEST_DPI_LW;
%constant void (*DPI_TEST_VR)(void *v) = xspcomm::TEST_DPI_VR;
%constant void (*DPI_TEST_VW)(const void *v) = xspcomm::TEST_DPI_VW;

%pythoncode%{
# PYTHON code
%}
