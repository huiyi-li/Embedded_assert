#include "sys_assert.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int g_custom_output_called = 0;

void sys_assert_output(const uint8_t *data, uint32_t len)
{
    g_custom_output_called++;
    printf("[CUSTOM OUTPUT #%d] ", g_custom_output_called);
    for (uint32_t i = 0; i < len && i < 128; i++) {
        putchar(data[i]);
    }
    printf("\n");
}

void sys_assert_halt(void)
{
    printf("[HALT OVERRIDE] Assert halted gracefully\n");
    while (1) { }
}

int main(void)
{
    printf("=== Test 1: Normal assert that passes ===\n");
    int x = 5;
    ASSERT(x > 0);
    printf("PASS: x > 0 evaluated true, ASSERT did not trigger\n");

    printf("\n=== Test 2: Expect assert to trigger ===\n");
    int y = 0;
    ASSERT(y > 0);

    printf("This line should NEVER execute after assert triggers\n");
    return 0;
}
