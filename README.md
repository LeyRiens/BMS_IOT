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
后续计划
- MQTT 下行通信
- SD 卡本地数据记录
- 更远距离的蜂窝网络通信
- 更完善的异常处理
## Status
Work in Progress
