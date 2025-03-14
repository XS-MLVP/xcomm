#ifndef __xspcomm_xcomuse_h__
#define __xspcomm_xcomuse_h__

#include "xspcomm/xcomm.h"

namespace xspcomm {

    // echo from XData pins
    void   ComUseSetEchoCfg(u_int64_t valid, u_int64_t data, bool stderr_echo = true);

    u_int64_t ComUseGetEchoFunc();

    // TBD

}

#endif