#include "xcomm/xutil.h"

namespace xcomm {
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
    return XCOMM_VERSION;
}
} // namespace xcomm
