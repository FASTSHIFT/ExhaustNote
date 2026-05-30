# Post-Mortem: SDIO SD卡初始化死锁

## 日期
2026-05-31

## 症状
MCU 固件在 `f_mount()` → `sd_init()` → `scr_find()` 处死循环。
GDB 显示卡在等待 SDIO 数据接收完成标志（`SDIO_DORX_FLAG`），SD 卡不返回数据。

## 根本原因
**链接器选择了错误的 `sd_init()` 实现。**

项目中存在两份完整的 SD 卡驱动代码：
1. `platform/at32/bsp/at_surf_f437_board_sd_card.c` — BSP 版本（已升级到 V2.2.4）
2. `platform/at32/fatfs/at32_sdio.c` — FatFS 适配层自带的旧版本（V2.1.1）

两个文件都定义了 `sd_init()`、`sd_power_on()`、`scr_find()` 等同名全局函数。
链接器在解析符号时，选择了 `fatfs` 库中的旧版本（因为 `fatfs` 库先于 `at32_bsp` 被链接）。

旧版本（V2.1.1）的 `sd_card.c` 有一个已知 bug：
- 在多块数据传输命令中使用 `SDIO_WAIT_FOR_INT`（等待中断模式）
- 如果 SDIO 中断没有正确触发，`scr_find()` 中的 while 循环永远不会退出

V2.2.4 的修复：
- 将 `SDIO_WAIT_FOR_INT` 改为 `SDIO_WAIT_FOR_NO`（轮询模式）
- 将 SDIO 高速时钟从 48MHz 降到 25MHz
- IRQ handler 中使用 `sdio_interrupt_flag_get()` 替代 `sdio_flag_get()`

## 为什么难以发现
1. **两个文件内容几乎相同**（2744 行 vs 2792 行），不容易注意到重复
2. **链接器不报错** — 静态库中的同名符号，链接器按库顺序选择第一个找到的
3. **在 BSP 文件中加的调试打印不生效** — 因为实际运行的是另一个文件的代码
4. **`#error` 测试确认文件在编译** — 但编译不等于链接（链接器选了另一个库的 .o）
5. **反汇编确认 `sd_init` 中没有 debug 调用** — 最终定位到符号冲突

## 修复
从 `fatfs` 库的编译列表中移除 `at32_sdio.c`，只保留 BSP 的 `at_surf_f437_board_sd_card.c`：

```cmake
add_library(fatfs STATIC
    ${FATFS_DIR}/fatfs_lib/source/ff.c
    ${FATFS_DIR}/fatfs_lib/source/ffsystem.c
    ${FATFS_DIR}/fatfs_lib/source/ffunicode.c
    # at32_sdio.c REMOVED — duplicate of at_surf_f437_board_sd_card.c
    ${BSP_DIR}/at_surf_f437_board_diskio.c
)
target_link_libraries(fatfs PUBLIC at32_hal at32_bsp)
```

## 教训
1. **永远不要在项目中保留两份功能相同的源文件** — 即使它们在不同目录
2. **链接器符号冲突是静默的** — 不会报 "multiple definition"（因为是静态库）
3. **升级 SDK 时要检查所有副本** — 不能只升级一份
4. **调试打印不生效时，先确认链接的是哪个 .o 文件**（`nm` + `objdump`）
5. **`-ffunction-sections` + `--gc-sections` 会掩盖问题** — 未引用的函数被丢弃，让人误以为代码没编译进去
