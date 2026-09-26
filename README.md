# 基于 STM32F407 的智能指纹考勤机

## 1. 项目简介

这是一个基于 STM32F407ZETx 和 FreeRTOS 的指纹考勤与门锁控制程序。系统通过 FPM383F 完成指纹录入和验证，通过 4×4 矩阵键盘完成密码、考勤和记录操作，并使用 OLED 显示时间、菜单与状态。密码摘要和考勤记录保存在 W25Q128，舵机用于开锁和自动上锁。

## 2. 硬件组成

| 部件 | 代码中的用途 |
| --- | --- |
| STM32F407ZETx | 主控 MCU |
| FPM383F | 指纹录入、1:N 验证、模板数量查询和指纹库清空，使用 USART2 |
| SSD1306 OLED | 日期、时间、菜单和操作结果显示，使用 GPIO 模拟 I2C |
| 4×4 矩阵键盘 | 六位密码、考勤、记录查看和管理操作 |
| S1～S4 独立按键 | 通过 EXTI 触发指纹录入、验证、数量查询和清空 |
| W25Q128 | 保存 SHA-256 密码摘要和最多 100 条考勤记录，使用 SPI1 |
| LSE 低速外部晶振 | 为 STM32 内部 RTC 提供时钟 |
| PWM 舵机 | 执行开锁和自动上锁，使用 TIM3 CH4；代码未限定舵机型号 |
| 蜂鸣器 | 指纹操作成功提示 |
| 串口调试接口 | USART1 输出初始化、操作结果和异常信息 |

当前源码没有 AT24C02 和 IWDG 的应用代码。`uart.c` 中保留了 USART3 驱动，但当前任务流程没有初始化或使用 USART3。

### 接口分配

| 模块 | STM32 引脚 | 说明 |
| --- | --- | --- |
| USART1 | PA9 / PA10 | 调试串口，当前程序只发送日志 |
| FPM383F | PA2 / PA3 | USART2 TX / RX，57600 bit/s |
| W25Q128 | PB3 / PB4 / PB5 / PB14 | SPI1 SCK / MISO / MOSI / CS |
| OLED | PB15 / PD10 | SCL / SDA，GPIO 模拟 I2C |
| 舵机 | PC9 | TIM3 CH4 PWM |
| 蜂鸣器 | PF8 | GPIO 输出 |
| S1～S4 | PA0 / PE2 / PE3 / PE4 | EXTI 输入 |
| 矩阵键盘行线 | PD6 / PD7 / PC6 / PC8 | 4×4 键盘扫描输出 |
| 矩阵键盘列线 | PC11 / PE5 / PA6 / PC7 | 4×4 键盘扫描输入 |

## 3. 软件环境

- Keil MDK，工程文件：`USER/project.uvprojx`
- Arm Compiler 5：ARMCC V5.06 update 5，build 528
- STM32F4 Standard Peripheral Library V1.4.0
- CMSIS Cortex-M4 Core V3.20
- FreeRTOS V9.0.0
- C

## 4. 功能

- 使用 FPM383F 录入指纹，模板 ID 范围为 1～50
- 1:N 指纹验证成功后开锁，10 秒后自动上锁
- 4×4 矩阵键盘输入六位数字密码
- 使用 `*` 保存或修改密码，使用 `#` 验证密码并开锁
- 密码先计算 SHA-256，再把 32 字节摘要保存到 W25Q128
- 使用六位密码加 `D` 获取 60 秒管理权限
- 指纹录入、清空指纹库和清空考勤记录需要管理权限
- 使用 `A` 发起指纹考勤，保存指纹 ID 和 RTC 日期时间；考勤验证不触发开锁
- 使用 `B` 在 OLED 分页显示本地考勤记录
- 使用 `C` 二次确认后清空考勤记录
- OLED 每秒刷新 RTC 日期和时间，并显示菜单、密码星号和操作结果
- 指纹操作成功时驱动蜂鸣器短响
- USART1 输出初始化、指纹、考勤、门锁和故障信息
- HardFault、MemManage、BusFault 和 UsageFault 发生时通过 USART1 输出故障类型

RTC 第一次初始化时写入源码中的固定日期和时间，正式记录考勤前需要校时。

## 5. FreeRTOS 任务设计

| Task | Priority | Function |
| --- | ---: | --- |
| `TaskAll_Init` | 3 | 初始化矩阵键盘、W25Q128、RTC、蜂鸣器、OLED、舵机和通信对象，创建业务任务后删除自身 |
| `TaskKeyboardUnlock` | 1 | 扫描 4×4 矩阵键盘，处理密码、管理权限、考勤和记录操作 |
| `TaskOLED_ShowTime` | 1 | 每秒读取 RTC，并刷新 OLED 顶部日期和时间 |
| `app_task_key` | 2 | 等待 S1～S4 的 EXTI 事件，在任务上下文中消抖并发送指纹命令 |
| `app_task_fpm` | 2 | 串行处理 FPM383F 命令，是当前业务中唯一访问 USART2 和指纹模块的任务 |
| `TaskServo` | 1 | 等待开锁请求，控制舵机并在保持时间结束后自动上锁 |

当前应用使用的 FreeRTOS 通信和同步机制如下：

