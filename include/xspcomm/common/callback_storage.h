#ifndef XSPCOMM_COMMON_CALLBACK_STORAGE_H
#define XSPCOMM_COMMON_CALLBACK_STORAGE_H

#include <functional>

template <typename R, typename... Args>
class _xfunction_ptr {
    public:
    std::function<R(Args...)> func = nullptr;
};

#endif
