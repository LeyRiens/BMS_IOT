#include "config.h"
#include "buffer.h"
#include "mqtt_at.h"
#include "state_user.h"
#include "led.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static DMA_HandleTypeDef rx_dma;
UART_HandleTypeDef huart1 = { &rx_dma, 0U, 0U };
LED_Handle_t Error_LED, NET_State_LED;
static uint32_t tick;
static uint8_t *rx_memory;
static uint16_t rx_size, rx_write;
static unsigned rx_starts, commands, publishes, payloads;
static uint8_t *payload;
static HAL_StatusTypeDef dma_status = HAL_OK;
static unsigned rx_failures;
uint32_t HAL_GetTick(void) { return tick; }
void HAL_NVIC_SetPriority(int i, uint32_t p, uint32_t s) { (void)i; (void)p; (void)s; }
void HAL_NVIC_EnableIRQ(int i) { (void)i; }
void Error_Handler(void) { CHECK(0); }
void LED_SetOn(LED_Handle_t *l) { l->mode = LED_MODE_ON; }
void LED_SetOff(LED_Handle_t *l) { l->mode = LED_MODE_OFF; }
HAL_StatusTypeDef LED_SetBlink(LED_Handle_t *l, uint32_t t) { (void)t; l->mode = LED_MODE_BLINK; return HAL_OK; }
void LED_Update(LED_Handle_t *l) { (void)l; }
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *u, uint8_t *p, uint16_t n)
{ if (rx_failures) { rx_failures--; return HAL_ERROR; } rx_memory = p; rx_size = n; rx_write = 0; rx_dma.remaining = n; u->RxState = HAL_UART_STATE_BUSY_RX; rx_starts++; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u, uint8_t *p, uint16_t n, uint32_t t)
{ (void)u; (void)n; (void)t; commands++; if (memcmp(p, "AT+MQTTPUBRAW", 12) == 0) publishes++; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *u, uint8_t *p, uint16_t n)
{ (void)u; CHECK(n == CAN_BLOCK_TX_SIZE); if (dma_status != HAL_OK) return dma_status; payload = p; payloads++; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u) { (void)u; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u) { u->RxState = HAL_UART_STATE_READY; return HAL_OK; }
static void feed(const char *text)
{
    while (*text) {
        CHECK(huart1.RxState == HAL_UART_STATE_BUSY_RX);
        rx_memory[rx_write++] = (uint8_t)*text++;
        rx_dma.remaining--;
        if (rx_write == rx_size) {
            if (rx_dma.Init.Mode == DMA_CIRCULAR) { rx_write = 0; rx_dma.remaining = rx_size; }
            else huart1.RxState = HAL_UART_STATE_READY;
        }
    }
}
static void poll(void) { MQTTAT_Poll(); StateUser_Poll(); }
static void setup(void)
{
    tick = 0; commands = publishes = payloads = rx_starts = 0; dma_status = HAL_OK; rx_failures = 0;
    rx_dma.Init.Mode = DMA_CIRCULAR;
    Buffer_Init(); CHECK(MQTTAT_Init() == HAL_OK); StateUser_Init();
}
static void connect_gateway(void)
{
    unsigned i;
    for (i = 0; i < 6; i++) {
        poll(); feed(i == 5 ? "+MQTTCONNECTED:0,1\r\nOK\r\n" : "OK\r\n"); poll();
    }
    CHECK(MQTTAT_IsConnected());
}
static void block(uint8_t marker)
{
    uint8_t data[8] = {0}; unsigned i; data[0] = marker;
    Buffer_ReceiveFrame(HEAD_CAN_ID, data);
    for (i = 1; i < CAN_FRAME_COUNT - 1; i++) Buffer_ReceiveFrame(0x150U, data);
    Buffer_ReceiveFrame(TAIL_CAN_ID, data);
}
static void start_payload(void)
{ block(1); poll(); CHECK(publishes == 1); feed("OK\r\n>"); poll(); CHECK(payloads == 1); }
static void race_test(void)
{
    setup(); connect_gateway(); start_payload();
    MQTTAT_Poll();
    HAL_UART_TxCpltCallback(&huart1); /* IRQ between the two real main-loop calls. */
    StateUser_Poll();
    feed("+MQTTPUB:OK\r\n"); poll(); poll();
    CHECK(MQTTAT_GetResult() == MQTTAT_RESULT_IDLE);
    CHECK(g_state_user_error_count == 0);
}
static void rx_error_test(void)
{
    unsigned before;
    setup(); CHECK(MQTTAT_Test() == HAL_OK); feed("O"); MQTTAT_Poll(); before = rx_starts;
    huart1.RxState = HAL_UART_STATE_READY; huart1.ErrorCode = HAL_UART_ERROR_ORE;
    HAL_UART_ErrorCallback(&huart1); MQTTAT_Poll();
    CHECK(rx_starts == before + 1);
    CHECK(MQTTAT_GetResult() == MQTTAT_RESULT_ERROR);
    CHECK(MQTTAT_Abort() == HAL_OK);
    CHECK(MQTTAT_Test() == HAL_OK); feed("OK\r\n"); MQTTAT_Poll();
    CHECK(MQTTAT_GetResult() == MQTTAT_RESULT_OK);
}
static void latest_test(void)
{
    setup(); connect_gateway(); start_payload();
    block(2); block(3); poll(); CHECK(publishes == 1);
    HAL_UART_TxCpltCallback(&huart1); feed("+MQTTPUB:OK\r\n"); poll(); poll();
    CHECK(tick == 0); CHECK(publishes == 2); /* No 20 ms wait. */
    block(4); feed("OK\r\n>"); poll();
    CHECK(payloads == 2); CHECK(payload[4] == 4);
    HAL_UART_TxCpltCallback(&huart1); feed("+MQTTPUB:OK\r\n"); poll(); poll();
    CHECK(publishes == 2); /* No duplicate when no newer block exists. */
}
static void raw_abort_test(void)
{
    unsigned before;
    setup(); connect_gateway(); block(1); poll();
    dma_status = HAL_ERROR; feed("OK\r\n>"); poll(); before = commands;
    tick += MQTT_STARTUP_RETRY_MS + 1; poll(); poll();
    CHECK(commands == before); /* Never send AT into an unfinished RAW payload. */
}
static void snapshot_test(void)
{
    unsigned i; uint8_t saved[CAN_BLOCK_TX_SIZE];
    setup(); connect_gateway(); start_payload(); memcpy(saved, payload, sizeof(saved));
    for (i = 2; i < 10; i++) block((uint8_t)i);
    CHECK(memcmp(saved, payload, sizeof(saved)) == 0);
    CHECK(global_block_seq == 9); CHECK(can_tx_collision_count == 0);
    HAL_UART_TxCpltCallback(&huart1); feed("+MQTTPUB:OK\r\n"); poll(); poll();
    feed("OK\r\n>"); poll(); CHECK(payload[4] == 9);
}
static void early_result_test(void)
{
    setup(); connect_gateway(); start_payload();
    feed("+MQTTPUB:OK\r\n"); poll();
    CHECK(MQTTAT_GetResult() == MQTTAT_RESULT_PENDING);
    HAL_UART_TxCpltCallback(&huart1); poll();
    CHECK(MQTTAT_GetResult() == MQTTAT_RESULT_IDLE);
}
static void prompt_echo_test(void)
{
    setup(); connect_gateway(); block(1); poll();
    feed("AT+MQTTPUBRAW=0,\"a>b\",1514,0,0\r\n"); poll(); CHECK(payloads == 0);
    feed("OK\r\n>"); poll(); CHECK(payloads == 1);
}
static void rx_wrap_test(void)
{
    unsigned i;
    setup(); CHECK(MQTTAT_Test() == HAL_OK);
    for (i = 0; i < MQTT_AT_RX_SIZE - 1; i++) { feed("\n"); MQTTAT_Poll(); }
    feed("O"); MQTTAT_Poll(); feed("K\r\n"); MQTTAT_Poll();
    CHECK(MQTTAT_GetResult() == MQTTAT_RESULT_OK); CHECK(rx_starts == 1);
}
static void raw_timeout_test(void)
{
    unsigned before;
    setup(); connect_gateway(); block(1); poll(); before = commands;
    tick += MQTT_AT_PUBLISH_TIMEOUT_MS; poll();
    CHECK(MQTTAT_NeedsEspReset());
    tick += MQTT_STARTUP_RETRY_MS + 1; poll(); poll(); CHECK(commands == before);
}
static void reject_test(void)
{
    unsigned before;
    setup(); connect_gateway(); block(1); poll(); feed("ERROR\r\n"); poll();
    CHECK(!MQTTAT_NeedsEspReset()); before = commands;
    tick += MQTT_STARTUP_RETRY_MS + 1; poll(); CHECK(commands == before + 1);
}
static void reserved_test(void)
{
    unsigned i; uint8_t data[8] = {0};
    setup(); connect_gateway(); block(7); poll();
    for (i = 0; i < 3; i++) { Buffer_ReceiveFrame(HEAD_CAN_ID, data); Buffer_ReceiveFrame(TAIL_CAN_ID, data); }
    Buffer_ReceiveFrame(HEAD_CAN_ID, data); /* Latest acquisition block becomes incomplete. */
    feed("OK\r\n>"); poll(); CHECK(payloads == 1); CHECK(payload[4] == 7);
}
static void rx_retry_test(void)
{
    unsigned before;
    setup(); CHECK(MQTTAT_Test() == HAL_OK); before = rx_starts;
    huart1.RxState = HAL_UART_STATE_READY; rx_failures = 1;
    HAL_UART_ErrorCallback(&huart1); MQTTAT_Poll();
    CHECK(rx_starts == before); CHECK(MQTTAT_Abort() == HAL_OK);
    CHECK(MQTTAT_Test() == HAL_BUSY);
    MQTTAT_Poll(); CHECK(rx_starts == before + 1); CHECK(MQTTAT_Abort() == HAL_OK);
    CHECK(MQTTAT_Test() == HAL_OK); feed("OK\r\n"); MQTTAT_Poll();
    CHECK(MQTTAT_GetResult() == MQTTAT_RESULT_OK);
}
static void raw_rx_test(void)
{
    unsigned before;
    setup(); connect_gateway(); start_payload(); before = commands;
    huart1.RxState = HAL_UART_STATE_READY; HAL_UART_ErrorCallback(&huart1); poll();
    CHECK(MQTTAT_NeedsEspReset());
    tick += MQTT_STARTUP_RETRY_MS + 1; poll(); CHECK(commands == before);
}
static void raw_dma_timeout_test(void)
{
    unsigned before;
    setup(); connect_gateway(); start_payload(); before = commands;
    tick += MQTT_AT_PUBLISH_TIMEOUT_MS; poll(); CHECK(MQTTAT_NeedsEspReset());
    tick += MQTT_STARTUP_RETRY_MS + 1; poll(); CHECK(commands == before);
}
static void esp_reset_test(void)
{
    unsigned before;
    setup(); connect_gateway(); block(1); poll(); dma_status = HAL_ERROR;
    feed("OK\r\n>"); poll(); CHECK(MQTTAT_NeedsEspReset()); before = commands;
    feed("ready\r\n"); poll(); CHECK(!MQTTAT_NeedsEspReset());
    tick += MQTT_STARTUP_RETRY_MS + 1; poll(); CHECK(commands == before + 1);
}
int main(int argc, char **argv)
{
    CHECK(argc == 2);
    if (strcmp(argv[1], "race") == 0) race_test();
    else if (strcmp(argv[1], "rx_error") == 0) rx_error_test();
    else if (strcmp(argv[1], "latest") == 0) latest_test();
    else if (strcmp(argv[1], "raw_abort") == 0) raw_abort_test();
    else if (strcmp(argv[1], "snapshot") == 0) snapshot_test();
    else if (strcmp(argv[1], "early_result") == 0) early_result_test();
    else if (strcmp(argv[1], "prompt_echo") == 0) prompt_echo_test();
    else if (strcmp(argv[1], "rx_wrap") == 0) rx_wrap_test();
    else if (strcmp(argv[1], "raw_timeout") == 0) raw_timeout_test();
    else if (strcmp(argv[1], "reject") == 0) reject_test();
    else if (strcmp(argv[1], "reserved") == 0) reserved_test();
    else if (strcmp(argv[1], "rx_retry") == 0) rx_retry_test();
    else if (strcmp(argv[1], "raw_rx") == 0) raw_rx_test();
    else if (strcmp(argv[1], "raw_dma_timeout") == 0) raw_dma_timeout_test();
    else if (strcmp(argv[1], "esp_reset") == 0) esp_reset_test();
    else CHECK(0);
    printf("gateway_test %s: OK\n", argv[1]); return 0;
}
