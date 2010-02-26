/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2009, Lei Xia <lxia@northwestern.edu> 
 * Copyright (c) 2009, Yuan Tang <ytang@northwestern.edu>  
 * Copyright (c) 2009, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Lei Xia <lxia@northwestern.edu>
 *	   Yuan Tang <ytang@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */
 
#include <palacios/vmm_vnet.h>
#include <palacios/vm_guest_mem.h>
#include <palacios/vmm_lock.h>
#include <palacios/vmm_queue.h>
#include <palacios/vmm_sprintf.h>

#ifndef CONFIG_DEBUG_VNET
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif



struct eth_hdr {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t type; // indicates layer 3 protocol type
} __attribute__((packed));





struct vnet_dev {

    uint8_t mac_addr[6];
    struct v3_vm_info * vm;
    
    int (*input)(struct v3_vm_info * vm, struct v3_vnet_pkt * pkt,  void * private_data);
    void * private_data;
    
    int dev_id;
    struct list_head node;
} __attribute__((packed));





struct vnet_route_info {
    struct v3_vnet_route route_def;

    struct vnet_dev * dst_dev;
    struct vnet_dev * src_dev;

    struct list_head node;
    struct list_head match_node; // used for route matching
};




struct route_list {
    uint8_t hash_buf[VNET_HASH_SIZE];

    uint32_t num_routes;
    struct vnet_route_info * routes[0];
} __attribute__((packed));



static struct {
    struct list_head routes;
    struct list_head devs;

    int num_routes;
    int num_devs;

    v3_lock_t lock;

    struct gen_queue * inpkt_q;
    struct hashtable * route_cache;

} vnet_state;




#ifdef CONFIG_DEBUG_VNET
static inline void mac_to_string(char mac[6], char * buf) {
    snprintf(buf, 20, "%02x:%02x:%02x:%02x:%02x:%02x", 
	     mac[0], mac[1], mac[2],
	     mac[3], mac[4], mac[5]);
}

#endif


/* 
 * A VNET packet is a packed struct with the hashed fields grouped together.
 * This means we can generate the hash from an offset into the pkt struct
 */
static inline uint_t hash_fn(addr_t hdr_ptr) {    
    uint8_t * hdr_buf = (uint8_t *)&(hdr_ptr);
    
    return v3_hash_buffer(hdr_buf, VNET_HASH_SIZE);
}

static inline int hash_eq(addr_t key1, addr_t key2) {
    return (memcmp((uint8_t *)key1, (uint8_t *)key2, VNET_HASH_SIZE) == 0);
}


static int add_route_to_cache(struct v3_vnet_pkt * pkt, struct route_list * routes) {
    memcpy(routes->hash_buf, pkt->hash_buf, VNET_HASH_SIZE);    

    if (v3_htable_insert(vnet_state.route_cache, (addr_t)routes->hash_buf, (addr_t)routes) == 0) {
	PrintError("Vnet: Failed to insert new route entry to the cache\n");
	return -1;
    }
    
    return 0;
}

static int clear_hash_cache() {

    v3_free_htable(vnet_state.route_cache, 1, 1);
    vnet_state.route_cache = v3_create_htable(0, &hash_fn, &hash_eq);

    return 0;
}

static int look_into_cache(struct v3_vnet_pkt * pkt, struct route_list ** routes) {
    
    *routes = (struct route_list *)v3_htable_search(vnet_state.route_cache, (addr_t)pkt);
   
    return 0;
}


struct vnet_dev * find_dev_by_id(int idx) {
    struct vnet_dev * dev = NULL; 
    
    list_for_each_entry(dev, &(vnet_state.devs), node) {
	int dev_id = dev->dev_id;

	if (dev_id == idx)
	    return dev;
    }

    return NULL;
}

static struct vnet_dev * find_dev_by_mac(char * name) {
    struct vnet_dev * dev = NULL; 
    
    list_for_each_entry(dev, &(vnet_state.devs), node) {
	if (!memcmp(dev->mac_addr, name, 6))
	    return dev;
    }

    return NULL;
}


int v3_vnet_add_route(struct v3_vnet_route route) {
    struct vnet_route_info * new_route = NULL;
    unsigned long flags; 

    new_route = (struct vnet_route_info *)V3_Malloc(sizeof(struct vnet_route_info));
    memset(new_route, 0, sizeof(struct vnet_route_info));

    PrintDebug("Vnet: vnet_add_route_entry\n");	
    
    new_route->route_def = route;

    /* TODO: Find devices */
    if (new_route->route_def.dst_type == LINK_INTERFACE) {
	new_route->dst_dev = find_dev_by_id(new_route->route_def.dst_id);
    }

    if (new_route->route_def.src_type == LINK_INTERFACE) {
	new_route->src_dev = find_dev_by_id(new_route->route_def.src_id);
    }

    flags = v3_lock_irqsave(vnet_state.lock);
    list_add(&(new_route->node), &(vnet_state.routes));
    v3_unlock_irqrestore(vnet_state.lock, flags);
   
    clear_hash_cache();

    return 0;
}



