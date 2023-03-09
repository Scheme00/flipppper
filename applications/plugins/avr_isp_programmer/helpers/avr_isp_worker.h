#pragma once

#include <furi_hal.h>
#include "../avr_isp_app_i.h"

typedef struct AvrIspWorker AvrIspWorker;

typedef struct {
    uint8_t vcp_ch;
    // uint8_t uart_ch;
    // uint8_t flow_pins;
    // uint8_t baudrate_mode;
    uint32_t baudrate;
} AvrIspWorkerUsbConfig;

// typedef void (*AvrIspWorkerCallback)(
//     void* context,
//     uint32_t frequency,
//     float rssi,
//     bool signal);

/** Allocate AvrIspWorker
 * 
 * @param context AvrIsp* context
 * @return AvrIspWorker* 
 */
AvrIspWorker* avr_isp_worker_alloc(void* context);

/** Free AvrIspWorker
 * 
 * @param instance AvrIspWorker instance
 */
void avr_isp_worker_free(AvrIspWorker* instance);

// /** Callback AvrIspWorker
//  *
//  * @param instance AvrIspWorker instance
//  * @param callback AvrIspWorkerOverrunCallback callback
//  * @param context
//  */
// void avr_isp_worker_set_pair_callback(
//     AvrIspWorker* instance,
//     AvrIspWorkerCallback callback,
//     void* context);

/** Start AvrIspWorker
 * 
 * @param instance AvrIspWorker instance
 */
void avr_isp_worker_start(AvrIspWorker* instance);

/** Stop AvrIspWorker
 * 
 * @param instance AvrIspWorker instance
 */
void avr_isp_worker_stop(AvrIspWorker* instance);

/** Check if worker is running
 * @param instance AvrIspWorker instance
 * @return bool - true if running
 */
bool avr_isp_worker_is_running(AvrIspWorker* instance);
