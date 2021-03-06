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

#include <palacios/vmm_io.h>
#include <palacios/vmm_string.h>
#include <palacios/vmm.h>




#ifndef DEBUG_IO
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


static int default_write(ushort_t port, void *src, uint_t length, void * priv_data);
static int default_read(ushort_t port, void * dst, uint_t length, void * priv_data);


void v3_init_vmm_io_map(struct guest_info * info) {
  struct vmm_io_map * io_map = &(info->io_map);
  io_map->num_ports = 0;
  io_map->head = NULL;
}





static int add_io_hook(struct vmm_io_map * io_map, struct vmm_io_hook * io_hook) {

  if (!(io_map->head)) {
    io_map->head = io_hook;
    io_map->num_ports = 1;
    return 0;
  } else if (io_map->head->port > io_hook->port) {
    io_hook->next = io_map->head;

    io_map->head->prev = io_hook;
    io_map->head = io_hook;
    io_map->num_ports++;

    return 0;
  } else {
    struct vmm_io_hook * tmp_hook = io_map->head;
    
    while ((tmp_hook->next)  && 
	   (tmp_hook->next->port <= io_hook->port)) {
	tmp_hook = tmp_hook->next;
    }
    
    if (tmp_hook->port == io_hook->port) {
      //tmp_hook->read = io_hook->read;
      //tmp_hook->write = io_hook->write;
      //V3_Free(io_hook);
      return -1;
    } else {
      io_hook->prev = tmp_hook;
      io_hook->next = tmp_hook->next;

      if (tmp_hook->next) {
	tmp_hook->next->prev = io_hook;
      }

      tmp_hook->next = io_hook;

      io_map->num_ports++;
      return 0;
    }
  }
  return -1;
}

static int remove_io_hook(struct vmm_io_map * io_map, struct vmm_io_hook * io_hook) {
  if (io_map->head == io_hook) {
    io_map->head = io_hook->next;
  } else if (io_hook->prev) {
    io_hook->prev->next = io_hook->next;
  } else {
    return -1;
    // data corruption failure
  }
  
  if (io_hook->next) {
    io_hook->next->prev = io_hook->prev;
  }

  io_map->num_ports--;

  return 0;
}





int v3_hook_io_port(struct guest_info * info, uint_t port, 
		    int (*read)(ushort_t port, void * dst, uint_t length, void * priv_data),
		    int (*write)(ushort_t port, void * src, uint_t length, void * priv_data), 
		    void * priv_data) {
  struct vmm_io_map * io_map = &(info->io_map);
  struct vmm_io_hook * io_hook = (struct vmm_io_hook *)V3_Malloc(sizeof(struct vmm_io_hook));

  io_hook->port = port;

  if (!read) {
    io_hook->read = &default_read;
  } else {
    io_hook->read = read;
  }

  if (!write) {
    io_hook->write = &default_write;
  } else {
    io_hook->write = write;
  }

  io_hook->next = NULL;
  io_hook->prev = NULL;

  io_hook->priv_data = priv_data;

  if (add_io_hook(io_map, io_hook) != 0) {
    V3_Free(io_hook);
    return -1;
  }

  return 0;
}

int v3_unhook_io_port(struct guest_info * info, uint_t port) {
  struct vmm_io_map * io_map = &(info->io_map);
  struct vmm_io_hook * hook = v3_get_io_hook(io_map, port);

  if (hook == NULL) {
    return -1;
  }

  remove_io_hook(io_map, hook);
  return 0;
}


struct vmm_io_hook * v3_get_io_hook(struct vmm_io_map * io_map, uint_t port) {
  struct vmm_io_hook * tmp_hook;
  FOREACH_IO_HOOK(*io_map, tmp_hook) {
    if (tmp_hook->port == port) {
      return tmp_hook;
    }
  }
  return NULL;
}



void v3_print_io_map(struct vmm_io_map * io_map) {
  struct vmm_io_hook * iter = io_map->head;

  PrintDebug("VMM IO Map (Entries=%d)\n", io_map->num_ports);

  while (iter) {
    PrintDebug("IO Port: %hu (Read=%p) (Write=%p)\n", 
	       iter->port, 
	       (void *)(iter->read), (void *)(iter->write));
  }
}



/*
 * Write a byte to an I/O port.
 */
void v3_outb(ushort_t port, uchar_t value) {
    __asm__ __volatile__ (
	"outb %b0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a byte from an I/O port.
 */
uchar_t v3_inb(ushort_t port) {
    uchar_t value;

    __asm__ __volatile__ (
	"inb %w1, %b0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}

/*
 * Write a word to an I/O port.
 */
void v3_outw(ushort_t port, ushort_t value) {
    __asm__ __volatile__ (
	"outw %w0, %w1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a word from an I/O port.
 */
ushort_t v3_inw(ushort_t port) {
    ushort_t value;

    __asm__ __volatile__ (
	"inw %w1, %w0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}

/*
 * Write a double word to an I/O port.
 */
void v3_outdw(ushort_t port, uint_t value) {
    __asm__ __volatile__ (
	"outl %0, %1"
	:
	: "a" (value), "Nd" (port)
    );
}

/*
 * Read a double word from an I/O port.
 */
uint_t v3_indw(ushort_t port) {
    uint_t value;

    __asm__ __volatile__ (
	"inl %1, %0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
}




/* FIX ME */
static int default_write(ushort_t port, void *src, uint_t length, void * priv_data) {
  /*
    
  if (length == 1) {
  __asm__ __volatile__ (
  "outb %b0, %w1"
  :
  : "a" (*dst), "Nd" (port)
  );
  } else if (length == 2) {
  __asm__ __volatile__ (
  "outw %b0, %w1"
  :
  : "a" (*dst), "Nd" (port)
  );
  } else if (length == 4) {
  __asm__ __volatile__ (
  "outw %b0, %w1"
  :
  : "a" (*dst), "Nd" (port)
  );
  }
  */
  return 0;
}

static int default_read(ushort_t port, void * dst, uint_t length, void * priv_data) {

  /*    
	uchar_t value;

    __asm__ __volatile__ (
	"inb %w1, %b0"
	: "=a" (value)
	: "Nd" (port)
    );

    return value;
  */

  return 0;
}
