# Claudee 固件 (KL25Z)

> Claudee 桌宠的板载固件：裸机 C，NXP **MKL25Z128**（Cortex-M0+） Blazar 教学板。
> 通过 UART 接收 Claude Code 的状态/遥测，在 320×240 TFT 上把像素螃蟹 **Clawd** 画出来；
> 收到权限请求时用**加速度计深蹲**或**跑酷小游戏**做挑战，过关回传放行字节。
>
> PC 侧（Windows/WSL 桥 + Claude Code 钩子）见 [`../bridge`](../bridge)。

---

## 目录结构

```
firmware/
├─ Sources/              固件源码（自写模块 + 复用驱动）
├─ Project_Headers/      NXP 设备头：MKL25Z4.h（寄存器定义）· derivative.h · bme.h
├─ Project_Settings/
│  ├─ Startup_Code/      启动代码：kinetis_sysinit.c（中断向量表）· startup_gcc.c · flash_config.c
│  └─ Linker_Files/      MKL25Z128_flash.ld（链接脚本）
└─ Makefile              arm-none-eabi-gcc + J-Link
```

## 模块速览（`Sources/`）


| 文件 | 职责 | 关键接口 |
|---|---|---|
| `main.c` | 顶层状态机 `NORMAL ⇄ WAIT`；非阻塞主循环；上电初始化各模块 | — |
| `claudee.h` | 全局类型 / 协议字节 / 调色板 | `cc_state_t`, `challenge_t`, `CLAWD_ORANGE`, `ACCEPT_BYTE`/`DENY_BYTE` |
| `comm.c/.h` | UART1 RX 中断收状态(含 ASK)+ 遥测(0xFE)+ 情境(0xFC) 帧；TX 回传放行；3s 静默→SLEEP | `comm_init/comm_state/comm_send_accept/comm_send_deny/comm_get_telemetry/comm_get_context` |
| `display.c/.h` | 状态屏（五态 + 眼神表情 + ✶脉冲 + 轮播状态词 + 屏底遥测）+ 挑战屏 | `disp_init/disp_render/disp_telemetry/disp_set_context/disp_challenge_enter/disp_squat_count/disp_allowed` |
| `clawd_art.h` | 20×12 索引位图 Clawd（平头顶方身 + 两侧钳 + 四腿），由 display 按 scale 放大叠表情 | `clawd_body`, `CLAWD_W/H`, `CLAWD_EYE_*` |
| `gfx.c/.h` | 绘图：逐像素 `gfx_fill/draw_indexed/text` + **开窗流式** `gfx_fill_fast/gfx_blit_fast`（列优先，匹配面板 GRAM 自增） | 见 .h |
| `game.c/.h` | 跑酷：整数物理 + 3 条命 + 10 个对手图标 + 空中障碍 + 10 分放行 | `game_reset/game_update(jump,exit,now)/game_score`, `GAME_TARGET=10` |
| `squat.c/.h` | 深蹲检测：三轴平方幅值 `|a|²` + 双阈值迟滞状态机（无 sqrt） | `squat_reset/squat_update`, `SQUAT_TARGET=10` |
| `input.c/.h` | PORTA 按键中断 + 去抖 + 预选模式（A 唤起挑战 / B 切换或拒绝 / C 跳跃） | `input_init/input_poll/input_mode` |
| `led.c/.h` | 4 颗板载 LED 状态灯：PIT(IRQ22) ~8kHz 软件 PWM 呼吸/快闪，**全程仅写 PORTC**（避开与 LCD 抢 GPIOB） | `led_init/led_set/led_from_cc` |
| `sfx.c/.h` | TPM0/PTC8 蜂鸣器（非阻塞）+ 旋钮音量（ADC0_SE14 → PWM 占空比） | `sfx_init/sfx_update/sfx_tick/ok/deny/jump/hit` |
| `timebase.c/.h` | SysTick 1ms 时基（整个非阻塞循环的节拍） | `tb_init/tb_millis/tb_due` |
| `UART1.c/.h` | UART1 初始化 + 收发助手 | `UART1_PutChar/UART1_GetChar` |

**复用驱动**

