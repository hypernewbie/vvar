#ifndef VVAR_TEST_STUBS_H
#define VVAR_TEST_STUBS_H

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cassert>

// Stub logging functions for tests
inline void dinfo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

inline void derr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

inline void derr_fatal(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}

// SUCCEED macro for utest.h compatibility
#define SUCCEED() do { *utest_result = UTEST_TEST_PASSED; } while(0)

#endif // VVAR_TEST_STUBS_H
