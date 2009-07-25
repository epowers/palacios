/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu>
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __DEVICES_LNX_VIRTIO_H__
#define __DEVICES_LNX_VIRTIO_H__

#ifdef __V3VEE__



/* PCI Vendor IDs (from Qemu) */
#define VIRTIO_VENDOR_ID    0x1af4 // Redhat/Qumranet
#define VIRTIO_SUBVENDOR_ID 0x1af4 // Redhat/Qumranet
#define VIRTIO_SUBDEVICE_ID 0x1100 // Qemu

// PCI Device IDs
#define VIRTIO_NET_DEV_ID         0x1000
#define VIRTIO_BLOCK_DEV_ID       0x1001
#define VIRTIO_BALLOON_DEV_ID     0x1002
#define VIRTIO_CONSOLE_DEV_ID     0x1003

#define VIRTIO_BLOCK_SUBDEVICE_ID 2

/* The virtio configuration space is a hybrid io/memory mapped model 
 * All IO is done via IO port accesses
 * The IO ports access fields in a virtio data structure, and the base io port 
 *    coincides with the base address of the in memory structure
 * There is a standard virtio structure of 20 bytes, followed by a 
 *    device specific structure of n bytes.
 * 
 */
struct virtio_config {
    uint32_t host_features;
    uint32_t guest_features;
    uint32_t vring_page_num;
    uint16_t vring_ring_size;
    uint16_t vring_queue_selector;
    uint16_t vring_queue_notifier;
    uint8_t status;
    uint8_t pci_isr;
} __attribute__((packed));






#endif

#endif
