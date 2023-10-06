# device-can

Device service for CAN protocol written in C using socketcan. This service is developed and contributed by HCL Technologies(EPL Team).

## CAN Overview

### Development
- Developed by Robert Bosh
- Standardized by ISO & Society of Automotive Engineering
- Most widely used protocol in Automotive domain
- Other domains include medical, industrial, agricultural, aero, defense

### Protocol Overview
- Multi-master bus protocol. No centralized master node
- Number of nodes is not limited
- Standard CAN speed is 1 Mbps and popular extension is CAN FD which has a speed of 15 Mbps
- Easily connect and disconnect nodes
- Broadcast and multicast capability
- Devices have Message Filter based on ID
- Lower the message ID, higher is the priority

### CAN Support in Linux
- Linux has support for CAN drivers
- Character device-based drivers – can4linux
- Network socket-based drivers - socketcan
- Each supports different controllers and protocols
- This link(https://elinux.org/CAN_Bus) shows the different controllers and protocols supported by can4linux & socketcan

## CAN Device Service Overview
- Socketcan is used
- The link - https://docs.kernel.org/networking/can.html#motivation-why-using-the-socket-api mentions why socketcan is preferred over character device-based driver
- C – device-sdk-c is used.
- CAN interface name and message filter can be configured in service configuration file
- This version supports the basic can read and write functionality
- struct can_frame is the data format of socketcan.
- Data format used by the can device service to read and write data is - uint32 array - [msgId, dlc, data1, data2, data3,...]
- All the CAN devices in a CAN bus are masters & every device bothers only about the Msg ID it can send/receive. So in devices.json,
no need to represent every can device in the can bus. Using a single profile and single device, with proper filter & mask, all CAN communication is possible.
- Currently it supports one set of filter and mask. Need to check how this can be scaled dynamically.
- This device service was tested using Raspberry Pi4 with MCP2515 CAN controller and loopback enabled. Testing was also done with virtual can
- The service runs on port 59999

## Build Instructions

### Building the CAN Device Service
Before building the CAN device service, please ensure
that you have the EdgeX C-SDK installed and make sure that
the current directory is the /device-can device service directory
. To build the CAN device service, enter
the command below into the command line to run the build
script.

        make build

## Running the Service

Before running the service, ensure to configure CAN interfaces as below:

### Install SocketCAN

- sudo apt-get install can-utils

### Virtual CAN

- modprobe vcan 
- sudo ip link add dev vcan0 type vcan 
- sudo ip link set up vcan0 

### MCP2515 CAN Controller

- modprobe can
- ip link set can0 type can bitrate 125000 triple-sampling on restart-ms 100 
- sudo ip link set can0 type can loopback on 
- sudo ip link set up can0 

### CAN Traffic

After the interface is enabled, CAN traffic can be sniffed through the below command:

candump -tz can0 (or vcan0)

### Run the Service

Ensure other EdgeX services are already running.

Once the build is completed and interfaces are enabled, run the executable with the below commands

export EDGEX_SECURITY_SECRET_STORE=false
./build/release/device-can

### Test with Core Commands

#### PUT Command

Execute the below command in a separate terminal

curl -X 'PUT' 'http://localhost:59882/api/v3/device/name/candev/canmsg' -d '{"canmsg":"[123, 3, 108, 109, 113]"}' | json_pp

(OR)

cansend can0 07B#1122334455667788

The above command assumes, msg filter is set as 123(07B).

#### GET Command

Execute the below command in a separate terminal

curl -X 'GET' 'http://localhost:59882/api/v3/device/name/candev/canmsg' | json_pp

It reads the sent frame with loopback mode ON. Note that loopback does not work with virtual can

If autoevents are enabled, it keeps on reading the can frames at the specified interval

