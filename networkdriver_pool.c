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

#define PD_MAX 10
#define TRY_MAX 5
/* any global variables required for use by your threads and your driver routines */
FreePacketDescriptorStore *free_pd;
NetworkDevice *netDevice;
BoundedBuffer *bbuffer_receive [MAX_PID + 1]; 
BoundedBuffer *bbuffer_send;
BoundedBuffer *receive_pool;

void *send() {
	PacketDescriptor *pd;
	int i = 0;
	int counter = 1;

	while (1) {
		/* Blocking Function to avoid busy calls */
		pd = (PacketDescriptor *) blockingReadBB(bbuffer_send);

		counter = 1;
		for (i = 0; i < TRY_MAX; i++) {
			if (send_packet(netDevice, pd)) {
				printf("[Driver> Send Packet to Application Success\n");
				break;
			}
			else 
				counter++;
		}

		if (counter == TRY_MAX){
			printf("[Error> Send to NetworkDevice Failed After Tring %d Times\n", counter);
		}
		
		/* Reset the Packet Descriptor */
		init_packet_descriptor(pd);
		/* Return Packet Descriptor */
		if (!nonblockingWriteBB(receive_pool, pd))
			blocking_put_pd(free_pd, pd);
	}
	return NULL;
}

void *receive() {
	
	PacketDescriptor *pd;
	PID pid;
	int need_new = 0;
	int cant_get_backup = 0;
	blocking_get_pd(free_pd, &pd);
	if(!nonblockingWriteBB(receive_pool, pd))
		printf("[Error > Initial Write into Pool Failed\n");
	
	while (1) {
		if (need_new) {
			if (!nonblocking_get_pd(free_pd, &pd)) {
				cant_get_backup = 1;
				printf("[Error> Use Previous PacketDescriptor since No Enough in FreePacketDescriptorStore\n");
			}
			else 
				cant_get_backup = 0;
		}

		init_packet_descriptor(pd_current);
		register_receiving_packetdescriptor(netDevice, pd_current);
		await_incoming_packet(netDevice);

		/* Try to Write into Receive Buffer */
		pid = packet_descriptor_get_pid(pd_current);
		if (!nonblockingWriteBB(bbuffer_receive[pid], pd_current)) 
			need_new = 0;
		else {
			if (!cant_get_backup) 
				pd_current = pd_backup;
			need_new = 1;
		}
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
	bbuffer_send = createBB(PD_MAX);
	receive_pool = createBB(PD_MAX);
	for (i = 0; i < (MAX_PID+1); i++)
		bbuffer_receive[i] = createBB(PD_MAX);

	/* create any threads you require for your implementation */
	if (pthread_create(&thread_send, NULL, send, NULL) != 0)
		printf("[Error> Thread Send Fail\n");
	if (pthread_create(&thread_recieve, NULL, receive, NULL) != 0)
		printf("[Error> Thread Receive Fail\n");

	/* return the FPDS to the code that called you */
	free_pd = *fpds_ptr;
}

void blocking_send_packet(PacketDescriptor *pd) { 
	blockingWriteBB(bbuffer_send, pd);
}

int nonblocking_send_packet(PacketDescriptor *pd) {
	 return nonblockingWriteBB(bbuffer_send, pd);
}

void blocking_get_packet(PacketDescriptor **pd, PID pid) { 
	*pd = (PacketDescriptor *) blockingReadBB(bbuffer_receive[pid]);
}

int nonblocking_get_packet(PacketDescriptor **pd, PID pid) {
	return nonblockingReadBB(bbuffer_receive[pid], (void **) pd);
}