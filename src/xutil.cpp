#include "xspcomm/xutil.h"

namespace xspcomm {
LogLevel log_level = LogLevel::debug;
LogLevel get_log_level()
{
    return log_level;
}
void set_log_level(LogLevel val)
{
    log_level = val;
}
std::string version(){
    return XSPCOMM_VERSION;
}
} // namespace xspcomm