// At the end allocate a route_list
// This list will be inserted into the cache so we don't need to free it
static struct route_list * match_route(struct v3_vnet_pkt * pkt) {
    struct vnet_route_info * route = NULL; 
    struct route_list * matches = NULL;
    int num_matches = 0;
    int max_rank = 0;
    struct list_head match_list;
    struct eth_hdr * hdr = (struct eth_hdr *)(pkt->data);
    uint8_t src_type = pkt->src_type;
    uint32_t src_link = pkt->src_id;

#ifdef CONFIG_DEBUG_VNET
    {
	char dst_str[18];
	char src_str[18];

	mac_to_string(hdr->src_mac, src_str);  
	mac_to_string(hdr->dst_mac, dst_str);
	PrintDebug("Vnet: match_route. pkt: SRC(%s), DEST(%s)\n", src_str, dst_str);
    }
#endif

    INIT_LIST_HEAD(&match_list);
    
#define UPDATE_MATCHES(rank) do {				\
	if (max_rank < (rank)) {				\
	    max_rank = (rank);					\
	    INIT_LIST_HEAD(&match_list);			\
	    							\
	    list_add(&(route->match_node), &match_list);	\
	    num_matches = 1;					\
	} else if (max_rank == (rank)) {			\
	    list_add(&(route->match_node), &match_list);	\
	    num_matches++;					\
	}							\
    } while (0)
    

    list_for_each_entry(route, &(vnet_state.routes), node) {
	struct v3_vnet_route * route_def = &(route->route_def);

	// CHECK SOURCE TYPE HERE
	if ( (route_def->src_type != LINK_ANY) && 
	     ( (route_def->src_type != src_type) || 
	       ( (route_def->src_id != src_link) &&
		 (route_def->src_id != (uint32_t)-1)))) {
	    continue;
	}


	if ((route_def->dst_mac_qual == MAC_ANY) &&
	    (route_def->src_mac_qual == MAC_ANY)) {      
	    UPDATE_MATCHES(3);
	}
	
	if (memcmp(route_def->src_mac, hdr->src_mac, 6) == 0) {
	    if (route_def->src_mac_qual != MAC_NOT) {
		if (route_def->dst_mac_qual == MAC_ANY) {
		    UPDATE_MATCHES(6);
		} else if (route_def->dst_mac_qual != MAC_NOT &&
			   memcmp(route_def->dst_mac, hdr->dst_mac, 6) == 0) {
		    UPDATE_MATCHES(8);
		}
	    }
	}
	    
	if (memcmp(route_def->dst_mac, hdr->dst_mac, 6) == 0) {
	    if (route_def->dst_mac_qual != MAC_NOT) {
		if (route_def->src_mac_qual == MAC_ANY) {
		    UPDATE_MATCHES(6);
		} else if ((route_def->src_mac_qual != MAC_NOT) && 
			   (memcmp(route_def->src_mac, hdr->src_mac, 6) == 0)) {
		    UPDATE_MATCHES(8);
		}
	    }
	}
	    
	if ((route_def->dst_mac_qual == MAC_NOT) &&
	    (memcmp(route_def->dst_mac, hdr->dst_mac, 6) != 0)) {
	    if (route_def->src_mac_qual == MAC_ANY) {
		UPDATE_MATCHES(5);
	    } else if ((route_def->src_mac_qual != MAC_NOT) && 
		       (memcmp(route_def->src_mac, hdr->src_mac, 6) == 0)) {     
		UPDATE_MATCHES(7);
	    }
	}
	
	if ((route_def->src_mac_qual == MAC_NOT) &&
	    (memcmp(route_def->src_mac, hdr->src_mac, 6) != 0)) {
	    if (route_def->dst_mac_qual == MAC_ANY) {
		UPDATE_MATCHES(5);
	    } else if ((route_def->dst_mac_qual != MAC_NOT) &&
		       (memcmp(route_def->dst_mac, hdr->dst_mac, 6) == 0)) {
		UPDATE_MATCHES(7);
	    }
	}
	
	// Default route
	if ( (memcmp(route_def->src_mac, hdr->src_mac, 6) == 0) &
	     (route_def->dst_mac_qual == MAC_NONE)) {
	    UPDATE_MATCHES(4);
	}
    }

    PrintDebug("Vnet: match_route: Matches=%d\n", num_matches);

    if (num_matches == 0) {
	return NULL;
    }

    matches = V3_Malloc(sizeof(struct route_list) + 
			(sizeof(struct vnet_route_info *) * num_matches));

    matches->num_routes = num_matches;

    {
	int i = 0;
	list_for_each_entry(route, &match_list, node) {
	    matches->routes[i++] = route;
	}
    }

    return matches;
}

