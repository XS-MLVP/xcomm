#ifndef XSPCOMM_DETAIL_FUNCTION_H
#define XSPCOMM_DETAIL_FUNCTION_H

#include <functional>

template <typename R, typename... Args>
class _xfunction_ptr {
    public:
    std::function<R(Args...)> func = nullptr;
};

#endif