- Queue：`FingerprintCommandQueue` 传递指纹命令；`FingerprintResultQueue` 返回考勤验证结果；`ServoQueue` 传递开锁保持时间。
- Mutex：`xOLEDMutex` 防止时钟任务、键盘任务和指纹任务同时写 OLED。
- Event Group：`KeyEventGroup` 接收 EXTI ISR 设置的 S1～S4 事件位。
- Critical Section：保护 `admin_active` 和 `admin_deadline` 管理权限状态。
- Task Notification：当前应用代码没有使用。
- Software Timer：`configUSE_TIMERS` 已开启且 `timers.c` 参与编译，但应用代码没有调用 `xTimerCreate` 创建软件定时器。

## 6. 系统流程

```text
上电
  ↓
SystemInit 和 USART1 初始化
  ↓
创建 TaskAll_Init，启动 FreeRTOS 调度器
  ↓
初始化键盘、W25Q128、RTC、蜂鸣器、OLED 和舵机
  ↓
创建键盘、OLED、指纹按键、指纹通信和舵机任务
  ↓
├─ 4×4 键盘：密码设置/验证、考勤、记录查看、记录清空、管理授权
├─ S1～S4：指纹录入、验证、数量查询、指纹库清空
├─ OLED：显示 RTC 日期时间、菜单和操作结果
└─ 舵机：接收开锁请求，保持 10 秒后上锁
```

考勤流程：

```text
按 A
  ↓
检查 RTC 状态
  ↓
向 app_task_fpm 发送考勤验证命令
  ↓
FPM383F 进行 1:N 匹配
  ↓
读取 RTC 日期时间
  ↓
向 W25Q128 追加考勤记录
```

## 7. 项目目录

```text
.
├── CMSIS/                 # Cortex-M4 内核文件和启动文件
├── DEVICE_LIB/            # STM32F4 标准外设库
│   ├── inc/
│   └── src/
├── FreeRTOS_INCLUDE/      # FreeRTOS 头文件
├── FreeRTOS_Portable/     # Cortex-M4F 端口
├── FreeRTOS_SOURCE/       # FreeRTOS 内核源码和 heap_4
├── HARDWARE/              # 指纹、键盘、OLED、RTC、Flash、舵机等驱动
├── SYSTEM/                # STM32F407 设备头文件和系统时钟配置
├── USER/                  # main、业务任务、FreeRTOS 配置和 Keil 工程
├── .gitignore
└── README.md
```

`OBJ/`、`LIST/`、`.vscode/` 和 Keil 用户级配置保留在本地，但不提交到仓库。

### 代码入口

- [`USER/main.c`](USER/main.c)：硬件初始化、FreeRTOS 对象初始化和任务创建。
- [`USER/app_keyboard.c`](USER/app_keyboard.c)：密码、管理权限、考勤和记录菜单。
- [`USER/app_fingerprint.c`](USER/app_fingerprint.c)：指纹按键事件和 FPM383F 命令处理。
- [`USER/app_servo.c`](USER/app_servo.c)：开锁请求和自动上锁。
- [`HARDWARE/w25q128.c`](HARDWARE/w25q128.c)：密码摘要和考勤记录存储。
- [`HARDWARE/FPM383F.c`](HARDWARE/FPM383F.c)：指纹模块协议和 USART2 接收。

## 8. 关键实现

### 8.1 指纹命令串行处理

`app_task_fpm` 通过 `FingerprintCommandQueue` 接收操作命令，避免多个任务同时访问 FPM383F。USART2 RX 中断接收模块应答，TIM5 以 1 ms 周期检测帧间空闲并标记一帧结束。

### 8.2 密码和管理权限

键盘任务只接受六位数字密码。密码以 SHA-256 摘要形式保存到 W25Q128 地址 `0x000000`。原密码验证通过后，管理权限保持 60 秒，用于修改密码、录入指纹和执行清空操作。

### 8.3 考勤记录存储

考勤记录从 W25Q128 地址 `0x001000` 开始保存，每条记录固定 16 字节，包含有效标志、指纹 ID 和日期时间。当前实现最多保存 100 条，满后停止追加；清空操作擦除整个记录扇区。

### 8.4 OLED 访问互斥

日期时间刷新和菜单显示共用 `xOLEDMutex`。时钟显示占用 OLED 顶部区域，菜单和记录显示使用底部四页。

### 8.5 舵机自动上锁

`ServoQueue` 长度为 1，新的开锁请求覆盖尚未处理的请求。门锁处于打开状态时再次验证成功，会重新计算保持时间；超时后由 `TaskServo` 上锁。

## 9. 编译和运行

1. 使用 Keil MDK 打开 `USER/project.uvprojx`。
2. 选择 `project` target。
3. 执行 Build。
4. 连接调试器，将程序下载到 STM32F407ZETx。
5. 连接 USART1 查看调试信息：9600 bit/s、8 data bits、1 stop bit、no parity、no flow control。
6. 第一次使用前确认 RTC 时间，并通过 4×4 键盘设置六位密码。

`OBJ/` 和 `LIST/` 未提交，克隆仓库后需要在 Keil 中重新 Build。

## 10. 当前限制

- RTC 只在备份域未初始化时写入 `RTC.c` 中的固定时间，程序没有校时菜单。
- 考勤记录最多保存 100 条，写满后停止追加，没有覆盖和导出功能。
- 指纹模板 ID 范围固定为 1～50。
- AT24C02、IWDG、任务通知和应用层软件定时器没有接入当前业务流程。
