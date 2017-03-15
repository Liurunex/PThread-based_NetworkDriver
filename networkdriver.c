/* 
 * Author: Zhixin Liu
 * Duck ID: zhixinl
 * UO ID: 951452405
 * Authorship Statement:
 * This is my own work.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "BoundedBuffer.h"
#include "freepacketdescriptorstore__full.h"
#include "freepacketdescriptorstore.h"
#include "networkdriver.h"
#include "networkdevice.h"
#include "pid.h"
#include "destination.h"
#include "packetdescriptor.h"
#include "packetdescriptorcreator.h"
#include "diagnostics.h"

#define PD_MAX 10
#define TRY_MAX 5

FreePacketDescriptorStore *free_pd;
NetworkDevice *netDevice;
BoundedBuffer *buffer_receive [MAX_PID + 1]; 
BoundedBuffer *buffer_send;
BoundedBuffer *buffer_pool;

static void *t_send() {
	PacketDescriptor *pd = NULL;
	int i = 0;
	int counter = 1;

	while (1) {
		/* Blocking Function to avoid busy calls */
		pd = (PacketDescriptor *) blockingReadBB(buffer_send);

		/* Try to send packet within TRY_MAX times */
		counter = 1;
		for (i = 0; i < TRY_MAX; i++) {
			if (send_packet(netDevice, pd)) {
				DIAGNOSTICS("[My_Driver> Send Packet to Application Success\n"); 
				break;
			}
			else 
				counter++;
		}

		if (counter == TRY_MAX)
			 DIAGNOSTICS("[My_Device> Send to NetworkDevice Failed After Tring %d Times\n", counter); 
		
		/* Reset the Packet Descriptor */
		init_packet_descriptor(pd);
		/* Return Packet Descriptor to pool, if failed, return it to free store */
		if (!nonblockingWriteBB(buffer_pool, pd)) {
			if(!nonblocking_put_pd(free_pd, pd))
				DIAGNOSTICS("[My_Error> Non-blocking Put PacketDescriptor Failed");
		}
		
	}
	return NULL;
}

static void *t_receive() {
	
	PacketDescriptor *pd_current = NULL;
	PacketDescriptor *pd_backup = NULL;
	PID pid;
	int can_get_backup = 1;

	/* Get first PacketDescriptor to start */
	blocking_get_pd(free_pd, &pd_current);
	/* Initial first PacketDescriptor */
	init_packet_descriptor(pd_current);
	register_receiving_packetdescriptor(netDevice, pd_current);

	while (1) {

		/* Block thread to waiting for incoming data packet */
		await_incoming_packet(netDevice);

		/* Try to get a new back-up PacketDescriptor from pool or free PacketDescriptor store */
		if (!nonblockingReadBB(buffer_pool, (void **) &pd_backup)) {
			if (!nonblocking_get_pd(free_pd, &pd_backup)) {
				can_get_backup = 0;
				DIAGNOSTICS("[My_Error> cannot get new PacketDescriptor \n");
			}
			else 
				can_get_backup = 1;
		}
		else 
			can_get_backup = 1;

		if (can_get_backup)  {
			/* If the back-up PacketDescriptor is ready, register the back-up PacketDescriptor */
			/* to network device for next incoming packet */
			init_packet_descriptor(pd_backup);
			register_receiving_packetdescriptor(netDevice, pd_backup);

			/* Try to write into receive buffer */
			pid = packet_descriptor_get_pid(pd_current);
			if (!nonblockingWriteBB(buffer_receive[pid], pd_current)) {
				/* Failed to write into buffer cause to return current PacketDescriptor */
				/* to pool or free store, and use the back-up PacketDescriptor as next */	
				init_packet_descriptor(pd_current);
				if (!nonblockingWriteBB(buffer_pool, pd_current)) {
					if(!nonblocking_put_pd(free_pd, pd_current))
						DIAGNOSTICS("[My_Error> Non-blocking Put PacketDescriptor Failed");
				}
			}
			
			/* No matter whether the operation of writing into buffer or returning PacketDescriptor */
			/* success or fail, make the pointer of current PacketDescriptor points to */ 
			/* the back-up one, and ask for a new back-up PacketDescriptor */
			pd_current = pd_backup;
			pd_backup = NULL;
		}
		else {
			/* If no back-up PacketDescriptor is ready, register the current PacketDescriptor */
			/* to network device for incoming packet, and ask for a new back-up PacketDescriptor */
			init_packet_descriptor(pd_current);
			register_receiving_packetdescriptor(netDevice, pd_current);
		}
	}

	return NULL;
}

/* definition[s] of function[s] required for your thread[s] */
void init_network_driver(NetworkDevice               *nd, 
                         void                        *mem_start, 
                         unsigned long               mem_length,
                         FreePacketDescriptorStore **fpds_ptr) {
/* create Free Packet Descriptor Store */
/* load FPDS with packet descriptors constructed from mem_start/mem_length */
	int i = 0;
	pthread_t thread_send;
	pthread_t thread_recieve;

	netDevice = nd;
	*fpds_ptr = create_fpds();
	create_free_packet_descriptors(*fpds_ptr, mem_start, mem_length);

	/* create any buffers required by your thread[s] */
	buffer_send = createBB(PD_MAX);
	buffer_pool = createBB(PD_MAX);
	for (i = 0; i < (MAX_PID+1); i++)
		buffer_receive[i] = createBB(PD_MAX);

	/* create any threads you require for your implementation */
	if (pthread_create(&thread_send, NULL, t_send, NULL) != 0)
		printf("[My_Error> Thread Send Fail\n");
	if (pthread_create(&thread_recieve, NULL, t_receive, NULL) != 0)
		printf("[My_Error> Thread Receive Fail\n");

	/* return the FPDS to the code that called you */
	free_pd = *fpds_ptr;
}

void blocking_send_packet(PacketDescriptor *pd) { 
	blockingWriteBB(buffer_send, pd);
}

int nonblocking_send_packet(PacketDescriptor *pd) {
	 return nonblockingWriteBB(buffer_send, pd);
}

void blocking_get_packet(PacketDescriptor **pd, PID pid) { 
	*pd = (PacketDescriptor *) blockingReadBB(buffer_receive[pid]);
}

int nonblocking_get_packet(PacketDescriptor **pd, PID pid) {
	return nonblockingReadBB(buffer_receive[pid], (void **) pd);
}