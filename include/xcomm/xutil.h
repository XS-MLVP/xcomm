#ifndef __xcomm_xutil_h__
#define __xcomm_xutil_h__

#include <stdio.h>
#include <string>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <execinfo.h>
#include <cxxabi.h>
#include "sys/time.h"
#include <ctime>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <climits>

#define xcomm_version "0.0.1"
#define XCOMM_VERSION xcomm_version

namespace xcomm {
void inline Traceback(FILE *out = stderr, unsigned int max_frames = 63)
{
    fprintf(out, "stack trace:\n");
    void *addrlist[max_frames + 1];
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void *));
    if (addrlen == 0) {
        fprintf(out, "  <empty, possibly corrupt>\n");
        return;
    }
    char **symbollist   = backtrace_symbols(addrlist, addrlen);
    size_t funcnamesize = 256;
    char *funcname      = (char *)malloc(funcnamesize);
    for (int i = 1; i < addrlen; i++) {
        char *begin_name = 0, *begin_offset = 0, *end_offset = 0;
        for (char *p = symbollist[i]; *p; ++p) {
            if (*p == '(')
                begin_name = p;
            else if (*p == '+')
                begin_offset = p;
            else if (*p == ')' && begin_offset) {
                end_offset = p;
                break;
            }
        }

        if (begin_name && begin_offset && end_offset
            && begin_name < begin_offset) {
            *begin_name++   = '\0';
            *begin_offset++ = '\0';
            *end_offset     = '\0';
            int status;
            char *ret = abi::__cxa_demangle(begin_name, funcname, &funcnamesize,
                                            &status);
            if (status == 0) {
                funcname = ret; // use possibly realloc()-ed string
                fprintf(out, "  %s : %s+%s\n", symbollist[i], funcname,
                        begin_offset);
            } else {
                fprintf(out, "  %s : %s()+%s\n", symbollist[i], begin_name,
                        begin_offset);
            }
        } else {
            fprintf(out, "  %s\n", symbollist[i]);
        }
    }
    free(funcname);
    free(symbollist);
}

inline long uTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

inline std::string fmtTime(long utime, std::string fmt = "%Y-%m-%d %H:%M:%S")
{
    time_t t = utime / 1000000;
    std::stringstream buffer;
    buffer << std::put_time(localtime(&t), fmt.c_str());
    return buffer.str();
}

inline std::string fmtNow(std::string fmt = "%Y-%m-%d %H:%M:%S")
{
    std::time_t t = std::time(nullptr);
    return fmtTime(t, fmt);
}

enum class LogLevel {
    debug = 1,
    info,
    warning,
    error,
    fatal,
};

LogLevel get_log_level();
void set_log_level(LogLevel val);
std::string version();

#define output(o, level, prefix, fmt, ...)                                     \
    {                                                                          \
        if (level >= xcomm::get_log_level()) {                                 \
            fprintf(o, "[%s:%d] %s ", __FILE__, __LINE__, prefix);             \
            fprintf(o, fmt, ##__VA_ARGS__);                                    \
            fprintf(o, "%s\n", "");                                            \
        }                                                                      \
    }
#define Info(fmt, ...)                                                         \
    {                                                                          \
        output(stdout, xcomm::LogLevel::info, "[ info]", fmt, ##__VA_ARGS__);  \
    }
#define Warn(fmt, ...)                                                         \
    {                                                                          \
        output(stdout, xcomm::LogLevel::warning, "[ warn]", fmt,               \
               ##__VA_ARGS__);                                                 \
    }
#define Error(fmt, ...)                                                        \
    {                                                                          \
        output(stderr, xcomm::LogLevel::error, "[error]", fmt, ##__VA_ARGS__); \
    }
#define Debug(fmt, ...)                                                        \
    {                                                                          \
        output(stdout, xcomm::LogLevel::debug, "[debug]", fmt, ##__VA_ARGS__); \
    }
#define DebugC(c, fmt, ...)                                                    \
    {                                                                          \
        if (c) {                                                               \
            output(stdout, xcomm::LogLevel::debug, "[debug]", fmt,             \
                   ##__VA_ARGS__);                                             \
        }                                                                      \
    }
#define Fatal(fmt, ...)                                                        \
    {                                                                          \
        output(stderr, xcomm::LogLevel::fatal, "[fatal]", fmt, ##__VA_ARGS__); \
        exit(-1);                                                              \
    }

#define Assert(c, fmt, ...)                                                    \
    {                                                                          \
        if (!(c)) {                                                            \
            output(stderr, xcomm::LogLevel::fatal, "[assert fail]", fmt,       \
                   ##__VA_ARGS__);                                             \
            xcomm::Traceback();                                                \
            exit(-1);                                                          \
        }                                                                      \
    }

template <typename... Args>
std::string sFmt(const std::string &format, Args... args)
{
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...)
                 + 1; // Extra space for '\0'
    if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(),
                       buf.get() + size - 1); // We don't want the '\0' inside
}

inline std::string sArrayHex(unsigned char *buff, int size)
{
    std::string ret = "0x";
    for (int i = 0; i < size; i++) { ret += sFmt("%02x ", buff[i]); }
    return ret;
}

inline bool sWith(const std::string &str, const std::string &prefix)
{
    return str.size() >= prefix.size()
           && str.compare(0, prefix.size(), prefix) == 0;
}

template <typename T>
inline bool contians(std::vector<T> v, T a)
{
    for (T x : v) {
        if (x == a) return true;
    }
    return false;
}

inline std::string sLower(std::string input)
{
    auto str = input;
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

inline std::string FmtSize(u_int64_t s)
{
    auto K = 1 << 10;
    auto M = 1 << 20;
    auto G = 1 << 30;
    if (s < K) { return sFmt("%d B", s); }
    if (s < M) { return sFmt("%.2f KB", double(s) / K); }
    if (s < G) { return sFmt("%.2f MB", double(s) / M); }
    return sFmt("%.2f GB", double(s) / G);
}

#define likely(cond) __builtin_expect(cond, 1)
#define unlikely(cond) __builtin_expect(cond, 0)

inline u_int64_t xRandom(u_int64_t a, u_int64_t b)
{
    auto size = b - a;
    Assert(b > a, "Need b > a");
    u_int64_t v = rand();
    if (b < UINT32_MAX) {
        v = v % size;
    } else {
        v = (v << 32 + rand()) % size;
    }
    return a + v;
}

inline void XSeed(unsigned int seed)
{
    srand(seed);
}

inline bool checkVersion()
{
    if (version() == XCOMM_VERSION) { return true; }
    Warn("so version(%s) conflict with headers (%s)", version().c_str(),
         XCOMM_VERSION) return false;
}

#define FOR_COUNT(n)                                                           \
    auto __n = n;                                                              \
    for (int __i = 0; __i < __n; __i++)

} // namespace xcomm

#endif
