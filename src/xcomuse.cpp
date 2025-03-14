
#include "xspcomm/xcomuse.h"


namespace xspcomm {

    uint64_t _comuse_echo_cfg[3] = {0, 0, 0};
    static void _comuse_echo(uint64_t c, void *p){
        xspcomm::XData** pins = (xspcomm::XData **)_comuse_echo_cfg;
        if ((*pins[0]) != 0){
            fprintf(_comuse_echo_cfg[3] != 0 ? stderr: stdout,"%c", (char)(*pins[1]));
        }
    }

    void   ComUseSetEchoCfg(u_int64_t valid, u_int64_t data, bool stderr_echo){
        _comuse_echo_cfg[0] = valid;
        _comuse_echo_cfg[1] = data;
        _comuse_echo_cfg[2] = stderr_echo;
    }

    u_int64_t ComUseGetEchoFunc(){
        if(!_comuse_echo_cfg[0]){
            fprintf(stderr, "please call comuse_set_echo_cfg first!\n");
            return 0;
        }
        return (u_int64_t)_comuse_echo;
    }

} // namespace xspcomm