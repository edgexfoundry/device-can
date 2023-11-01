/* CAN Device Service based on SOCKETCAN
 *
 * Copyright (C) 2023 HCL Technologies Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This device service provides a SOCKETCAN implementation of
 * CAN protocol interface.
 *
 * CONTRIBUTORS              COMPANY
===============================================================
 1. Sathya Durai           HCL Technologies Ltd (EPL Team)
 2. Vijay Annamalaisamy    HCL Technologies Ltd (EPL Team)
*/

#ifndef _DEVICE_CAN_H_
#define _DEVICE_CAN_H_ 1

/**
 * @file
 * @brief Defines common artifacts for the CAN device service.
 */

#include <devsdk/devsdk.h>
#include <edgex/devices.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  char can_interface[32];
  uint32_t filter_msg_id;
  uint32_t filter_mask;
  uint8_t  timeout;
  bool can_IsOpened;
  int sock_fd;
  pthread_mutex_t mutex; //for interface specific synchronization
} end_dev_params;

/**
 * device-can specific data included with service callbacks
 */
typedef struct can_driver {
  iot_logger_t *lc;
  devsdk_service_t *service;
} can_driver;

int OpenCan(void *impl, const devsdk_device_t *device);

extern can_driver *impl;
#ifdef __cplusplus
}
#endif

#endif
