#ifndef SYS_ASSERT_H
#define SYS_ASSERT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ASSERT_ENABLED
#define ASSERT_ENABLED  1
#endif

#ifndef ASSERT_BKPT_ENABLED
#define ASSERT_BKPT_ENABLED  1
#endif

#ifndef ASSERTVerbose_ENABLED
#define ASSERTVerbose_ENABLED  1
#endif

#ifndef ASSERT_LOCK_INTERRUPT
#define ASSERT_LOCK_INTERRUPT  1
#endif

#ifndef ASSERT_DIRECT
#define ASSERT_DIRECT  0
#endif

typedef enum {
    ASSERT_LVL_DEBUG = 0,
    ASSERT_LVL_INFO  = 1,
    ASSERT_LVL_WARN  = 2,
    ASSERT_LVL_ERROR = 3,
    ASSERT_LVL_FATAL = 4,
} assert_level_t;

typedef struct {
    const char     *file;
    uint32_t        line;
    const char     *func;
    const char     *expr;
    assert_level_t  level;
} assert_info_t;

#ifndef ASSERT_PRINTF
#define ASSERT_PRINTF(fmt, ...)  printf((fmt), ##__VA_ARGS__)
#endif

#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) \
    || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6T2__) || defined(__ARM_ARCH_6M__)
  #define ASSERT_ARCH_CM0    1
  #define ASSERT_ARCH_MASK   0x00000001U
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
  #if defined(__ARM_FEATURE_DSP) || defined(__ARM_PROFILE_M__)
    #define ASSERT_ARCH_CM4   1
    #define ASSERT_ARCH_MASK 0x00000004U
  #else
    #define ASSERT_ARCH_CM3   1
    #define ASSERT_ARCH_MASK 0x00000003U
  #endif
#endif

#if ASSERT_LOCK_INTERRUPT && !ASSERT_DIRECT

#if defined(ASSERT_ARCH_CM0)
  #define ASSERT_ENTER_CRITICAL()  __asm volatile ("cpsid i" : : : "memory")
  #define ASSERT_EXIT_CRITICAL()   __asm volatile ("cpsie i" : : : "memory")
#elif defined(ASSERT_ARCH_CM3) || defined(ASSERT_ARCH_CM4)
  #define ASSERT_ENTER_CRITICAL()  do { \
        uint32_t __primask__; \
        __asm volatile ("mrs %0, primask" : "=r"(__primask__)); \
        __asm volatile ("cpsid i" : : : "memory"); \
        (void)__primask__; \
    } while (0)
  #define ASSERT_EXIT_CRITICAL()   __asm volatile ("cpsie i" : : : "memory")
#else
  #define ASSERT_ENTER_CRITICAL()  ((void)0)
  #define ASSERT_EXIT_CRITICAL()   ((void)0)
#endif

#else
  #define ASSERT_ENTER_CRITICAL()  ((void)0)
  #define ASSERT_EXIT_CRITICAL()   ((void)0)
#endif

#if ASSERT_DIRECT

#define _ASSERT_DIRECT_PRINT(file, line, func, expr) \
    do { \
        ASSERT_PRINTF("[ASSERT FAILED] %s:%u %s()\r\n", file, (unsigned)line, func); \
        ASSERT_PRINTF("  expr: %s\r\n", expr); \
    } while (0)

#if ASSERTVerbose_ENABLED
#define _ASSERT_DIRECT_PRINT_V(file, line, func, expr) \
    do { \
        ASSERT_PRINTF("\r\n========================================\r\n"); \
        ASSERT_PRINTF("       ASSERTION FAILURE DETECTED       \r\n"); \
        ASSERT_PRINTF("========================================\r\n"); \
        ASSERT_PRINTF("  File     : %s\r\n", file); \
        ASSERT_PRINTF("  Line     : %u\r\n", (unsigned)line); \
        ASSERT_PRINTF("  Function : %s\r\n", func); \
        ASSERT_PRINTF("  Expr     : %s\r\n", expr); \
        ASSERT_PRINTF("----------------------------------------\r\n"); \
    } while (0)
#endif

#if ASSERT_BKPT_ENABLED
  #if defined(ASSERT_ARCH_CM0) || defined(ASSERT_ARCH_CM3) || defined(ASSERT_ARCH_CM4)
    #define _ASSERT_DIRECT_HALT()  __asm volatile ("BKPT #0")
  #else
    #define _ASSERT_DIRECT_HALT()  ((void)0)
  #endif
#else
  #define _ASSERT_DIRECT_HALT()  ((void)0)
#endif

#define _ASSERT_DIRECT_LOOP()  do { while (1) { __asm volatile ("NOP"); } } while (0)

#endif

void sys_assert_failed(const char *file, uint32_t line,
                       const char *func, const char *expr);

#if ASSERTVerbose_ENABLED
void sys_assert_failed_verbose(const char *file, uint32_t line,
                                const char *func, const char *expr);
#endif

void sys_assert_init(void);

#if ASSERT_ENABLED

#if ASSERT_DIRECT

#define ASSERT(expr) \
    do { \
        if (__builtin_expect(!(expr), 0)) { \
            _ASSERT_DIRECT_PRINT(__FILE__, __LINE__, __func__, #expr); \
            _ASSERT_DIRECT_HALT(); \
            _ASSERT_DIRECT_LOOP(); \
        } \
    } while (0)

#define ASSERT_NOT_REACHED() \
    do { \
        _ASSERT_DIRECT_PRINT(__FILE__, __LINE__, __func__, "NOT_REACHED"); \
        _ASSERT_DIRECT_HALT(); \
        _ASSERT_DIRECT_LOOP(); \
    } while (0)

#if ASSERTVerbose_ENABLED
#define ASSERT_V(expr) \
    do { \
        if (__builtin_expect(!(expr), 0)) { \
            _ASSERT_DIRECT_PRINT_V(__FILE__, __LINE__, __func__, #expr); \
            _ASSERT_DIRECT_HALT(); \
            _ASSERT_DIRECT_LOOP(); \
        } \
    } while (0)
#else
#define ASSERT_V(expr)  ASSERT(expr)
#endif

#else

#define ASSERT(expr) \
    do { \
        if (__builtin_expect(!(expr), 0)) { \
            sys_assert_failed(__FILE__, __LINE__, __func__, #expr); \
        } \
    } while (0)

#define ASSERT_NOT_REACHED() \
    do { \
        sys_assert_failed(__FILE__, __LINE__, __func__, "NOT_REACHED"); \
    } while (0)

#if ASSERTVerbose_ENABLED
#define ASSERT_V(expr) \
    do { \
        if (__builtin_expect(!(expr), 0)) { \
            sys_assert_failed_verbose(__FILE__, __LINE__, __func__, #expr); \
        } \
    } while (0)
#else
#define ASSERT_V(expr)  ASSERT(expr)
#endif

#endif

#else

#define ASSERT(expr)              ((void)0)
#define ASSERT_NOT_REACHED()      ((void)0)
#define ASSERT_V(expr)            ((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif
