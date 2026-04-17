#include "sys_assert.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifndef ASSERT_OUTPUT_BUF_SIZE
#define ASSERT_OUTPUT_BUF_SIZE  256
#endif

__attribute__((weak))
void sys_assert_output(const uint8_t *data, uint32_t len)
{
    if (!data || len == 0) return;

    char buf[ASSERT_OUTPUT_BUF_SIZE];
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    memcpy(buf, data, len);
    buf[len] = '\0';

    ASSERT_PRINTF("%s", buf);
}

__attribute__((weak))
void sys_assert_lock(void)
{
#if ASSERT_LOCK_INTERRUPT
    ASSERT_ENTER_CRITICAL();
#endif
}

__attribute__((weak))
void sys_assert_unlock(void)
{
#if ASSERT_LOCK_INTERRUPT
    ASSERT_EXIT_CRITICAL();
#endif
}

__attribute__((weak))
void sys_assert_halt(void)
{
#if ASSERT_BKPT_ENABLED
    #if defined(__ARM_CORTEX_M)
        __asm volatile ("BKPT #0");
    #elif defined(__thumb__) && defined(__ARM_ARCH_6__)
        __asm volatile ("BKPT #0");
    #endif
#endif

    while (1) {
#if defined(__ARM_CORTEX_M)
        __asm volatile ("NOP");
#endif
    }
}

__attribute__((weak))
void sys_assert_info(const assert_info_t *info)
{
    if (!info) return;

    char buf[ASSERT_OUTPUT_BUF_SIZE];
    int pos = 0;

    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "[ASSERT] %s:%u %s()",
                    info->file ? info->file : "?",
                    (unsigned)info->line,
                    info->func ? info->func : "?");

    if (info->expr) {
        pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                        "  expr: %s", info->expr);
    }

    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos, "\r\n");

    sys_assert_output((const uint8_t *)buf, (uint32_t)pos);
}

void sys_assert_failed(const char *file, uint32_t line,
                       const char *func, const char *expr)
{
    sys_assert_lock();

    char buf[ASSERT_OUTPUT_BUF_SIZE];
    int pos = 0;
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "[ASSERT FAILED] %s:%u %s()\r\n",
                    file ? file : "?",
                    (unsigned)line,
                    func ? func : "?");
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "  expr: %s\r\n", expr ? expr : "?");

    sys_assert_output((const uint8_t *)buf, (uint32_t)pos);

    (void)file;
    (void)line;
    (void)func;
    (void)expr;

    sys_assert_unlock();

    sys_assert_halt();
}

#if ASSERTVerbose_ENABLED
void sys_assert_failed_verbose(const char *file, uint32_t line,
                                const char *func, const char *expr)
{
    sys_assert_lock();

    char buf[ASSERT_OUTPUT_BUF_SIZE];
    int pos = 0;

    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "\r\n========================================\r\n");
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "       ASSERTION FAILURE DETECTED       \r\n");
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "========================================\r\n");
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "  File     : %s\r\n", file ? file : "?");
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "  Line     : %u\r\n", (unsigned)line);
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "  Function : %s\r\n", func ? func : "?");
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "  Expr     : %s\r\n", expr ? expr : "?");
    pos += snprintf(buf + pos, ASSERT_OUTPUT_BUF_SIZE - pos,
                    "----------------------------------------\r\n");

    sys_assert_output((const uint8_t *)buf, (uint32_t)pos);

#if defined(ASSERT_ARCH_CM0) || defined(ASSERT_ARCH_CM3) || defined(ASSERT_ARCH_CM4)
    {
        const char *arch_name =
#if defined(ASSERT_ARCH_CM0)
            "Cortex-M0";
#elif defined(ASSERT_ARCH_CM4)
            "Cortex-M4";
#else
            "Cortex-M3";
#endif
        pos = snprintf(buf, ASSERT_OUTPUT_BUF_SIZE,
                       "  Arch     : %s (0x%08X)\r\n", arch_name, ASSERT_ARCH_MASK);
        sys_assert_output((const uint8_t *)buf, (uint32_t)pos);
    }
#endif

    sys_assert_unlock();

    sys_assert_halt();
}
#endif

void sys_assert_init(void)
{
}
