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


#include <devices/os_debug.h>
#include <palacios/vmm.h>

#define BUF_SIZE 1024

#define DEBUG_PORT1 0xc0c0


struct debug_state {
  char debug_buf[BUF_SIZE];
  uint_t debug_offset;

};


static int handle_gen_write(ushort_t port, void * src, uint_t length, struct vm_device * dev) {
  struct debug_state * state = (struct debug_state *)dev->private_data;

  state->debug_buf[state->debug_offset++] = *(char*)src;

  if ((*(char*)src == 0xa) ||  (state->debug_offset == (BUF_SIZE - 1))) {
    PrintDebug("VM_CONSOLE>%s", state->debug_buf);
    memset(state->debug_buf, 0, BUF_SIZE);
    state->debug_offset = 0;
  }

  return length;
}


static int debug_init(struct vm_device * dev) {
  struct debug_state * state = (struct debug_state *)dev->private_data;

  v3_dev_hook_io(dev, DEBUG_PORT1,  NULL, &handle_gen_write);

  state->debug_offset = 0;
  memset(state->debug_buf, 0, BUF_SIZE);
  
  return 0;
}

static int debug_deinit(struct vm_device * dev) {
  v3_dev_unhook_io(dev, DEBUG_PORT1);


  return 0;
};




static struct vm_device_ops dev_ops = {
  .init = debug_init,
  .deinit = debug_deinit,
  .reset = NULL,
  .start = NULL,
  .stop = NULL,
};


struct vm_device * v3_create_os_debug() {
  struct debug_state * state = NULL;

  state = (struct debug_state *)V3_Malloc(sizeof(struct debug_state));

  PrintDebug("Creating OS Debug Device\n");
  struct vm_device * device = v3_create_device("OS Debug", &dev_ops, state);



  return device;
}