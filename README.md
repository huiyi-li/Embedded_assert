# 断言系统（sys_assert）设计文档

## 1. 组件功能概述

`sys_assert` 是一套轻量级断言系统，严格依赖 `sys_assert.h` 和 `sys_assert.c` 两个文件实现，专为嵌入式 ARM Cortex-M 系列芯片设计。系统融合了 Nordic nRF52840 SDK 的弱函数覆盖机制与 STM32 HAL 的 verbose 诊断格式，具备以下核心能力：

- **弱函数覆盖机制**：所有钩子函数均为 `__attribute__((weak))` 实现，客户可直接重写，无需任何注册调用
- **BKPT 断点调试**：Cortex-M 架构下触发 `BKPT #0` 指令，连接调试器时自动暂停至断言位置
- **临界区保护**：断言处理全流程包裹在临界区中，防止中断嵌套导致诊断信息损坏
- **编译期裁剪**：`ASSERT_ENABLED=0` 时所有宏展开为 `((void)0)`，零运行时开销
- **数据指针+长度接口**：`sys_assert_output(data, len)` 底层接口直接传递字节流，客户可无缝转发至 UART/DMA/RTT 等任意介质

---

## 2. 设计决策说明

### 2.1 为何使用弱函数而非函数注册机制

**注册方式的问题**：

- 需要成对传递 `lock_fn` 和 `unlock_fn`，用户接口臃肿
- `lock`/`unlock` 容易出现不匹配（漏掉某个出口路径）
- 无法提供默认实现（裸机直接卡死，客户无法自定义行为）

**弱函数的优势**：

- 默认实现即插即用，无需任何初始化调用
- 客户只需重写自己需要的函数，其他保持默认
- 编译链接阶段决议，无运行时查找开销
- Nordic nRF52840 SDK 和 STM32 HAL 均采用此机制

### 2.2 为何采用 `data + len` 而非 `va_list` 接口

`void sys_assert_output(const uint8_t *data, uint32_t len)` 比 `void (*fn)(const char *fmt, ...)` 更底层、更灵活：

- 客户可自行解析原始字节流，转发至任意介质（UART 环形缓冲、DMA 描述符、RTT Viewer）
- 支持二进制数据（如寄存器转储）而非仅限于格式化文本
- 避免了 `va_list` 在不同编译器间的不一致性问题

---

## 3. 接口使用说明

### 3.1 核心弱函数（客户可覆盖）

```c
// 输出断言信息，data 为字节数据指针，len 为字节长度
// 默认实现：复制到本地 buf 并通过 ASSERT_PRINTF 输出
void sys_assert_output(const uint8_t *data, uint32_t len);

// 进入临界区，关闭全局中断
// 默认实现：ASSERT_ENTER_CRITICAL() 宏（见 3.4）
void sys_assert_lock(void);

// 退出临界区，恢复全局中断
// 默认实现：ASSERT_EXIT_CRITICAL() 宏（见 3.4）
void sys_assert_unlock(void);

// 断言失败后的停机动作
// 默认实现：BKPT 断点 + while(1) 死循环
void sys_assert_halt(void);

// 以结构化形式输出断言信息
// 默认实现：输出 "[ASSERT] file:line func() expr: ..."
void sys_assert_info(const assert_info_t *info);
```

### 3.2 断言宏

```c
// 断言表达式，失败时触发 sys_assert_failed
ASSERT(expr)

// 断言不应到达的代码位置，失败时触发 sys_assert_failed("NOT_REACHED")
ASSERT_NOT_REACHED()

// Verbose 模式断言，失败时触发 sys_assert_failed_verbose
// 仅在 ASSERTVerbose_ENABLED=1 时有效
ASSERT_V(expr)
```

### 3.3 编译配置宏

| 宏名 | 默认值 | 说明 |
|------|--------|------|
| `ASSERT_ENABLED` | 1 | 主开关，0 = 所有断言宏展开为 `((void)0)` |
| `ASSERT_BKPT_ENABLED` | 1 | 0 = 禁用 BKPT 断点指令 |
| `ASSERTVerbose_ENABLED` | 1 | 0 = `ASSERT_V` 等同于 `ASSERT` |
| `ASSERT_LOCK_INTERRUPT` | 1 | 0 = 禁用临界区保护 |
| `ASSERT_PRINTF` | `printf(...)` | 自定义输出底层函数 |
| `ASSERT_OUTPUT_BUF_SIZE` | 256 | 断言输出缓冲区大小 |

### 3.4 内核兼容性实现

系统通过预检测 `__ARM_ARCH_*` 宏自动识别目标内核，无需客户配置：

| 内核 | 宏定义 | 临界区实现 |
|------|--------|-----------|
| **Cortex-M0/M0+** | `ASSERT_ARCH_CM0=1` | `cpsid i` / `cpsie i`（直接开关 IRQ） |
| **Cortex-M3** | `ASSERT_ARCH_CM3=1` | `mrs primask` + `cpsid i` / `cpsie i`（保存中断状态） |
| **Cortex-M4** | `ASSERT_ARCH_CM4=1` | 同 CM3，基于 `__ARM_FEATURE_DSP` 或 `__ARM_PROFILE_M__` 区分 |