static int handle_one_pkt(struct v3_vnet_pkt * pkt) {
    struct route_list * matched_routes = NULL;
    unsigned long flags;
    int i;


#ifdef CONFIG_DEBUG_VNET
   {
	struct eth_hdr * hdr = (struct eth_hdr *)(pkt->data);
	char dest_str[18];
	char src_str[18];

	mac_to_string(hdr->src_mac, src_str);  
	mac_to_string(hdr->dst_mac, dest_str);
	PrintDebug("Vnet: HandleDataOverLink. SRC(%s), DEST(%s)\n", src_str, dest_str);
   }
#endif

    flags = v3_lock_irqsave(vnet_state.lock);

    look_into_cache(pkt, &matched_routes);
	
    if (matched_routes == NULL) {  
        matched_routes = match_route(pkt);
		
      if (matched_routes) {
	    add_route_to_cache(pkt, matched_routes);
	} else {
	    PrintError("Could not find route for packet...\n");
	    v3_unlock_irqrestore(vnet_state.lock, flags);
	    return -1;
	}
    }

    v3_unlock_irqrestore(vnet_state.lock, flags);
    
    
    for (i = 0; i < matched_routes->num_routes; i++) {
	struct vnet_route_info * route = matched_routes->routes[i];

        if (route->route_def.dst_type == LINK_EDGE) {

        } else if (route->route_def.dst_type == LINK_INTERFACE) {
            if (route->dst_dev->input(route->dst_dev->vm, pkt, route->dst_dev->private_data) == -1) {
                PrintDebug("VNET: Packet not sent properly\n");
                continue;
	     }
        } else {
            PrintDebug("Vnet: Wrong Edge type\n");
            continue;
        }

        PrintDebug("Vnet: HandleDataOverLink: Forward packet according to Route\n");
    }
    
    return 0;
}

int v3_vnet_send_pkt(struct v3_vnet_pkt * pkt) {
    
    PrintDebug("In Vnet Send: pkt size: %d\n", pkt->size);
		
    if (handle_one_pkt(pkt) != -1) {
        PrintDebug("VNET: send one packet! pt length %d\n", pkt->size);  
    } else {
        PrintDebug("VNET: Fail to forward one packet, discard it!\n"); 
    }

    return 0;
}

int v3_vnet_add_dev(struct v3_vm_info *vm,uint8_t mac[6], 
		    int (*netif_input)(struct v3_vm_info * vm, struct v3_vnet_pkt * pkt, void * private_data), 
		    void * priv_data){
    struct vnet_dev * new_dev = NULL;

    new_dev = find_dev_by_mac(mac);

    PrintDebug("VNET: register device\n");

    if (new_dev) {
	PrintDebug("VNET: register device: Already has device with the same mac\n");
	return -1;
    }
    
    new_dev = (struct vnet_dev *)V3_Malloc(sizeof(struct vnet_dev)); 

    if (new_dev == NULL) {
	PrintError("VNET: Malloc fails\n");
	return -1;
    }
   
    memcpy(new_dev->mac_addr, mac, 6);
    new_dev->input = netif_input;
    new_dev->private_data = priv_data;
    new_dev->vm = vm;

    PrintDebug("VNET: register device new_dev22 %p\n", (void *)new_dev);
	
    list_add(&(new_dev->node), &(vnet_state.devs));
    vnet_state.num_devs ++;
    new_dev->dev_id = vnet_state.num_devs;

    return 0;
}

#if 0
static int v3_vnet_pkt_process() {
    struct v3_vnet_pkt * pkt = NULL;

    while ((pkt = (struct v3_vnet_pkt *)v3_dequeue(vnet_state.inpkt_q)) != NULL) {
        if (handle_one_pkt(pkt) != -1) {
            PrintDebug("VNET: vnet_check: handle one packet! pt length %d\n", (int)pkt->size);  
        } else {
            PrintDebug("VNET: vnet_check: Fail to forward one packet, discard it!\n"); 
        }
	
        V3_Free(pkt); // be careful here
    }
    
    return 0;
}
#endif

int V3_init_vnet() {
	
    INIT_LIST_HEAD(&(vnet_state.routes));
    INIT_LIST_HEAD(&(vnet_state.devs));

    vnet_state.num_devs = 0;
    vnet_state.num_routes = 0;

    PrintDebug("VNET: Links and Routes tables initiated\n");

    if (v3_lock_init(&(vnet_state.lock)) == -1){
        PrintError("VNET: Failure to init lock for routes table\n");
    }

    PrintDebug("VNET: Locks initiated\n");

    /*initial pkt receiving queue */
    vnet_state.inpkt_q = v3_create_queue();
    v3_init_queue(vnet_state.inpkt_q);
    PrintDebug("VNET: Receiving queue initiated\n");

    vnet_state.route_cache = v3_create_htable(0, &hash_fn, &hash_eq);

    if (vnet_state.route_cache == NULL) {
        PrintError("Vnet: Route Cache Init Fails\n");
        return -1;
    }

    PrintDebug("VNET: initiated\n");

    return 0;
}
