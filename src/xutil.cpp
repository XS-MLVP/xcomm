#include "xspcomm/xutil.h"
#include <map>

namespace xspcomm {
LogLevel log_level = LogLevel::info;
LogLevel get_log_level()
{
    std::map<std::string, LogLevel> levels;
    levels["debug"] = LogLevel::debug;
    levels["info"] = LogLevel::info;
    levels["error"] = LogLevel::error;
    levels["warn"] = LogLevel::warning;
    auto env = getenv("XSPCOMM_LOG_LEVEL");
    if (env){
        auto lv = sLower(env);
        if (levels.count(lv) > 0)
        {
            return levels[lv];
        }
    }
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
