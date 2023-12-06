#ifndef __xspcomm_no_export__
#define __xspcomm_no_export__

#include <functional>

template <typename R, typename... Args>
class _xfunction_ptr {
    public:
    std::function<R(Args...)> func = nullptr;
};

#endif
