#ifndef __xspcomm_third_call__
#define __xspcomm_third_call__
#pragma once

#include "stdio.h"

extern "C" {
    bool free_third_call();
    bool init_third_call();
    int  get_function_id(const char *function_name);
    bool call_third_function(int id, unsigned int *args, int argc, unsigned int *ret, int *retc);
    bool test_third_call();
}

#endif
