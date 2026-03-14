#ifndef VVAR_TEST_STUBS_H
#define VVAR_TEST_STUBS_H

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cassert>
#include <string>

inline std::string& vvarTestLogBuffer() {
    static std::string buffer;
    return buffer;
}

inline void vvarTestClearLog() {
    vvarTestLogBuffer().clear();
}

inline const std::string& vvarTestGetLog() {
    return vvarTestLogBuffer();
}

inline void vvarTestAppendLog(const char* format, va_list args) {
    char buffer[8192];
    vsnprintf(buffer, sizeof(buffer), format, args);
    vvarTestLogBuffer() += buffer;
}

// Stub logging functions for tests
inline void vvar_test_dinfo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vvarTestAppendLog(format, args);
    va_end(args);
}

inline void vvar_test_derr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vvarTestAppendLog(format, args);
    va_end(args);
}

inline void vvar_test_derr_fatal(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vvarTestAppendLog(format, args);
    va_end(args);
    exit(1);
}

#define dinfo(...) vvar_test_dinfo(__VA_ARGS__)
#define derr(...) vvar_test_derr(__VA_ARGS__)
#define derr_fatal(...) vvar_test_derr_fatal(__VA_ARGS__)

// SUCCEED macro for utest.h compatibility
#define SUCCEED() do { *utest_result = UTEST_TEST_PASSED; } while(0)

#endif // VVAR_TEST_STUBS_H
