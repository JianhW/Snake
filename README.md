# FPGA RISC-V VGA Game Console

这是一个运行在 Efinix Trion FPGA 上的简易游戏机工程。项目把 Sapphire RISC-V SoC、APB 显存桥、VGA 扫描输出、按键/拨码开关、数码管、LED 和蜂鸣器组合在一起，由 RISC-V 软核运行 C 语言游戏程序，通过内存映射显存绘制画面，FPGA 逻辑负责把 64 x 64 帧缓冲区实时放大输出到 640 x 480 VGA 显示器。

当前软件内置两个游戏：

- Snake：贪吃蛇，支持加速、穿墙、暂停、主题切换、得分和历史最高分。
- Tetris：俄罗斯方块，支持左右移动、旋转、加速下落、暂停、下一块预览、得分和历史最高分。

## 项目特性

- Efinity 工程目标器件：`Trion T20F256`
- 顶层模块：`VGA_test`
- 软核：Efinix Sapphire RISC-V SoC，RV32IM，片上 RAM 约 80 KB
- 显示：VGA 640 x 480 @ 60 Hz 时序，RGB565 输出
- 软件绘图分辨率：64 x 64，硬件端等比例映射到 VGA 有效显示区
- 显存接口：RISC-V 通过 APB 写入 `0xF8100000`
- 外设：4 个方向按键、9 个拨码开关、8 位数码管、10 个 LED、蜂鸣器
- 软件：裸机 C 程序，无操作系统

## 系统结构

```text
+--------------------+         APB         +---------------------+
| Sapphire RISC-V SoC| ------------------> | apb_framebuffer     |
|                    |                     | - 64x64 framebuffer |
| - game logic       |                     | - switch register   |
| - RGB565 drawing   |                     | - seven-seg regs    |
| - GPIO buttons     |                     +----------+----------+
| - buzzer control   |                                |
+----------+---------+                                | pixel_addr/pixel_data
           | GPIO                                     |
           v                                          v
   buttons / buzzer                         +---------------------+
                                            |  VGA timing/scaler  |
                                            |  640x480 RGB output |
                                            +---------------------+
```

RISC-V 程序只需要把像素写到内存映射显存中。`apb_framebuffer.v` 把 APB 写操作转换成双口 RAM 写入，VGA 扫描逻辑在另一个端口按当前行列坐标读取像素。这样游戏逻辑和显示扫描互相解耦，软件侧可以用普通数组方式绘制画面。

## 内存映射

| 地址 | 方向 | 说明 |
| --- | --- | --- |
| `0xF8100000` - `0xF8101FFF` | R/W | 64 x 64 framebuffer，每个像素按 16-bit RGB565 写入 |
| `0xF8102000` | R | 拨码开关输入寄存器，低 5 位对应 `sw5` - `sw9` |
| `0xF8102004` | W/R | 数码管时间寄存器，4 位 BCD |
| `0xF8102008` | W/R | 数码管分数寄存器，4 位 BCD |

硬件内部为了节省 BRAM，把 RGB565 压缩为 8-bit 存储，再在读出时扩展回 RGB565。软件仍然按 16-bit RGB565 方式写像素。

## 操作方式

### 按键

| 按键 | 游戏菜单 | Snake | Tetris |
| --- | --- | --- | --- |
| `BUT1` | 选择上一个选项 | 向上/开始 | 旋转/开始 |
| `BUT2` | 选择下一个选项 | 向下/开始 | 加速下落/开始 |
| `BUT3` | 无动作 | 向左/开始/返回菜单 | 左移/返回菜单 |
| `BUT4` | 进入当前选项 | 向右/开始 | 右移/开始 |

按键在软件中按上升沿处理，避免长按被重复当成多次菜单选择。`BUT3` 在暂停页和结束页用于返回游戏菜单。

### 拨码开关

| 开关 | 软件功能 | 说明 |
| --- | --- | --- |
| `sw5` | 速度位 0 | 和 `sw6` 组合选择 4 档速度 |
| `sw6` | 速度位 1 | `00` 最慢，`11` 最快 |
| `sw7` | Snake 穿墙 | 打开后蛇撞边界会从另一侧出现 |
| `sw8` | 暂停 | 游戏中打开暂停，关闭继续 |
| `sw9` | 主题 | 切换另一套颜色主题 |

`sw1` - `sw4` 由硬件直接用于 LED 动效选择，优先级为 `sw1` 最高。全关时显示默认柔和流动效果。

## 显示与外设

