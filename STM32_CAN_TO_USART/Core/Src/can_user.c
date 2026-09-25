#include "can_user.h"



HAL_StatusTypeDef CAN_UserInit(void)
{
    CAN_FilterTypeDef filter = {0};
    HAL_StatusTypeDef status;

    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    
    filter.FilterIdHigh = (0x100U << 5);
    filter.FilterIdLow = 0x0000U;
    filter.FilterMaskIdHigh = (0x100U << 5);
    filter.FilterMaskIdLow = 0x0000U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;

    status = HAL_CAN_ConfigFilter(&hcan, &filter);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_CAN_Start(&hcan);
    if (status != HAL_OK)
    {
        return status;
    }

    return HAL_CAN_ActivateNotification(
        &hcan,
        CAN_IT_RX_FIFO0_MSG_PENDING
    );
}



void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can)
{
    CAN_RxHeaderTypeDef header;
    buffer_pointer_t *write = &g_buffer_write;
    frame_t *frame;

    if (can != &hcan) {
        return;
    }

    while (HAL_CAN_GetRxFifoFillLevel(can, CAN_RX_FIFO0) > 0U) {
        frame = &write->current_buffer
                     ->block[write->block_index]
                     .frame[write->frame_index];

        if (HAL_CAN_GetRxMessage(can, CAN_RX_FIFO0,
                                 &header, frame->data) != HAL_OK) {
            break;
        }

        if (header.IDE != CAN_ID_STD ||
            header.RTR != CAN_RTR_DATA ||
            header.DLC != 8U) {
            continue;  /* 不占用 buffer 槽位，继续取下一帧 */
        }

        frame->id = header.StdId;
        frame->dlc = (uint8_t)header.DLC;

        if (header.StdId == 0x1A0U) {
            finalizeBuffer(write->current_buffer);
        } else {
            updateFrame(write);
        }
    }
}

