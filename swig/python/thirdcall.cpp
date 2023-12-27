#include "xspcomm/thirdcall.h"
#include "stdlib.h"
#include "stdio.h"
#include <map>
#include <Python.h>
#include "xspcomm/xutil.h"
#include <unistd.h>
#include <string>

std::map<std::string, int> __thirdcall__function_ids__;
std::vector<PyObject *> __thirdcall__function_objs__;

PyObject *__thirdcall__module__ = nullptr;
PyObject *__thirdcall__mname__  = nullptr;

bool is_in_init = false;

extern "C" {
    bool free_third_call(){
        if(__thirdcall__module__ != nullptr){
            Py_DECREF(__thirdcall__module__);
            __thirdcall__module__ = nullptr;
        }
        if(__thirdcall__mname__ != nullptr){
            Py_DECREF(__thirdcall__mname__);
            __thirdcall__mname__ = nullptr;
        }
        __thirdcall__function_objs__.clear();
        __thirdcall__function_ids__.clear();
        return true;
    }
    bool init_third_call(){
        if(is_in_init){
            return false;
        }
        if(__thirdcall__module__ != nullptr){
            return true;
        }
        is_in_init = true;
        const char *mname = getenv("PYTHON_THIRD_MODUEL");
        if(mname == nullptr){
            Debug("PYTHON_THIRD_MODUEL not set, using default name: thirdcall");
            mname = "thirdcall";
        }
        __thirdcall__mname__ = PyUnicode_FromString(mname);
        __thirdcall__module__ = PyImport_Import(__thirdcall__mname__);
        if(__thirdcall__module__ == nullptr){
            PyErr_Print();
            is_in_init = false;
            return false;
        }
        PyObject *dict = PyModule_GetDict(__thirdcall__module__);
        PyObject *keys = PyDict_Keys(dict);
        int size = PyList_Size(keys);
        for(int i = 0; i < size; i++){
            PyObject *key = PyList_GetItem(keys, i);
            PyObject *value = PyDict_GetItem(dict, key);
            if(!PyCallable_Check(value)){
                continue;
            }
            auto name = PyUnicode_AsUTF8(key);
            Debug("Found function: %s", name);
            __thirdcall__function_objs__.push_back(value);
            __thirdcall__function_ids__[name] = __thirdcall__function_objs__.size() - 1;
        }
        is_in_init = false;
        return true;
    }
    int get_function_id(const char *function_name){
        if(__thirdcall__function_ids__.count(function_name)){
            return __thirdcall__function_ids__[function_name];
        }
        return -1;
    }
    bool call_third_function(int id, unsigned int *args, int argc, unsigned int *ret, int *retc){
        if(id < 0 || id >= __thirdcall__function_objs__.size()){
            return false;
        }
        auto func = __thirdcall__function_objs__[id];
        PyObject *arglist = PyList_New(argc);
        for(int i = 0; i < argc; i++){
            PyList_SetItem(arglist, i, PyLong_FromLong(args[i]));
        }
        PyObject *argtupe = PyTuple_New(1);
        PyTuple_SetItem(argtupe, 0, arglist);
        PyObject *result = PyObject_CallObject(func, argtupe);
        Py_XDECREF(arglist);
        Py_XDECREF(argtupe);
        if(result == nullptr){
            PyErr_Print();
            return false;
        }
        if(PyList_Check(result)){
            int size = PyList_Size(result);
            for(int i = 0; i < size; i++){
                PyObject *item = PyList_GetItem(result, i);
                if(!PyLong_Check(item)){
                    Error("Return type not match, expect int, got %s", Py_TYPE(item)->tp_name);
                    return false;
                }
                ret[i] = PyLong_AsLong(item);
            }
            *retc = size;
        }
        Py_XDECREF(result);
        return true;
    }
    bool test_third_call(){
        if(!__thirdcall__module__){
            Error("Third call not initialized");
            return false;
        }
        auto fc_id =  get_function_id("test_third_call");
        if(fc_id < 0){
            Error("Function test_third_call not found");
            return false;
        }
        unsigned int args[] = {1, 2, 3};
        unsigned int ret[128];
        int retc = 0;
        if(!call_third_function(fc_id, args, 3, ret, &retc)){
            Error("Call test_third_call failed");
            return false;
        }
        if(retc > 128){
            Error("Return value count not match, expect <= 128, got %d", retc);
            return false;
        }
        Debug("Test success, return value count: %d", retc);
        return true;
    }
}
