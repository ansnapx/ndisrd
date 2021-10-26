# Another Network Kernel Development SDK

# Overview

ANS consists of total 4 drivers: two drivers for NT and two drivers for Vista+. Each of these drivers exports its own interface and can have any number of clients.
First driver is **ansfltr.sys.** It is supposed to intercept network packets and send them to clients.
Second driver is **ansfw.sys** and it is a client of **ansfltr.sys**. It is supposed to perform PID detection, make additional analysis of received packets and send them to clients.

![image](https://user-images.githubusercontent.com/79788735/138902477-b94f19c4-441c-440b-a41e-3032e0dee6b5.png)

## ANSFTLR
### Description

ansfltr.sys is implemented as a NDIS intermediate driver for Windows XP and as a NDIS lightweight filter driver for Windows Vista+.
The main task of ansfltr.sys is to intercept all network packets at NDIS layer, gather each packet into a continuous ( defragmented ) memory buffer and send it to all registered clients.
In order to register callbacks client should receive exported functions which is done by sending IOCTL request to ansfltr device, after which client should call RegisterCallbacks function.

### Algorithm
1. Intercepting packet 
2. If client is not registered, the packet is forwarded to network. Returning to step 1. 
3. Allocating defragmented buffer and copying intercepted packet into the buffer. 
4. Sending the buffer to a client. 
5. If client returns NOT STATUS_SUCCESS, packet is dropped and driver returns to step 1. 
6. If client returns STATUS_SUCCESS 
    6.a  Going to step 4 - In case any other client is waiting for the packet. 
    6.b	 Forwarding the packet to network and Going to step 1. 
	

## ANSFW
### Description
ansfw.sys is a TDI driver for Windows XP and a WFP driver for Windows Vista+. At startup it connects to the ansfltr.sys as a client.
ansfw.sys monitors current connections of each process, so it can associate PID with every network packet. It is also supposed to analyze network headers, collect fragmented packets and intercept loopback packets.
In order to register callback client should import ansfw as dynamic link library and then call RegisterCallback function.
### Algorithm
1. Receiving a packet from ansfltr.sys. 
2. If client is not registered, ansflw.sys returns STATUS_SUCCESS. Returning to step 1. 
3. Analyzing packet headers. 
4. Associating PID with the packet. 
5. If packet is not fragmented - going to step 7. 
6. Adding fragmented packet to assembler. If packet is not assembled - going to step 1. 
7. Sending the packet to a client. 
8. If client returns NOT STATUS_SUCCESS, dropping the packet and going to step 1. 
9. If client returns STATUS_SUCCESS 
9.a  Going to step 7 - In case any other client is waiting for the packet. 
9.b  ansflw.sys returns STATUS_SUCCESS. Returning to step 1. 


# How2Build

Please use the WDK **6001.18002**, and set the **WLHBASE**="WDK_6001.18002_DIRS" in your environment variable.