- VGA：顶层逻辑把输入时钟二分频得到 25 MHz 像素时钟，使用 640 x 480 标准时序输出 `VGA_HS`、`VGA_VS` 和 RGB565 色彩。
- 数码管：高 4 位显示游戏用时，低 4 位显示分数，软件以 BCD 写入两个寄存器，硬件动态扫描 8 位数码管。
- LED：硬件中实现 5 种 PWM 动效，包括流动、交叉拖尾、呼吸高亮、中心涟漪和伪随机闪烁。
- 蜂鸣器：软件通过 Sapphire SoC GPIO bit3 控制，吃到食物或消行时发出短音。

## 目录结构

```text
.
|-- VGA.xml                         # Efinity 工程文件
|-- VGA_test.v                      # FPGA 顶层：SoC、APB、VGA、数码管、LED、蜂鸣器
|-- apb_framebuffer.v               # APB 显存桥和寄存器映射
|-- VGA_test.peri.xml               # 外设和引脚配置数据库
|-- VGA_test_io.isf                 # IO 约束导出
|-- ip/
|   |-- framebuffer/                # Efinity framebuffer IP
|   `-- riscv_soc/                  # Sapphire RISC-V SoC IP
|-- embedded_sw/
|   `-- riscv_soc/
|       |-- bsp/efinix/EfxSapphireSoc/
|       `-- software/standalone/gpioDemo/
|           `-- src/main.c          # 游戏菜单、Snake、Tetris、绘图和输入逻辑
`-- outflow/
    |-- VGA_test.bit                # 已生成 bitstream
    `-- VGA_test.hex                # 已生成配置 hex
```

> 注意：`outflow/`、`work_*` 和 `embedded_sw/.metadata/` 多数是 Efinity/Eclipse 生成物。上传 GitHub 时可以根据需要保留已验证的 bitstream，也可以用 `.gitignore` 排除临时构建目录。

## 构建硬件

1. 使用 Efinity 2024.1 或兼容版本打开 `VGA.xml`。
2. 确认目标器件为 `Trion T20F256`，顶层为 `VGA_test`。
3. 确认 IP 路径 `ip/framebuffer` 和 `ip/riscv_soc` 可正常加载。
4. 运行综合、布局布线和 bitstream generation。
5. 生成结果位于 `outflow/VGA_test.bit` 和 `outflow/VGA_test.hex`。

工程当前 `VGA.xml` 中记录的最近一次 bitstream flow 状态为 `pass`。

## 构建软件

游戏程序位于：

```text
embedded_sw/riscv_soc/software/standalone/gpioDemo/src/main.c
```

在配置好 Efinix RISC-V 工具链后，可以进入软件目录构建：

```sh
cd embedded_sw/riscv_soc/software/standalone/gpioDemo
make BSP=EfxSapphireSoc
```

默认工具链前缀是：

```text
riscv-none-embed-
```

构建产物会生成在 `build/` 下，包括：

- `gpioDemo.elf`
- `gpioDemo.bin`
- `gpioDemo.hex`
- `gpioDemo.asm`

如果工具链不在 `PATH` 中，需要先配置 Efinity RISC-V IDE 或手动设置 `RISCV_BIN`。

## 烧录与运行

1. 将 FPGA bitstream `outflow/VGA_test.bit` 下载到开发板。
2. 通过 Efinity RISC-V IDE/OpenOCD/JTAG 下载或调试 `gpioDemo.elf`。
3. 连接 VGA 显示器，复位后进入游戏菜单。
4. 使用 `BUT1/BUT2` 选择游戏，`BUT4` 进入。
5. 游戏中数码管显示时间和分数，蜂鸣器在得分事件时提示。

## 关键实现点

- `VGA_test.v` 中把 Sapphire SoC 的 16-bit APB 地址拼接成完整 `0xF810xxxx` 地址，保证软件地址和硬件 framebuffer 地址一致。
- VGA 缩放使用 `x = h * 64 / 640`、`y = v * 64 / 480`，避免简单除法导致底部访问超过 64 行。
- `apb_framebuffer.v` 同时提供 framebuffer、switch、time、score 四类寄存器区域，减少额外外设 IP。
- Snake 使用 32 x 32 逻辑棋盘，每个格子占 2 x 2 像素。
- Tetris 使用 9 x 16 逻辑棋盘，每个格子占 3 x 3 像素，并在右侧绘制下一块和分数面板。
- 软件中使用机器定时器计算游戏用时，并将用时/分数转换为 BCD 写给数码管。

## 后续可以改进

- 增加 `.gitignore`，排除 Efinity/Eclipse 临时文件和二进制索引。
- 将游戏工程从 `gpioDemo` 重命名为更贴近项目的 `game_console`。
- 为按键增加硬件或软件消抖参数配置。
- 增加更多游戏，复用现有 framebuffer 绘图库。
- 增加截图、接线图和开发板型号说明，方便他人复现。
