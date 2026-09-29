# STM32 CAN to MQTT Gateway

一个基于 STM32F103C8T6 和 ESP8266 的实验性 CAN 数据物联网网关。

## 功能

第一版实现单向数据上传：

```text
CAN
 ↓
STM32F103C8T6
 ↓
UART
 ↓
ESP8266
 ↓
Wi-Fi
 ↓
MQTT
```
STM32 持续接收 CAN 总线数据，并通过 UART 发送给 ESP8266，由 ESP8266 负责 Wi-Fi 和 MQTT 通信。

## 数据规模
- 约 120 个标准 CAN 帧 / 100 ms

## 当前硬件
- STM32F103C8T6
- ESP8266
- CAN 收发器

## 后续计划
- MQTT 下行通信
- SD 卡本地数据记录
- 蜂窝网络通信
- 异常处理

# Status
Work in Progress

## 当前分支的上传行为

- 空闲且有更新的完整 CAN block 时立即启动一次 MQTT RAW 发布，没有固定 20 ms 发送周期。
- 等待 ESP 的 `>` 提示期间若有更新的完整 block，到提示时优先使用最新块。没有更新则发送已准备的快照。
- 一次只进行一个发布事务，收到 `+MQTTPUB:OK` 且 UART 发送完成后才开始下一次。没有新 block 不重复发送。
- 忙碌期间允许旧采集块被覆盖；不排队补发历史块，也不自动重发已经开始发送但发布失败的块。UART DMA 使用独立的 1514 字节快照。
- 每个完整块目前要求 HEAD 到 TAIL 恰好 126 个标准、DLC=8 的数据帧。首尾 ID 必须按实际发送协议配置；中间帧没有 ID 白名单或顺序验证，部署时需确认总线上没有会混入本批次的其他报文。
- payload 为 126 条记录，每条是小端 32 位标准 ID 和 8 字节数据，最后附小端 16 位块序号，共 1514 字节；不发送 C 结构体末尾的填充。序号按有效采集块递增，允许回绕和跳号。

## 配置及故障恢复

在 `STM32_CAN_TO_USART/Core/Inc/config.h` 配置 Wi-Fi、broker、topic、首尾 CAN ID。默认首尾 ID `0xFFFF`、空 broker/topic 会禁止上传。UART 为 460800、8N1，ESP 的实际 UART 配置必须匹配。

当前一次发送 1514 字节，ESP-AT 固件的 MQTT 缓冲还需要容纳 topic 和协议头。默认容量配置 512 不满足要求，因此会停在 `CONFIG_REQUIRED`；必须确认并配置 ESP 固件实际容量，不能只修改 STM32 中的宏来绕过检查。

RX 使用 1024 字节循环 DMA。UART 错误后主循环重新启动 RX 并丢弃未完成的响应行。普通命令失败按现有启动流程重试。

没有配置 ESP 的 RST/EN 控制引脚。若 RAW 命令可能已经被接受，却没有收到明确的终结响应（例如提示丢失、payload DMA 启动失败或发送中断），系统进入 `ESP_RESET_REQUIRED`，错误灯闪烁，停止后续 AT 命令。`MQTTAT_NeedsEspReset()` 可用于诊断本地 abort 后的这一状态。手动复位 ESP 后，只有收到其 `ready` 才解除锁定并重新连接；也可以整板重新上电。只复位 STM32 不能保证 ESP 已退出收数状态。收到明确命令拒绝或发布失败响应时不进入此锁定。

## 状态灯

- CAN 状态灯：每提交一个完整有效 block 短闪一次，不依赖 MQTT 是否在线。
- Error 灯：连接重试为单闪，配置错误为三闪，等待手动复位 ESP 为双闪；各闪码循环显示。
- NET 状态灯：MQTT 连接成功时常亮。
- CAN 与 Error 灯的脉冲时间、间隔和闪码数量在 `Core/Inc/config.h` 配置。

## 验证

Windows 安装 GCC 后，在仓库根目录运行 `./tests/run.ps1`。脚本在临时目录编译并运行缓冲区测试及带 HAL 替身的网关回归测试，覆盖完成中断竞争、RX 故障恢复、最新块立即发布、快照覆盖保护、提前到达的发布响应、循环 RX 回绕和 RAW 故障锁定及 ESP 复位恢复。它不替代 Keil 完整构建及实际 CAN/ESP8266 联调。
