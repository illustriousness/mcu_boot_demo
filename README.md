# mcu_boot_demo

GD32F30x 上的 Boot + App 启动/重定位示例工程。当前重点是实现：

- App 单 bin 可在任意 Flash 地址运行（例如 `0x08007800`、`0x0800A000`）。
- Boot 只做最小必要的移交（向量表 RAM shadow + 跳转上下文）。
- App 在 Reset 阶段自行完成 `.data/.bss` 初始化和 RAM 目标重定位。

## 目录结构

- `application/`：Boot 工程（含跳转、重定位准备、升级入口）。
- `test_app/`：App 工程（PIE/PIC，支持任意 Flash 地址运行）。
- `scripts/`：签名、烧录、PIC 重定位检查脚本。
- `linker_scripts/`：Boot 链接脚本。

## 当前内存/分区约定

定义见 `application/boot/boot_port.h`：

- Flash 基址：`0x08000000`，总大小 `256KB`
- Boot 区：`0x08000000` ~ `0x080077FF`（`0x7800`，30KB）
- Slot0（primary）：`0x08007800`，大小 `0x1B800`
- Slot1（secondary）：`0x08023000`，大小 `0x1B800`
- Scratch：`0x0803E800`，大小 `0x800`
- Param（持久参数区）：`0x0803F000`，大小 `0x1000`

Boot/App 共享 RAM 约定：

- App VTOR shadow：`0x2000BE00`，大小 `0x200`
- 约定文件：`application/boot/boot_port.h` 与 `test_app/app/link.ld`

## 启动链路（当前实现）

### 1) Boot 侧（`JumpToApplication` + `app_prepare_exec`）

关键文件：

- `application/main.c`
- `application/app_reloc.c`

Boot 做的事情：

- 关闭中断、清理部分外设状态。
- 解析 App 的 `__app_reloc_info` 头。
- 校验 `.rel.dyn`（仅允许白名单内 Flash 目标：向量表范围 + Reset 启动窗口）。
- 计算运行时 `delta`，得到正确的 `Reset_Handler` 入口地址。
- 设置 `MSP` 后通过 `AppEntry(0, 0)` 跳转到 App `Reset_Handler`。

Boot 不再做的事情：

- 不再在 Boot 里搬运/修正 App 向量表并设置 `VTOR`
- 不再在 Boot 里设置 `r9(pic_base)`
- 不再在 Boot 里拷贝 App `.data`
- 不再在 Boot 里清 App `.bss`
- 不在 Boot 里执行 RAM 目标重定位

### 2) App 侧（Reset 阶段）

关键文件：

- `test_app/app/startup_gd32f30x_hd.s`
- `test_app/app/app_runtime_reloc.c`
- `test_app/app/link.ld`

App 在 Reset 中完成：

- 自行计算运行时 `delta`。
- 自行拷贝并修正向量表到 `0x2000BE00`，然后设置 `VTOR`。
- 自行设置 `r9(pic_base)`。
- 使用 `_sidata + delta` 拷贝 `.data` 到 RAM。
- 清零 `.bss`。
- 调用 `app_runtime_relocate_from_reset(__app_reloc_info + delta, delta)`，对 RAM 目标的 `R_ARM_RELATIVE` 做修正。

## PIC/PIE 构建要求（App）

`test_app/.eide/eide.yml` 当前关键选项：

- C/C++：`-fPIC -msingle-pic-base -mpic-register=r9 -mno-pic-data-is-text-relative`
- Link：`-pie`

链接脚本 `test_app/app/link.ld` 中，以下内容已放到 RAM（`.data`）以减少 Flash 目标重定位风险：

- `.data.rel.ro*`
- `FSymTab` / `VSymTab`
- `.rti_fn*`

## 常用命令（MSH）

Boot 工程中可用：

- `run_app [addr]`
  - 不带参数默认跳 `0x08007800`
  - 示例：`run_app 0x0800a000`
- `boot`
  - 调用 `mcuboot_start()`，按 MCUboot 选槽策略启动
- `boot_state`：查看回滚状态
- `boot_set_trial <0|1>`：设置试运行槽位（armed）
- `boot_set_active <0|1>`：强制设置稳定槽位并清 trial
- `boot_param_reset`：重置回滚状态为默认值（slot0 confirmed）
- `app_diag` / `app_diag_clr`：查看/清除启动诊断标记
- `reset_flags`：查看复位来源标志
- `say_hello`

App 工程中可用：

- `cat_slot`：查看当前运行槽位
- `app_confirm`：确认当前槽位为健康版本（自定义 `BOOT_PARAM` 标记，可选）

## 构建与烧录

建议用 EIDE 分别构建：

1. Boot：工程根目录
2. App：`test_app/`

可用脚本：

- `scripts/flash.sh`：默认将 bin 烧录到 `0x08023000`（secondary）
- `scripts/imgtool.sh`：示例签名脚本（按你的实际输入 bin 路径调整）
- `scripts/check_pic_reloc.py`：链接后检查非法 Flash 重定位目标

示例（PIC 检查）：

```bash
python3 scripts/check_pic_reloc.py \
  --elf test_app/build/Debug/test_app.elf \
  --allow-flash-reloc 0x08000000:0x08000130
```

## MCUboot 当前状态

配置位于 `application/boot/mcuboot_config/mcuboot_config.h`，当前为：

- `MCUBOOT_DIRECT_XIP = 1`
- `MCUBOOT_IMAGE_NUMBER = 1`
- `MCUBOOT_VALIDATE_PRIMARY_SLOT = 1`

说明：

- 未启用 `MCUBOOT_DIRECT_XIP_REVERT`。
- 看门狗驱动位于 `Drivers/bsp/fwdg.c`。
- 自定义回滚状态机位于 `application/main.c`（`BOOT_PARAM` 持久化在 `0x0803F000`）。

注意：`application/main.c` 上电后先进入约 3 秒窗口，再按 `BOOT_PARAM` 决策自动跳转；
也可在窗口内手动执行 `boot` / `run_app`。

## 回滚策略建议（不在 App 引入 MCUboot API）

当前实现（已落地）：

- `boot_set_trial <slot>` 将升级槽标记为 trial（`confirmed=0`）
- 主循环状态机在 `main.c` 决策启动：
  - 若检测到 WDG 复位且上次启动的是 trial 槽，则自动回滚到 `active_slot`
  - 否则优先启动 trial 槽，并递增 `trial_boot_count`
- App 启动后执行 `app_confirm`（模板中默认自动调用）：
  - 写 `active_slot=current_slot`
  - 清 `trial_slot`
  - 置 `confirmed=1`
- App 主循环喂狗；若 App 卡死不喂狗，触发 WDG 复位后下次 Boot 回滚

## 调试提示

- 观察 `run_app` / `[app_reloc]` 日志可快速判断卡在哪个阶段。
- `APP_DIAG_MARK_ADDR` 目前使用 `0x2000BF80`（Boot 与 App 启动路径都会写入标记）。
- 遇到“能跑但 data 异常”优先检查：
  - App 是否确实从 Reset_Handler 进入（非直接跳 main）
  - `.data` LMA 拷贝是否加了 `delta`
  - `.rel.dyn` 是否仍有非白名单 Flash 目标
