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

can_driver *impl;

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

int OpenCan(void *impl, const devsdk_device_t *device)
{
	struct sockaddr_can addr;
	struct ifreq ifr;
	struct timeval tv;
	struct can_filter rfilter[1];
	int iret = -1;
	can_driver *driver = (can_driver *)impl;
	end_dev_params *end_dev_params_ptr = (end_dev_params *)device->address;

	iot_log_debug(driver->lc, "can open called..\n");

	// Open the SOCKETCAN
	if ((end_dev_params_ptr->sock_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
	{
		perror("Socket");
		return iret;
	}

	strcpy(ifr.ifr_name, end_dev_params_ptr->can_interface);
	ioctl(end_dev_params_ptr->sock_fd, SIOCGIFINDEX, &ifr);

	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	// Bind the socket
	if (bind(end_dev_params_ptr->sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind");
		return iret;
	}

	tv.tv_sec = end_dev_params_ptr->timeout;
	tv.tv_usec = 0;

	if (setsockopt(end_dev_params_ptr->sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
	{
		perror("setsockopt");
		return iret;
	}

	rfilter[0].can_id   = end_dev_params_ptr->filter_msg_id;
	rfilter[0].can_mask = end_dev_params_ptr->filter_mask; //CAN_SFF_MASK;

	// Set the CAN msg ID filter
	if (setsockopt(end_dev_params_ptr->sock_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0)
	{
		perror("setsockopt");
		return iret;
	}

	iret = 0;

	return iret;

}

/* Init callback; reads in config values to device driver */
static bool can_init(void *impl, struct iot_logger_t *lc,
		const iot_data_t *config) {
	can_driver *driver = (can_driver *)impl;
	bool result = false;

	iot_log_debug(driver->lc, "can_init..\n");

	driver->lc = lc;

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
	bool successful_get_request = false;
	uint32_t i = 0, j = 0, k = 0;
	struct can_frame frame;
	uint32_t data[128] = {0};
	int nbytes = 0;
	int iret = -1;

	end_dev_params *end_dev_params_ptr = (end_dev_params *)device->address;

	pthread_mutex_lock(&end_dev_params_ptr->mutex);

	if(!end_dev_params_ptr->can_IsOpened)
	{
		iret = OpenCan(impl, device); 
		if(iret != 0)
		{
			pthread_mutex_unlock(&end_dev_params_ptr->mutex);
			return successful_get_request;
		}

		end_dev_params_ptr->can_IsOpened = true;
	} else
	{
		iot_log_debug(driver->lc, "CAN already opened\n");
	}


	// Parse and read the values
	for (i = 0; i < nreadings; i++) {
		iot_log_debug(driver->lc, "CAN:Triggering Get events resource name=%s\n",
				requests[i].resource->name);
		iot_log_debug(driver->lc, "CAN:Triggering Get events req type=%d\n",
				requests[i].resource->type);

		// Read the socket interface for the CAN frame
		nbytes = read(end_dev_params_ptr->sock_fd, &frame, sizeof(struct can_frame));
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

	pthread_mutex_unlock(&end_dev_params_ptr->mutex);

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
	int iret = -1;
	bool successful_put_request = false;
	uint8_t i = 0, k = 0; 

	end_dev_params *end_dev_params_ptr = (end_dev_params *)device->address;

	pthread_mutex_lock(&end_dev_params_ptr->mutex);

	if(!end_dev_params_ptr->can_IsOpened)
	{
		iret = OpenCan(impl, device);
		if(iret != 0)
		{
			pthread_mutex_unlock(&end_dev_params_ptr->mutex);
			return successful_put_request;
		}

		end_dev_params_ptr->can_IsOpened = true;
	} else
	{
		iot_log_debug(driver->lc, "CAN already opened..\n");
	}

	for (k = 0; k < nvalues; k++)
	{
		// Set the iterator to the first array
		iot_data_array_iter_t iter;
		iot_data_array_iter (values[k], &iter);

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
		if (write(end_dev_params_ptr->sock_fd, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
			perror("Write");
			pthread_mutex_unlock(&end_dev_params_ptr->mutex);
			return successful_put_request;
		}
	}

	successful_put_request = true;

	pthread_mutex_unlock(&end_dev_params_ptr->mutex);

	return successful_put_request;
}

static void can_stop(void *impl, bool force) {
}

static devsdk_address_t can_create_address(void *impl,
		const devsdk_protocols *protocols,
		iot_data_t **exception) {
	can_driver *driver = (can_driver *)impl;
	char *end = NULL;
	errno = 0;
	iot_log_debug (driver->lc, "can_create_address called..!!\n");

	end_dev_params *end_dev_params_ptr = (end_dev_params *)malloc(sizeof(end_dev_params));
	if (end_dev_params_ptr == NULL)
        {
                *exception = iot_data_alloc_string ("end_dev_params_ptr - malloc failed", IOT_DATA_REF);
                return NULL;
        }

	memset(end_dev_params_ptr, 0x00, sizeof(end_dev_params));

	// Initialize CAN interface specific mutex
	pthread_mutex_init(&end_dev_params_ptr->mutex, NULL);

	const iot_data_t *props = devsdk_protocols_properties (protocols, "CAN");
	if (props == NULL)
	{
		*exception = iot_data_alloc_string ("CAN field not present", IOT_DATA_REF);
		return NULL;
	}

	// Get CAN Interface
	const char *params_ptr = iot_data_string_map_get_string(props, "DevInterface");
	if (params_ptr == NULL) {
		*exception = iot_data_alloc_string("DevInterface in device address missing",
				IOT_DATA_REF);
		return false;
	}
	strcpy(end_dev_params_ptr->can_interface, params_ptr);

	// Get CAN Filter Msg ID
	params_ptr = iot_data_string_map_get_string(props, "FilterMsgId");
	if (params_ptr == NULL)
	{
		*exception = iot_data_alloc_string("FilterMsgId in device address missing",
				IOT_DATA_REF);
		return false;
	}

	end_dev_params_ptr->filter_msg_id = strtoul(params_ptr, &end, 0);
	if(end_dev_params_ptr->filter_msg_id == 0 && (errno != 0))
	{
		*exception = iot_data_alloc_string("FilterMsgId access error",
				IOT_DATA_REF);
		return false;
	}

	// Get Filter Mask
	params_ptr = iot_data_string_map_get_string(props, "FilterMask");
	if (params_ptr == NULL)
	{
		*exception = iot_data_alloc_string("FilterMask in device address missing",
				IOT_DATA_REF);
		return false;
	}

	end_dev_params_ptr->filter_mask = strtoul(params_ptr, &end, 0);
	if(end_dev_params_ptr->filter_mask == 0 && (errno != 0))
	{
		*exception = iot_data_alloc_string("FilterMask access error",
				IOT_DATA_REF);
		return false;
	}

	// Get TimeOut
	params_ptr = iot_data_string_map_get_string(props, "TimeOut");
	if (params_ptr == NULL)
	{
		*exception = iot_data_alloc_string("TimeOut in device address missing",
				IOT_DATA_REF);
		return false;
	}

	end_dev_params_ptr->timeout = strtoul(params_ptr, &end, 0);
	if(end_dev_params_ptr->timeout == 0 && (errno != 0))
	{
		*exception = iot_data_alloc_string("TimeOut access error",
				IOT_DATA_REF);
		return false;
	}

	iot_log_debug (driver->lc, "CAN Interface %s\n", end_dev_params_ptr->can_interface);
	iot_log_debug (driver->lc, "Filter Msg Id %d\n", end_dev_params_ptr->filter_msg_id);
	iot_log_debug (driver->lc, "Filter Mask%d\n", end_dev_params_ptr->filter_mask);
	iot_log_debug (driver->lc, "Timeout %d\n", end_dev_params_ptr->timeout);

	return (devsdk_address_t)end_dev_params_ptr;
}

static void can_free_address(void *impl, devsdk_address_t address) {
	if (address != NULL) {
		end_dev_params *end_dev_params_ptr = (end_dev_params *)address;
		pthread_mutex_destroy(&end_dev_params_ptr->mutex);
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

	iot_data_t *driver_map = iot_data_alloc_map(IOT_DATA_STRING);

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