```c
// CM0：直接开关中断（无嵌套支持）
#define ASSERT_ENTER_CRITICAL()  __asm volatile ("cpsid i" : : : "memory")
#define ASSERT_EXIT_CRITICAL()   __asm volatile ("cpsie i" : : : "memory")

// CM3/CM4：保存 PRIMASK 状态，允许多次嵌套调用
#define ASSERT_ENTER_CRITICAL()  do { \
        uint32_t __primask__; \
        __asm volatile ("mrs %0, primask" : "=r"(__primask__)); \
        __asm volatile ("cpsid i" : : : "memory"); \
        (void)__primask__; \
    } while (0)
#define ASSERT_EXIT_CRITICAL()   __asm volatile ("cpsie i" : : : "memory")
```

**不兼容 ARM A/R 系列**：本系统专为嵌入式实时系统设计，A 系列（如 Cortex-A7/A9）使用不同的电源管理和中断控制器（GIC），不适合嵌入式裸机场景。

---

## 4. 客户覆盖示例

### 4.1 自定义输出函数（转发至 UART）

```c
// 重写弱函数，字节流直接写入 UART 寄存器
void sys_assert_output(const uint8_t *data, uint32_t len)
{
    extern void UART_SendBytes(const uint8_t *data, uint32_t len);
    UART_SendBytes(data, len);
}
```

### 4.2 自定义停机行为（不卡死）

```c
// 重写为复位而非死循环
void sys_assert_halt(void)
{
    NVIC_SystemReset(); // 触发芯片软复位
}
```

### 4.3 转发至 SEGGER RTT

```c
void sys_assert_output(const uint8_t *data, uint32_t len)
{
    SEGGER_RTT_Write(0, data, len);
}
```

---

## 5. 测试用例说明

### 5.1 测试场景：正常断言通过

```c
int x = 5;
ASSERT(x > 0);  // 条件为 true，断言不触发，程序继续执行
```

**预期结果**：程序正常继续，无任何输出。

### 5.2 测试场景：断言失败

```c
int y = 0;
ASSERT(y > 0);  // 条件为 false，触发 sys_assert_failed
```

**预期输出**：
```
[ASSERT FAILED] example.c:42 main()
  expr: y > 0
```
随后 `sys_assert_halt()` 触发 BKPT 断点（调试器连接时暂停）或死循环。

### 5.3 测试场景：Verbose 模式

```c
int z = 0;
ASSERT_V(z > 0);  // 触发 sys_assert_failed_verbose
```

**预期输出**：
```
========================================
       ASSERTION FAILURE DETECTED
========================================
  File     : example.c
  Line     : 42
  Function : main
  Expr     : z > 0
----------------------------------------
  Arch     : Cortex-M3 (0x00000003)
========================================
```

### 5.4 测试场景：自定义覆盖

```c
// test_assert.c 中的覆盖实现
void sys_assert_output(const uint8_t *data, uint32_t len)
{
    g_custom_output_called++;
    printf("[CUSTOM OUTPUT #%d] ", g_custom_output_called);
    for (uint32_t i = 0; i < len && i < 128; i++) putchar(data[i]);
    printf("\n");
}

void sys_assert_halt(void)
{
    printf("[HALT OVERRIDE] Assert halted gracefully\n");
    while (1) { }
}

int main(void)
{
    int y = 0;
    ASSERT(y > 0);  // 触发覆盖版本
    return 0;
}
```

**预期输出**：
```
[CUSTOM OUTPUT #1] [ASSERT FAILED] test_assert.c:33 main()
  expr: y > 0
[HALT OVERRIDE] Assert halted gracefully
```

---

## 6. 使用注意事项

1. **`ASSERT_ENABLED=0` 时所有参数不评估**：即使表达式有副作用也会被完全剔除，不适合需要"总是执行"的检查。

2. **BKPT 断点仅在调试器连接时有效**：无调试器时会直接落入 `while(1)` 死循环，可覆盖 `sys_assert_halt()` 实现看门狗复位。

3. **临界区嵌套安全**：CM3/CM4 下 `ASSERT_EXIT_CRITICAL()` 使用 `cpsie i` 而非 `cpsie if`，确保多次嵌套调用时中断状态正确恢复。

4. **`ASSERT_OUTPUT_BUF_SIZE`**：默认 256 字节，超出时截断。如需输出大型寄存器转储，请增大此值。

5. **多任务环境**：在 RTOS 中使用时，建议覆盖 `sys_assert_lock/unlock` 为信号量或任务锁，保护诊断输出的原子性。

6. **非 ARM 架构**：在 x86/PC 仿真环境中，`ASSERT_ENTER_CRITICAL()` / `ASSERT_EXIT_CRITICAL()` 为空操作，无中断保护。
