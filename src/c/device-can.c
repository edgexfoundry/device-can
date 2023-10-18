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

#include <net/if.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <signal.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <iot/data.h>
#include "device-can.h"

#include <execinfo.h>

#include "devsdk/devsdk.h"
#define ERR_CHECK(x)                                      \
	if (x.code) {                                           \
		fprintf(stderr, "Error: %d: %s\n", x.code, x.reason); \
		devsdk_service_free(service);                         \
		free(impl);                                           \
		return x.code;                                        \
	}

#define CAN_INTFC_ADDR "CanInterface"
#define FILTER_MSG_ID1 "FilterMsgId1"
#define FILTER_MASK1   "FilterMask1"
#define TIMEOUT "Timeout"

can_driver *impl;
int sock_fd;

/* variable to control the service execution */
volatile sig_atomic_t quit = 0;

/* signal handler to catch the signals */
static void handle_sig (int sig)
{
	void *array[10];
	size_t size;

	quit = 1;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);

	exit(1);
}

/* Init callback; reads in config values to device driver */
static bool can_init(void *impl, struct iot_logger_t *lc,
		const iot_data_t *config) {
	can_driver *driver = (can_driver *)impl;
	bool result = false;
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct timeval tv;
	struct can_filter rfilter[1];
	int64_t timeout = 0, filter = 0, mask = 0;

	iot_log_debug(driver->lc, "can_init..\n");

	driver->lc = lc;
	pthread_mutex_init(&driver->mutex, NULL);

	// Read the can interface name from the configuration
	const char *can_intfc = iot_data_string_map_get_string(config, CAN_INTFC_ADDR);
	if (can_intfc) 
	{
		driver->can_intfc = iot_data_alloc_string(can_intfc, IOT_DATA_COPY);
	} else 
	{
		iot_log_error(lc, "CAN Interface not in configuration");
		driver->can_intfc = NULL;
		return result;
	}

	// Open the SOCKETCAN
	if ((sock_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) 
	{
		perror("Socket");
		return result;
	}

	// Read the filter and the mask from the configuration
	// TBD - How to set multiple filters and mask
	filter = iot_data_string_map_get_i64(config, FILTER_MSG_ID1, 11); 
	iot_log_debug(driver->lc, "Val of filter is %d ",filter);

	mask = iot_data_string_map_get_i64(config, FILTER_MASK1, 11); 
	iot_log_debug(driver->lc, "Val of mask is %d ",mask);

	rfilter[0].can_id   = filter;
	rfilter[0].can_mask = mask; //CAN_SFF_MASK;

	// Get the interface index
	iot_log_debug(driver->lc, "Val of interface is %s ",can_intfc);
	strcpy(ifr.ifr_name, can_intfc );
	ioctl(sock_fd, SIOCGIFINDEX, &ifr);

	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	// Bind the socket
	if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind");
		return result;
	}

	// Read the timeout from the configuration and set that
	// in the socket
	timeout = iot_data_string_map_get_i64(config, TIMEOUT , 11);
	iot_log_debug(driver->lc, "Val of timeout is %d ",timeout);
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
	{
            perror("setsockopt");
	    return result;
	}

	// Set the CAN msg ID filter
	if (setsockopt(sock_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0)
	{
	    perror("setsockopt");
	    return result;
	}

	result = true;
	iot_log_debug(driver->lc, "can_init complete..\n");

	return result;
}

// GET Handler which gets called on autoevents and the GET request
// Fill the uint32 array from the struct can_frame
// data format - uint32 array - [msgId, dlc, data1, data2, data3,...]
static bool can_get_handler(void *impl, const devsdk_device_t *device,
		uint32_t nreadings,
		const devsdk_commandrequest *requests,
		devsdk_commandresult *readings,
		const iot_data_t *options,
		iot_data_t **exception) {
	can_driver *driver = (can_driver *)impl;
	pthread_mutex_lock(&driver->mutex);
	bool successful_get_request = false;
	uint32_t i = 0, j = 0, k = 0;
	struct can_frame frame;
	uint32_t data[128] = {0};
	int nbytes = 0;

	if (device == NULL) {
		return successful_get_request;
	}

	// Parse and read the values
	for (i = 0; i < nreadings; i++) {
		iot_log_debug(driver->lc, "CAN:Triggering Get events resource name=%s\n",
				requests[i].resource->name);
		iot_log_debug(driver->lc, "CAN:Triggering Get events req type=%d\n",
				requests[i].resource->type);

		// Read the socket interface for the CAN frame
		nbytes = read(sock_fd, &frame, sizeof(struct can_frame));
		iot_log_debug(driver->lc, "CAN:nbytes=%d\n", nbytes);
		if (nbytes <= 0) {
			perror("Read");
			successful_get_request = false;
		} else {

			iot_log_debug(driver->lc,"0x%03X [%d] ",frame.can_id, frame.can_dlc);
			data[0] = frame.can_id;
			data[1] = frame.can_dlc;

			// j is 2, as 0 & 1 are already retrieved from the can frame
			// and filled in data array
			// k is used to retrieve the can data in the can frame
			// (frame.can_dlc + 2) --> Since j is started at 2,
			// +2 is added to retrieve all the can data
			for (j = 2, k = 0; j < (frame.can_dlc + 2); j++, k++)
			{
				iot_log_debug(driver->lc, "%02X ",frame.data[k]);
				data[j] = frame.data[k];
			}

			// Allocate array and pass the received can data
			// (frame.can_dlc + 2) --> The length of the entire can frame including msg ID and data len
			readings[i].value = iot_data_alloc_array (data, (frame.can_dlc + 2),  IOT_DATA_UINT32, IOT_DATA_COPY);

			successful_get_request = true;          
		}
	}

	pthread_mutex_unlock(&driver->mutex);

	return successful_get_request;
}

// PUT Handler which gets called on initiating PUT request
// Fill the struct can_frame from the data from PUT request
// PUT request data format - uint32 array - [msgId, dlc, data1, data2, data3,...]
static bool can_put_handler(void *impl, const devsdk_device_t *device,
		uint32_t nvalues,
		const devsdk_commandrequest *requests,
		const iot_data_t *values[],
		const iot_data_t *options,
		iot_data_t **exception) {
	(void)requests;

	can_driver *driver = (can_driver *)impl;
	struct can_frame frame;

	bool successful_get_request = false;
	uint8_t i = 0; 

	pthread_mutex_lock(&driver->mutex);

	// Set the iterator to the first array
	iot_data_array_iter_t iter;
	iot_data_array_iter (values[0], &iter);

	iot_log_debug(driver->lc, "CAN: PUT on device \n");
	iot_log_debug(driver->lc, "CAN: nvalues = %d\n", nvalues);

	// Move the iterator and fill the msg ID filter
	iot_data_array_iter_next (&iter);
	frame.can_id = *((uint32_t *) iot_data_array_iter_value (&iter));
	iot_log_debug(driver->lc, "can_id: %d\n", frame.can_id);

	// Move the iterator and fill the data length code
	iot_data_array_iter_next (&iter);
	frame.can_dlc = *((uint32_t *) iot_data_array_iter_value (&iter));
	iot_log_debug(driver->lc, "can_dlc: %d\n", frame.can_dlc);
	for (i = 0; i<frame.can_dlc; i++)
	{
		// Move the iterator and fill all the can data in the frame
		iot_data_array_iter_next (&iter);
		frame.data[i] = *((uint32_t *) iot_data_array_iter_value (&iter));
		iot_log_debug(driver->lc, "CAN: data[%d] = %d\n", i, frame.data[i]);
	}

        // Write the can frame to the socket
	if (write(sock_fd, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
		perror("Write");
		return 1;
	}

	successful_get_request = true;

	pthread_mutex_unlock(&driver->mutex);

	return successful_get_request;
}

static void can_stop(void *impl, bool force) {
	can_driver *driver = (can_driver *)impl;
	pthread_mutex_destroy(&driver->mutex);
}

static devsdk_address_t can_create_address(void *impl,
		const devsdk_protocols *protocols,
		iot_data_t **exception) {
	return (devsdk_address_t)protocols;
}

static void can_free_address(void *impl, devsdk_address_t address) {
	if (address != NULL) {
		free(address);
	} 
}

static devsdk_resource_attr_t can_create_resource_attr(
		void *impl, const iot_data_t *attributes, iot_data_t **exception) {

	return (devsdk_resource_attr_t)attributes;
}

static void can_free_resource_attr(void *impl, devsdk_resource_attr_t attr) {

}

int main(int argc, char *argv[]) {
	impl = malloc(sizeof(can_driver));
	memset(impl, 0, sizeof(can_driver));
	struct sigaction sa;

	devsdk_error e;
	e.code = 0;

	/* Device Callbacks */
	devsdk_callbacks *canImpls =
		devsdk_callbacks_init(can_init, can_get_handler, can_put_handler,
				can_stop, can_create_address, can_free_address,
				can_create_resource_attr, can_free_resource_attr);

	/* Initialize a new device service */
	devsdk_service_t *service = devsdk_service_new("device-can", VERSION, impl,
			canImpls, &argc, argv, &e);
	ERR_CHECK(e);
	impl->service = service;

	int n = 1;
	while (n < argc) {
		if (strcmp(argv[n], "-h") == 0 || strcmp(argv[n], "--help") == 0) {
			printf("Options:\n");
			printf("  -h, --help\t\t\tShow this text\n");
			devsdk_usage();
			return 0;
		} else {
			printf("%s: Unrecognized option %s\n", argv[0], argv[n]);
			return 0;
		}
	}

	/* setup signal handling for input loop */
	sigemptyset (&sa.sa_mask);
	sa.sa_handler = handle_sig;
	sa.sa_flags = 0;
	sigaction (SIGINT, &sa, NULL);
	sigaction (SIGTERM, &sa, NULL);
	sigaction (SIGSEGV, &sa, NULL);

	/* Create default Driver config and start the device service */
	iot_data_t *driver_map = iot_data_alloc_map(IOT_DATA_STRING);
	iot_data_string_map_add(driver_map, CAN_INTFC_ADDR, iot_data_alloc_string("can", IOT_DATA_REF));
	iot_data_string_map_add(driver_map, FILTER_MSG_ID1, iot_data_alloc_i64(100));
	iot_data_string_map_add(driver_map, FILTER_MASK1, iot_data_alloc_i64(100));
	iot_data_string_map_add(driver_map, TIMEOUT, iot_data_alloc_i64(10));

	devsdk_service_start(service, driver_map, &e);
	ERR_CHECK(e);

	iot_log_debug(impl->lc, "CAN device service started..\n");
	while (!quit);

	devsdk_service_stop(service, true, &e);
	ERR_CHECK(e);

	devsdk_service_free(service);
	iot_data_free(driver_map);
	free(impl);
	free(canImpls);
	puts("Exiting gracefully");
	return 0;
}