| 文件 | 作用 |
|---|---|
| `Blazar_TFTLCD.c/.h` + `font.h` | ILI9341 320×240 TFT，8 位并口（8080 时序） |
| `MMA8451Q.c/.h` | 三轴加速度计，I²C0 |
| `KL2x_gpio.c/.h` | GPIO 端口时钟/引脚助手 |
| `KL2x_uart.c/.h` | UART1 初始化 + 收发助手 |

## 引脚 / 外设

| 外设 | 引脚 | 备注 |
|---|---|---|
| LCD 数据(8 位并口) | PTD0–PTD7 | ILI9341，横屏 RGB565 |
| LCD 控制 | CS=PTB9 · RS=PTB8 · WR=PTB11 · RD=PTB10 · RST=PTE31 · 背光=PTA19 | 8080 写：置数据 → WR↓↑ 锁存 |
| 加速度计 I²C0 | SCL=PTE24 · SDA=PTE25 (MUX5) | MMA8451Q @ `0x1D` |
| 串口 UART1 | RX=PTC3 · TX=PTC4 (ALT3) | 9600 8N1，`SBR = 时钟/(16×波特率) ≈ 68` → `BDL=0x44` |
| 蜂鸣器 | TPM0_CH4 = PTC8 (ALT3) | PWM 输出；占空比随旋钮音量 |
| 音量旋钮 | PTC0 = ADC0_SE14 | 12 位采样 0–4095 → 映射 `C4V` |
| 按键(全 PORTA) | A=PTA14 · B=PTA16 · C=PTA17 | 低有效，下降沿，200ms 去抖 |
| 板载 4 LED | 组选 PTC6/7/12/13 + 红 PTC9 | **仅用红 + PORTC**，单写者，零总线竞争 |

## 中断

| 源 | 向量 / 号 | 用途 |
|---|---|---|
| SysTick | — | 1ms 时基 |
| **UART1 RX** | **IRQ13** (`NVIC_ISER |= 1<<13`) | 收状态码 / 遥测 / 情境帧 |
| **PORTA 按键** | **IRQ30** (`1<<30`) | A/B/C 去抖 |
| **PIT 通道0** | **IRQ22** (`1<<22`) | 4 LED 软件 PWM |

## 串口协议（9600 8N1）

| PC → 板 | 含义 | | 板 → PC | 含义 |
|---|---|---|---|---|
| `0x00`–`0x07` | 状态码（SLEEP/STARTED/WORKING/IDLE/NEEDPERM/…/ASK） | | `'A'` (0x41) | accept 放行 |
| `0xFE … 0x0A` | 遥测帧 ASCII（如 `Opus $0.22 40.8k 4%`） | | `'D'` (0x44) | deny 放弃 |
| `0xFC … 0x0A` | 情境帧 ASCII（工具命令 / ASK 问题） | | | |

板子 **>3s 收不到任何字节 → 自动回 SLEEP**（拔线 / CC 退出 / 桥崩）。

---

## 构建

**工具链**：GNU Arm Embedded（`arm-none-eabi-gcc`）+ `make` + SEGGER **J-Link**。

> ⚠️ **编译 + 烧录在 Windows 做**（J-Link 经 USB-SWD）。WSL 里只能做语法检查，不能烧录。

```bash
make            # 编译 -> build/app.{elf,bin,hex} + 打印 size
make clean      # 清 build/
make flash      # 经 J-Link 烧录 build/app.bin（也可在 VS Code 里 F5）
make erase      # 整片擦除
```

要点（已在 `Makefile` 配好）：`-mcpu=cortex-m0plus -mthumb`、`-Os`、`-std=gnu99`、
`-DCPU_MKL25Z128VLK4`、`-IProject_Headers -IProject_Settings/Startup_Code -ISources`、
链接脚本 `Project_Settings/Linker_Files/MKL25Z128_flash.ld`。
`Makefile` 用 `wildcard Sources/*.c`——**新增 `.c` 丢进 `Sources/` 即自动编译**，不用改 Makefile。

**语法检查**（任意平台，无需 J-Link；上板前自查）：

```bash
cd firmware
for f in Sources/*.c; do
  gcc -fsyntax-only -std=gnu99 -Wall -Wextra -Wno-unused-parameter \
      -DCPU_MKL25Z128VLK4 -IProject_Headers -IProject_Settings/Startup_Code -ISources "$f" || break
done && echo "all clean"
```