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
 * Authors: Jack Lange <jarusl@cs.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu> (SMP)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#include <devices/apic.h>
#include <devices/apic_regs.h>
#include <palacios/vmm.h>
#include <palacios/vmm_msr.h>
#include <palacios/vmm_sprintf.h>
#include <palacios/vm_guest.h>
#include <palacios/vmm_types.h>




#ifndef CONFIG_DEBUG_APIC
#undef PrintDebug
#define PrintDebug(fmt, args...)
#endif


#ifdef CONFIG_DEBUG_APIC
static char * shorthand_str[] = { 
    "(no shorthand)",
    "(self)",
    "(all)",
    "(all-but-me)",
};

static char * deliverymode_str[] = { 
    "(fixed)",
    "(lowest priority)",
    "(SMI)",
    "(reserved)",
    "(NMI)",
    "(INIT)",
    "(Start Up)",
    "(ExtInt)",
};
#endif

// Temporary removal of locking
#define v3_lock(p) p=p
#define v3_unlock(p) p=p


typedef enum { APIC_TMR_INT, APIC_THERM_INT, APIC_PERF_INT, 
	       APIC_LINT0_INT, APIC_LINT1_INT, APIC_ERR_INT } apic_irq_type_t;

#define APIC_FIXED_DELIVERY  0x0
#define APIC_LOWEST_DELIVERY 0x1
#define APIC_SMI_DELIVERY    0x2
#define APIC_RES1_DELIVERY   0x3
#define APIC_NMI_DELIVERY    0x4
#define APIC_INIT_DELIVERY   0x5
#define APIC_SIPI_DELIVERY   0x6
#define APIC_EXTINT_DELIVERY 0x7

#define APIC_SHORTHAND_NONE        0x0
#define APIC_SHORTHAND_SELF        0x1
#define APIC_SHORTHAND_ALL         0x2
#define APIC_SHORTHAND_ALL_BUT_ME  0x3

#define APIC_DEST_PHYSICAL    0x0
#define APIC_DEST_LOGICAL     0x1


#define BASE_ADDR_MSR     0x0000001B
#define DEFAULT_BASE_ADDR 0xfee00000

#define APIC_ID_OFFSET                    0x020
#define APIC_VERSION_OFFSET               0x030
#define TPR_OFFSET                        0x080
#define APR_OFFSET                        0x090
#define PPR_OFFSET                        0x0a0
#define EOI_OFFSET                        0x0b0
#define REMOTE_READ_OFFSET                0x0c0
#define LDR_OFFSET                        0x0d0
#define DFR_OFFSET                        0x0e0
#define SPURIOUS_INT_VEC_OFFSET           0x0f0

#define ISR_OFFSET0                       0x100   // 0x100 - 0x170
#define ISR_OFFSET1                       0x110   // 0x100 - 0x170
#define ISR_OFFSET2                       0x120   // 0x100 - 0x170
#define ISR_OFFSET3                       0x130   // 0x100 - 0x170
#define ISR_OFFSET4                       0x140   // 0x100 - 0x170
#define ISR_OFFSET5                       0x150   // 0x100 - 0x170
#define ISR_OFFSET6                       0x160   // 0x100 - 0x170
#define ISR_OFFSET7                       0x170   // 0x100 - 0x170

#define TRIG_OFFSET0                      0x180   // 0x180 - 0x1f0
#define TRIG_OFFSET1                      0x190   // 0x180 - 0x1f0
#define TRIG_OFFSET2                      0x1a0   // 0x180 - 0x1f0
#define TRIG_OFFSET3                      0x1b0   // 0x180 - 0x1f0
#define TRIG_OFFSET4                      0x1c0   // 0x180 - 0x1f0
#define TRIG_OFFSET5                      0x1d0   // 0x180 - 0x1f0
#define TRIG_OFFSET6                      0x1e0   // 0x180 - 0x1f0
#define TRIG_OFFSET7                      0x1f0   // 0x180 - 0x1f0


#define IRR_OFFSET0                       0x200   // 0x200 - 0x270
#define IRR_OFFSET1                       0x210   // 0x200 - 0x270
#define IRR_OFFSET2                       0x220   // 0x200 - 0x270
#define IRR_OFFSET3                       0x230   // 0x200 - 0x270
#define IRR_OFFSET4                       0x240   // 0x200 - 0x270
#define IRR_OFFSET5                       0x250   // 0x200 - 0x270
#define IRR_OFFSET6                       0x260   // 0x200 - 0x270
#define IRR_OFFSET7                       0x270   // 0x200 - 0x270


#define ESR_OFFSET                        0x280
#define INT_CMD_LO_OFFSET                 0x300
#define INT_CMD_HI_OFFSET                 0x310
#define TMR_LOC_VEC_TBL_OFFSET            0x320
#define THERM_LOC_VEC_TBL_OFFSET          0x330
#define PERF_CTR_LOC_VEC_TBL_OFFSET       0x340
#define LINT0_VEC_TBL_OFFSET              0x350
#define LINT1_VEC_TBL_OFFSET              0x360
#define ERR_VEC_TBL_OFFSET                0x370
#define TMR_INIT_CNT_OFFSET               0x380
#define TMR_CUR_CNT_OFFSET                0x390
#define TMR_DIV_CFG_OFFSET                0x3e0
#define EXT_APIC_FEATURE_OFFSET           0x400
#define EXT_APIC_CMD_OFFSET               0x410
#define SEOI_OFFSET                       0x420

#define IER_OFFSET0                       0x480   // 0x480 - 0x4f0
#define IER_OFFSET1                       0x490   // 0x480 - 0x4f0
#define IER_OFFSET2                       0x4a0   // 0x480 - 0x4f0
#define IER_OFFSET3                       0x4b0   // 0x480 - 0x4f0
#define IER_OFFSET4                       0x4c0   // 0x480 - 0x4f0
#define IER_OFFSET5                       0x4d0   // 0x480 - 0x4f0
#define IER_OFFSET6                       0x4e0   // 0x480 - 0x4f0
#define IER_OFFSET7                       0x4f0   // 0x480 - 0x4f0

#define EXT_INT_LOC_VEC_TBL_OFFSET0       0x500   // 0x500 - 0x530
#define EXT_INT_LOC_VEC_TBL_OFFSET1       0x510   // 0x500 - 0x530
#define EXT_INT_LOC_VEC_TBL_OFFSET2       0x520   // 0x500 - 0x530
#define EXT_INT_LOC_VEC_TBL_OFFSET3       0x530   // 0x500 - 0x530

struct apic_msr {
    union {
	uint64_t value;
	struct {
	    uint8_t rsvd;
	    uint8_t bootstrap_cpu : 1;
	    uint8_t rsvd2         : 2;
	    uint8_t apic_enable   : 1;
	    uint64_t base_addr    : 40;
	    uint32_t rsvd3        : 12;
	} __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));



typedef enum {INIT_ST, 
	      SIPI, 
	      STARTED} ipi_state_t; 

struct apic_dev_state;

struct apic_state {
    addr_t base_addr;

    /* MSRs */
    struct apic_msr base_addr_msr;


    /* memory map registers */

    struct lapic_id_reg lapic_id;
    struct apic_ver_reg apic_ver;
    struct ext_apic_ctrl_reg ext_apic_ctrl;
    struct local_vec_tbl_reg local_vec_tbl;
    struct tmr_vec_tbl_reg tmr_vec_tbl;
    struct tmr_div_cfg_reg tmr_div_cfg;
    struct lint_vec_tbl_reg lint0_vec_tbl;
    struct lint_vec_tbl_reg lint1_vec_tbl;
    struct perf_ctr_loc_vec_tbl_reg perf_ctr_loc_vec_tbl;
    struct therm_loc_vec_tbl_reg therm_loc_vec_tbl;
    struct err_vec_tbl_reg err_vec_tbl;
    struct err_status_reg err_status;
    struct spurious_int_reg spurious_int;
    struct int_cmd_reg int_cmd;
    struct log_dst_reg log_dst;
    struct dst_fmt_reg dst_fmt;
    struct arb_prio_reg arb_prio;
    struct task_prio_reg task_prio;
    struct proc_prio_reg proc_prio;
    struct ext_apic_feature_reg ext_apic_feature;
    struct spec_eoi_reg spec_eoi;
  

    uint32_t tmr_cur_cnt;
    uint32_t tmr_init_cnt;


    struct local_vec_tbl_reg ext_intr_vec_tbl[4];

    uint32_t rem_rd_data;


    ipi_state_t ipi_state;

    uint8_t int_req_reg[32];
    uint8_t int_svc_reg[32];
    uint8_t int_en_reg[32];
    uint8_t trig_mode_reg[32];

    struct guest_info * core;

    void * controller_handle;

    struct v3_timer * timer;

    uint32_t eoi;

    v3_lock_t  lock;

    // debug
    uint8_t in_icr;   
};




struct apic_dev_state {
    int num_apics;
    //    v3_lock_t ipi_lock;   // acquired by route_ipi - only one IPI active at a time

    struct apic_state apics[0];
} __attribute__((packed));





static int apic_read(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data);
static int apic_write(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data);

// No lcoking done
static void init_apic_state(struct apic_state * apic, uint32_t id) {
    apic->base_addr = DEFAULT_BASE_ADDR;

    if (id == 0) { 
	// boot processor, enabled
	apic->base_addr_msr.value = 0x0000000000000900LL;
    } else {
	// ap processor, enabled
	apic->base_addr_msr.value = 0x0000000000000800LL;
    }

    // same base address regardless of ap or main
    apic->base_addr_msr.value |= ((uint64_t)DEFAULT_BASE_ADDR); 

    PrintDebug("apic %u: (init_apic_state): msr=0x%llx\n",id, apic->base_addr_msr.value);

    PrintDebug("apic %u: (init_apic_state): Sizeof Interrupt Request Register %d, should be 32\n",
	       id, (uint_t)sizeof(apic->int_req_reg));

    memset(apic->int_req_reg, 0, sizeof(apic->int_req_reg));
    memset(apic->int_svc_reg, 0, sizeof(apic->int_svc_reg));
    memset(apic->int_en_reg, 0xff, sizeof(apic->int_en_reg));
    memset(apic->trig_mode_reg, 0, sizeof(apic->trig_mode_reg));

    apic->eoi = 0x00000000;
    apic->rem_rd_data = 0x00000000;
    apic->tmr_init_cnt = 0x00000000;
    apic->tmr_cur_cnt = 0x00000000;

    apic->lapic_id.val = id;
    
    apic->ipi_state = INIT_ST;

    // The P6 has 6 LVT entries, so we set the value to (6-1)...
    apic->apic_ver.val = 0x80050010;

    apic->task_prio.val = 0x00000000;
    apic->arb_prio.val = 0x00000000;
    apic->proc_prio.val = 0x00000000;
    apic->log_dst.val = 0x00000000;
    apic->dst_fmt.val = 0xffffffff;
    apic->spurious_int.val = 0x000000ff;
    apic->err_status.val = 0x00000000;
    apic->int_cmd.val = 0x0000000000000000LL;
    apic->tmr_vec_tbl.val = 0x00010000;
    apic->therm_loc_vec_tbl.val = 0x00010000;
    apic->perf_ctr_loc_vec_tbl.val = 0x00010000;
    apic->lint0_vec_tbl.val = 0x00010000;
    apic->lint1_vec_tbl.val = 0x00010000;
    apic->err_vec_tbl.val = 0x00010000;
    apic->tmr_div_cfg.val = 0x00000000;
    //apic->ext_apic_feature.val = 0x00000007;
    apic->ext_apic_feature.val = 0x00040007;
    apic->ext_apic_ctrl.val = 0x00000000;
    apic->spec_eoi.val = 0x00000000;

    v3_lock_init(&(apic->lock));

    //debug
    apic->in_icr=0;
}




// MSR handler - locks apic itself
static int read_apic_msr(struct guest_info * core, uint_t msr, v3_msr_t * dst, void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)priv_data;
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]);

    PrintDebug("apic %u: core %u: MSR read\n", apic->lapic_id.val, core->cpu_id);
    v3_lock(apic->lock);
    dst->value = apic->base_addr;
    v3_unlock(apic->lock);
    return 0;
}

// MSR handler - locks apic itself
static int write_apic_msr(struct guest_info * core, uint_t msr, v3_msr_t src, void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)priv_data;
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]);
    struct v3_mem_region * old_reg = v3_get_mem_region(core->vm_info, core->cpu_id, apic->base_addr);


    PrintDebug("apic %u: core %u: MSR write\n", apic->lapic_id.val, core->cpu_id);

    if (old_reg == NULL) {
	// uh oh...
	PrintError("apic %u: core %u: APIC Base address region does not exit...\n",
		   apic->lapic_id.val, core->cpu_id);
	return -1;
    }
    
    v3_lock(apic->lock);

    v3_delete_mem_region(core->vm_info, old_reg);

    apic->base_addr = src.value;

    if (v3_hook_full_mem(core->vm_info, core->cpu_id, apic->base_addr, 
			 apic->base_addr + PAGE_SIZE_4KB, 
			 apic_read, apic_write, apic_dev) == -1) {
	PrintError("apic %u: core %u: Could not hook new APIC Base address\n",
		   apic->lapic_id.val, core->cpu_id);
	v3_unlock(apic->lock);
	return -1;
    }

    v3_unlock(apic->lock);
    return 0;
}


// irq_num is the bit offset into a 256 bit buffer...
// return values
//    -1 = error
//     0 = OK, no interrupt needed now
//     1 = OK, interrupt needed now
// the caller is expeced to have locked the apic
static int activate_apic_irq_nolock(struct apic_state * apic, uint32_t irq_num) {
    int major_offset = (irq_num & ~0x00000007) >> 3;
    int minor_offset = irq_num & 0x00000007;
    uint8_t * req_location = apic->int_req_reg + major_offset;
    uint8_t * en_location = apic->int_en_reg + major_offset;
    uint8_t flag = 0x1 << minor_offset;


    if (irq_num <= 15 || irq_num>255) {
	PrintError("apic %u: core %d: Attempting to raise an invalid interrupt: %d\n", 
		   apic->lapic_id.val, apic->core->cpu_id, irq_num);
	return -1;
    }


    PrintDebug("apic %u: core %d: Raising APIC IRQ %d\n", apic->lapic_id.val, apic->core->cpu_id, irq_num);

    if (*req_location & flag) {
	PrintDebug("Interrupt %d  coallescing\n", irq_num);
	return 0;
    }

    if (*en_location & flag) {
	*req_location |= flag;

	if (apic->in_icr) { 
	    PrintError("apic %u: core %d: activate_apic_irq_nolock to deliver irq 0x%x when in_icr=1\n",  apic->lapic_id.val, apic->core->cpu_id, irq_num);
	    //	    return 0;
	}

	return 1;
    } else {
	PrintDebug("apic %u: core %d: Interrupt  not enabled... %.2x\n", 
		   apic->lapic_id.val, apic->core->cpu_id,*en_location);
	return 0;
    }

}


// Caller is expected to have locked the apic
static int get_highest_isr(struct apic_state * apic) {
    int i = 0, j = 0;

    // We iterate backwards to find the highest priority
    for (i = 31; i >= 0; i--) {
	uint8_t  * svc_major = apic->int_svc_reg + i;
    
	if ((*svc_major) & 0xff) {
	    for (j = 7; j >= 0; j--) {
		uint8_t flag = 0x1 << j;
		if ((*svc_major) & flag) {
		    return ((i * 8) + j);
		}
	    }
	}
    }

    return -1;
}
 

// Caller is expected to have locked the apic
static int get_highest_irr(struct apic_state * apic) {
    int i = 0, j = 0;

    // We iterate backwards to find the highest priority
    for (i = 31; i >= 0; i--) {
	uint8_t  * req_major = apic->int_req_reg + i;
    
	if ((*req_major) & 0xff) {
	    for (j = 7; j >= 0; j--) {
		uint8_t flag = 0x1 << j;
		if ((*req_major) & flag) {
		    return ((i * 8) + j);
		}
	    }
	}
    }

    return -1;
}
 


// Caller is expected to have locked the apic
static int apic_do_eoi(struct apic_state * apic) {
    int isr_irq = get_highest_isr(apic);

    if (isr_irq != -1) {
	int major_offset = (isr_irq & ~0x00000007) >> 3;
	int minor_offset = isr_irq & 0x00000007;
	uint8_t flag = 0x1 << minor_offset;
	uint8_t * svc_location = apic->int_svc_reg + major_offset;
	
	PrintDebug("apic %u: core ?: Received APIC EOI for IRQ %d\n", apic->lapic_id.val,isr_irq);
	
	*svc_location &= ~flag;

#ifdef CONFIG_CRAY_XT
	
	if ((isr_irq == 238) || 
	    (isr_irq == 239)) {
	    PrintDebug("apic %u: core ?: Acking IRQ %d\n", apic->lapic_id.val,isr_irq);
	}
	
	if (isr_irq == 238) {
	    V3_ACK_IRQ(238);
	}
#endif
    } else {
	//PrintError("apic %u: core ?: Spurious EOI...\n",apic->lapic_id.val);
    }
	
    return 0;
}
 
// Caller is expected to have locked the apic
static int activate_internal_irq_nolock(struct apic_state * apic, apic_irq_type_t int_type) {
    uint32_t vec_num = 0;
    uint32_t del_mode = 0;
    int masked = 0;


    switch (int_type) {
	case APIC_TMR_INT:
	    vec_num = apic->tmr_vec_tbl.vec;
	    del_mode = APIC_FIXED_DELIVERY;
	    masked = apic->tmr_vec_tbl.mask;
	    break;
	case APIC_THERM_INT:
	    vec_num = apic->therm_loc_vec_tbl.vec;
	    del_mode = apic->therm_loc_vec_tbl.msg_type;
	    masked = apic->therm_loc_vec_tbl.mask;
	    break;
	case APIC_PERF_INT:
	    vec_num = apic->perf_ctr_loc_vec_tbl.vec;
	    del_mode = apic->perf_ctr_loc_vec_tbl.msg_type;
	    masked = apic->perf_ctr_loc_vec_tbl.mask;
	    break;
	case APIC_LINT0_INT:
	    vec_num = apic->lint0_vec_tbl.vec;
	    del_mode = apic->lint0_vec_tbl.msg_type;
	    masked = apic->lint0_vec_tbl.mask;
	    break;
	case APIC_LINT1_INT:
	    vec_num = apic->lint1_vec_tbl.vec;
	    del_mode = apic->lint1_vec_tbl.msg_type;
	    masked = apic->lint1_vec_tbl.mask;
	    break;
	case APIC_ERR_INT:
	    vec_num = apic->err_vec_tbl.vec;
	    del_mode = APIC_FIXED_DELIVERY;
	    masked = apic->err_vec_tbl.mask;
	    break;
	default:
	    PrintError("apic %u: core ?: Invalid APIC interrupt type\n", apic->lapic_id.val);
	    return -1;
    }

    // interrupt is masked, don't send
    if (masked == 1) {
	PrintDebug("apic %u: core ?: Inerrupt is masked\n", apic->lapic_id.val);
	return 0;
    }

    if (del_mode == APIC_FIXED_DELIVERY) {
	//PrintDebug("Activating internal APIC IRQ %d\n", vec_num);
	return activate_apic_irq_nolock(apic, vec_num);
    } else {
	PrintError("apic %u: core ?: Unhandled Delivery Mode\n", apic->lapic_id.val);
	return -1;
    }
}


// Caller is expected to have locked the destination apic
static inline int should_deliver_cluster_ipi(struct guest_info * dst_core, 
					     struct apic_state * dst_apic, uint8_t mda) {

    if 	( ((mda & 0xf0) == (dst_apic->log_dst.dst_log_id & 0xf0)) &&  /* (I am in the cluster and */
	  ((mda & 0x0f) & (dst_apic->log_dst.dst_log_id & 0x0f)) ) {  /*  I am in the set)        */

	PrintDebug("apic %u core %u: accepting clustered IRQ (mda 0x%x == log_dst 0x%x)\n",
		   dst_apic->lapic_id.val, dst_core->cpu_id, mda, 
		   dst_apic->log_dst.dst_log_id);
	
	return 1;
    } else {
	PrintDebug("apic %u core %u: rejecting clustered IRQ (mda 0x%x != log_dst 0x%x)\n",
		   dst_apic->lapic_id.val, dst_core->cpu_id, mda, 
		   dst_apic->log_dst.dst_log_id);
	return 0;
    }
}

// Caller is expected to have locked the destiation apic
static inline int should_deliver_flat_ipi(struct guest_info * dst_core,
					  struct apic_state * dst_apic, uint8_t mda) {

    if (dst_apic->log_dst.dst_log_id & mda) {  // I am in the set 

	PrintDebug("apic %u core %u: accepting flat IRQ (mda 0x%x == log_dst 0x%x)\n",
		   dst_apic->lapic_id.val, dst_core->cpu_id, mda, 
		   dst_apic->log_dst.dst_log_id);

	return 1;

  } else {

	PrintDebug("apic %u core %u: rejecting flat IRQ (mda 0x%x != log_dst 0x%x)\n",
		   dst_apic->lapic_id.val, dst_core->cpu_id, mda, 
		   dst_apic->log_dst.dst_log_id);
	return 0;
  }
}


// Caller is expected to have locked the destiation apic
static int should_deliver_ipi(struct guest_info * dst_core, 
			      struct apic_state * dst_apic, uint8_t mda) {


    if (dst_apic->dst_fmt.model == 0xf) {

	if (mda == 0xff) {
	    /* always deliver broadcast */
	    return 1;
	}

	return should_deliver_flat_ipi(dst_core, dst_apic, mda);

    } else if (dst_apic->dst_fmt.model == 0x0) {

	if (mda == 0xff) {
	    /*  always deliver broadcast */
	    return 1;
	}

	return should_deliver_cluster_ipi(dst_core, dst_apic, mda);

    } else {
	PrintError("apic %u core %u: invalid destination format register value 0x%x for logical mode delivery.\n", 
		   dst_apic->lapic_id.val, dst_core->cpu_id, dst_apic->dst_fmt.model);
	return -1;
    }
}

// Caller is expected to have locked the destination apic
// Only the src_apic pointer is used
static int deliver_ipi(struct apic_state * src_apic, 
		       struct apic_state * dst_apic, 
		       uint32_t vector, uint8_t del_mode) {


    struct guest_info * dst_core = dst_apic->core;
    int do_xcall;

    switch (del_mode) {

	case APIC_FIXED_DELIVERY:  
	case APIC_LOWEST_DELIVERY:
	    // lowest priority - 
	    // caller needs to have decided which apic to deliver to!

	    PrintDebug("delivering IRQ %d to core %u\n", vector, dst_core->cpu_id); 

	    do_xcall=activate_apic_irq_nolock(dst_apic, vector);
	    
	    if (do_xcall<0) { 
		PrintError("Failed to activate apic irq!\n");
		return -1;
	    }

	    if (do_xcall && (dst_apic != src_apic)) { 
		// Assume core # is same as logical processor for now
		// TODO FIX THIS FIX THIS
		// THERE SHOULD BE:  guestapicid->virtualapicid map,
		//                   cpu_id->logical processor map
		//     host maitains logical proc->phsysical proc
		PrintDebug(" non-local core with new interrupt, forcing it to exit now\n"); 

#ifdef CONFIG_MULTITHREAD_OS
		v3_interrupt_cpu(dst_core->vm_info, dst_core->cpu_id, 0);
#else
		V3_ASSERT(0);
#endif
	    }

	    break;

	case APIC_INIT_DELIVERY: { 

	    PrintDebug(" INIT delivery to core %u\n", dst_core->cpu_id);

	    // TODO: any APIC reset on dest core (shouldn't be needed, but not sure...)

	    // Sanity check
	    if (dst_apic->ipi_state != INIT_ST) { 
		PrintError(" Warning: core %u is not in INIT state (mode = %d), ignored (assuming this is the deassert)\n",
			   dst_core->cpu_id, dst_apic->ipi_state);
		// Only a warning, since INIT INIT SIPI is common
		break;
	    }

	    // We transition the target core to SIPI state
	    dst_apic->ipi_state = SIPI;  // note: locking should not be needed here

	    // That should be it since the target core should be
	    // waiting in host on this transition
	    // either it's on another core or on a different preemptive thread
	    // in both cases, it will quickly notice this transition 
	    // in particular, we should not need to force an exit here

	    PrintDebug(" INIT delivery done\n");

	    break;							
	}
	case APIC_SIPI_DELIVERY: { 

	    // Sanity check
	    if (dst_apic->ipi_state != SIPI) { 
		PrintError(" core %u is not in SIPI state (mode = %d), ignored!\n",
			   dst_core->cpu_id, dst_apic->ipi_state);
		break;
	    }

	    // Write the RIP, CS, and descriptor
	    // assume the rest is already good to go
	    //
	    // vector VV -> rip at 0
	    //              CS = VV00
	    //  This means we start executing at linear address VV000
	    //
	    // So the selector needs to be VV00
	    // and the base needs to be VV000
	    //
	    dst_core->rip = 0;
	    dst_core->segments.cs.selector = vector << 8;
	    dst_core->segments.cs.limit = 0xffff;
	    dst_core->segments.cs.base = vector << 12;

	    PrintDebug(" SIPI delivery (0x%x -> 0x%x:0x0) to core %u\n",
		       vector, dst_core->segments.cs.selector, dst_core->cpu_id);
	    // Maybe need to adjust the APIC?
	    
	    // We transition the target core to SIPI state
	    dst_core->core_run_state = CORE_RUNNING;  // note: locking should not be needed here
	    dst_apic->ipi_state = STARTED;
	    
	    // As with INIT, we should not need to do anything else
	    
	    PrintDebug(" SIPI delivery done\n");
	    
	    break;							
	}
	case APIC_SMI_DELIVERY: 
	case APIC_RES1_DELIVERY: // reserved						
	case APIC_NMI_DELIVERY:
	case APIC_EXTINT_DELIVERY: // ExtInt
	default:
	    PrintError("IPI %d delivery is unsupported\n", del_mode); 
	    return -1;
    }
    
    return 0;
    
}

static struct apic_state * find_physical_apic(struct apic_dev_state *apic_dev, struct int_cmd_reg *icr)
{
    int i;
    
    if (icr->dst >0 && icr->dst < apic_dev->num_apics) { 
	// see if it simply is the core id
	if (apic_dev->apics[icr->dst].lapic_id.val == icr->dst) { 
	    return &(apic_dev->apics[icr->dst]);
	}
    }

    for (i=0;i<apic_dev->num_apics;i++) { 
	if (apic_dev->apics[i].lapic_id.val == icr->dst) { 
	    return &(apic_dev->apics[i]);
	}
    }
    
    return NULL;

}

// route_ipi is responsible for all locking
// the assumption is that you enter with no locks
// there is a global lock for the icc bus, so only
// one route_ipi progresses at any time
// destination apics are locked as needed
// if multiple apic locks are acquired at any point,
// this is done in the order of the array, so no
// deadlock should be possible
static int route_ipi(struct apic_dev_state * apic_dev,
		     struct apic_state * src_apic, 
		     struct int_cmd_reg * icr) {
    struct apic_state * dest_apic = NULL;


    //v3_lock(apic_dev->ipi_lock);  // this may not be needed
    // now I know only one IPI is being routed, this one
    // also, I do not have any apic locks
    // I need to acquire locks on pairs of src/dest apics
    // and I will do that using the total order
    // given by their cores


    PrintDebug("apic: IPI %s %u from apic %p to %s %s %u (icr=0x%llx)\n",
	       deliverymode_str[icr->del_mode], 
	       icr->vec, 
	       src_apic, 	       
	       (icr->dst_mode == 0) ? "(physical)" : "(logical)", 
	       shorthand_str[icr->dst_shorthand], 
	       icr->dst,
	       icr->val);

#if 0
        if (icr->vec!=48) { 
	    V3_Print("apic: IPI %s %u from apic %p to %s %s %u (icr=0x%llx)\n",
		    deliverymode_str[icr->del_mode], 
		    icr->vec, 
		    src_apic, 	       
		    (icr->dst_mode == 0) ? "(physical)" : "(logical)", 
		    shorthand_str[icr->dst_shorthand], 
		    icr->dst,
		    icr->val);
	}

#endif


    switch (icr->dst_shorthand) {

	case APIC_SHORTHAND_NONE:  // no shorthand
	    if (icr->dst_mode == APIC_DEST_PHYSICAL) { 

		dest_apic=find_physical_apic(apic_dev,icr);
		
		if (dest_apic==NULL) { 
		    PrintError("apic: Attempted send to unregistered apic id=%u\n", icr->dst);
		    goto route_ipi_out_bad;
		}


	
		//V3_Print("apic: phsyical destination of %u (apic %u at 0x%p)\n", icr->dst,dest_apic->lapic_id.val,dest_apic);

		v3_lock(dest_apic->lock);

		if (deliver_ipi(src_apic, dest_apic, 
				icr->vec, icr->del_mode) == -1) {
		    PrintError("apic: Could not deliver IPI\n");
		    v3_unlock(dest_apic->lock);
		    goto route_ipi_out_bad;
		}

		v3_unlock(dest_apic->lock);

		//V3_Print("apic: done\n");

	    } else if (icr->dst_mode == APIC_DEST_LOGICAL) {
		
		if (icr->del_mode!=APIC_LOWEST_DELIVERY ) { 
		    // logical, but not lowest priority
		    // we immediately trigger
		    // fixed, smi, reserved, nmi, init, sipi, etc
		    int i;
		    
		    uint8_t mda = icr->dst;
		    
		    for (i = 0; i < apic_dev->num_apics; i++) { 
			
			dest_apic = &(apic_dev->apics[i]);
			
			v3_lock(dest_apic->lock);
			
			int del_flag = should_deliver_ipi(dest_apic->core, dest_apic, mda);
			
			if (del_flag == -1) {
			    PrintError("apic: Error checking delivery mode\n");
			    v3_unlock(dest_apic->lock);
			    goto route_ipi_out_bad;
			} else if (del_flag == 1) {
			    if (deliver_ipi(src_apic, dest_apic, 
					    icr->vec, icr->del_mode) == -1) {
				PrintError("apic: Error: Could not deliver IPI\n");
				v3_unlock(dest_apic->lock);
				goto route_ipi_out_bad;
			    }
			}
			
			v3_unlock(dest_apic->lock);
		    }
		} else {  //APIC_LOWEST_DELIVERY
		    // logical, lowest priority
		    // scan, keeping a lock on the current best, then trigger
		    int i;
		    int have_cur_lock;
		    struct apic_state * cur_best_apic = NULL;
		    
		    uint8_t mda = icr->dst;
		    
		    have_cur_lock=0;


		    // Note that even if there are multiple concurrent 
		    // copies of this loop executing, they are all 
		    // locking in the same order

		    for (i = 0; i < apic_dev->num_apics; i++) { 
			
			dest_apic = &(apic_dev->apics[i]);
			
			v3_lock(dest_apic->lock);
			have_cur_lock=1;
			
			int del_flag = should_deliver_ipi(dest_apic->core, dest_apic, mda);
			
			if (del_flag == -1) {
			    PrintError("apic: Error checking delivery mode\n");
			    v3_unlock(dest_apic->lock);
			    if (cur_best_apic && cur_best_apic!=dest_apic) { 
				v3_unlock(cur_best_apic->lock);
			    }
			    goto route_ipi_out_bad;
			} else if (del_flag == 1) {
			    // update priority for lowest priority scan
			    if (!cur_best_apic) {
				cur_best_apic=dest_apic;  // note we leave it locked
				have_cur_lock=0;          // we will unlock as cur_best_apic
			    } else if (dest_apic->task_prio.val < cur_best_apic->task_prio.val) {
				// we now unlock the current best one and then switch
				// so in the end we have a lock on the new cur_best_apic
				v3_unlock(cur_best_apic->lock);
				cur_best_apic=dest_apic;
				have_cur_lock=0; // will unlock as cur_best_apic
			    } 
			}
			if (have_cur_lock) {
			    v3_unlock(dest_apic->lock);
			}
			
		    }
		    // now we will deliver to the best one if it exists
		    // and it is locked
		    if (!cur_best_apic) { 
			PrintDebug("apic: lowest priority deliver, but no destinations!\n");
		    } else {
			if (deliver_ipi(src_apic, cur_best_apic, 
					icr->vec, icr->del_mode) == -1) {
			    PrintError("apic: Error: Could not deliver IPI\n");
			    v3_unlock(cur_best_apic->lock);
			    goto route_ipi_out_bad;
			} else {
			    v3_unlock(cur_best_apic->lock);
			}
			    //V3_Print("apic: logical, lowest priority delivery to apic %u\n",cur_best_apic->lapic_id.val);
		    }
		}
	    }

	    break;
	    
	case APIC_SHORTHAND_SELF:  // self

	    /* I assume I am already locked! */

	    if (src_apic == NULL) {    /* this is not an apic, but it's trying to send to itself??? */
		PrintError("apic: Sending IPI to self from generic IPI sender\n");
		break;
	    }

	    v3_lock(src_apic->lock);

	    if (icr->dst_mode == APIC_DEST_PHYSICAL)  {  /* physical delivery */
		if (deliver_ipi(src_apic, src_apic, icr->vec, icr->del_mode) == -1) {
		    PrintError("apic: Could not deliver IPI to self (physical)\n");
		    v3_unlock(src_apic->lock);
		    goto route_ipi_out_bad;
		}
	    } else if (icr->dst_mode == APIC_DEST_LOGICAL) {  /* logical delivery */
		PrintError("apic: use of logical delivery in self (untested)\n");
		if (deliver_ipi(src_apic, src_apic, icr->vec, icr->del_mode) == -1) {
		    PrintError("apic: Could not deliver IPI to self (logical)\n");
		    v3_unlock(src_apic->lock);
		    goto route_ipi_out_bad;
		}
	    }
	    v3_unlock(src_apic->lock);
	    break;
	    
	case APIC_SHORTHAND_ALL: 
	case APIC_SHORTHAND_ALL_BUT_ME: { /* all and all-but-me */
	    /* assuming that logical verus physical doesn't matter
	       although it is odd that both are used */

	    int i;

	    for (i = 0; i < apic_dev->num_apics; i++) { 
		dest_apic = &(apic_dev->apics[i]);
		
		
		if ((dest_apic != src_apic) || (icr->dst_shorthand == APIC_SHORTHAND_ALL)) { 
		    v3_lock(dest_apic->lock);
		    if (deliver_ipi(src_apic, dest_apic, icr->vec, icr->del_mode) == -1) {
			PrintError("apic: Error: Could not deliver IPI\n");
			v3_unlock(dest_apic->lock);
			goto route_ipi_out_bad;
		    }
		    v3_unlock(dest_apic->lock);
		}
		
	    }
	}
	    break;
	default:
	    PrintError("apic: Error routing IPI, invalid Mode (%d)\n", icr->dst_shorthand);
	    goto route_ipi_out_bad;
    }


    // route_ipi_out_good:
    //v3_unlock(apic_dev->ipi_lock);  
    return 0;

 route_ipi_out_bad:
    //v3_unlock(apic_dev->ipi_lock);  
    return -1;
}


// External function, expected to acquire lock on apic
static int apic_read(struct guest_info * core, addr_t guest_addr, void * dst, uint_t length, void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(priv_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]);
    addr_t reg_addr  = guest_addr - apic->base_addr;
    struct apic_msr * msr = (struct apic_msr *)&(apic->base_addr_msr.value);
    uint32_t val = 0;

    v3_lock(apic->lock);

    PrintDebug("apic %u: core %u: at %p: Read apic address space (%p)\n",
	       apic->lapic_id.val, core->cpu_id, apic, (void *)guest_addr);

    if (msr->apic_enable == 0) {
	PrintError("apic %u: core %u: Read from APIC address space with disabled APIC, apic msr=0x%llx\n",
		   apic->lapic_id.val, core->cpu_id, apic->base_addr_msr.value);

	goto apic_read_out_bad;
    }


    /* Because "May not be supported" doesn't matter to Linux developers... */
    /*   if (length != 4) { */
    /*     PrintError("Invalid apic read length (%d)\n", length); */
    /*     return -1; */
    /*   } */

    switch (reg_addr & ~0x3) {
	case EOI_OFFSET:
	    // Well, only an idiot would read from a architectural write only register
	    // Oh, Hello Linux.
	    //    PrintError("Attempting to read from write only register\n");
	    //    return -1;
	    break;

	    // data registers
	case APIC_ID_OFFSET:
	    val = apic->lapic_id.val;
	    break;
	case APIC_VERSION_OFFSET:
	    val = apic->apic_ver.val;
	    break;
	case TPR_OFFSET:
	    val = apic->task_prio.val;
	    break;
	case APR_OFFSET:
	    val = apic->arb_prio.val;
	    break;
	case PPR_OFFSET:
	    val = apic->proc_prio.val;
	    break;
	case REMOTE_READ_OFFSET:
	    val = apic->rem_rd_data;
	    break;
	case LDR_OFFSET:
	    val = apic->log_dst.val;
	    break;
	case DFR_OFFSET:
	    val = apic->dst_fmt.val;
	    break;
	case SPURIOUS_INT_VEC_OFFSET:
	    val = apic->spurious_int.val;
	    break;
	case ESR_OFFSET:
	    val = apic->err_status.val;
	    break;
	case TMR_LOC_VEC_TBL_OFFSET:
	    val = apic->tmr_vec_tbl.val;
	    break;
	case LINT0_VEC_TBL_OFFSET:
	    val = apic->lint0_vec_tbl.val;
	    break;
	case LINT1_VEC_TBL_OFFSET:
	    val = apic->lint1_vec_tbl.val;
	    break;
	case ERR_VEC_TBL_OFFSET:
	    val = apic->err_vec_tbl.val;
	    break;
	case TMR_INIT_CNT_OFFSET:
	    val = apic->tmr_init_cnt;
	    break;
	case TMR_DIV_CFG_OFFSET:
	    val = apic->tmr_div_cfg.val;
	    break;

	case IER_OFFSET0:
	    val = *(uint32_t *)(apic->int_en_reg);
	    break;
	case IER_OFFSET1:
	    val = *(uint32_t *)(apic->int_en_reg + 4);
	    break;
	case IER_OFFSET2:
	    val = *(uint32_t *)(apic->int_en_reg + 8);
	    break;
	case IER_OFFSET3:
	    val = *(uint32_t *)(apic->int_en_reg + 12);
	    break;
	case IER_OFFSET4:
	    val = *(uint32_t *)(apic->int_en_reg + 16);
	    break;
	case IER_OFFSET5:
	    val = *(uint32_t *)(apic->int_en_reg + 20);
	    break;
	case IER_OFFSET6:
	    val = *(uint32_t *)(apic->int_en_reg + 24);
	    break;
	case IER_OFFSET7:
	    val = *(uint32_t *)(apic->int_en_reg + 28);
	    break;

	case ISR_OFFSET0:
	    val = *(uint32_t *)(apic->int_svc_reg);
	    break;
	case ISR_OFFSET1:
	    val = *(uint32_t *)(apic->int_svc_reg + 4);
	    break;
	case ISR_OFFSET2:
	    val = *(uint32_t *)(apic->int_svc_reg + 8);
	    break;
	case ISR_OFFSET3:
	    val = *(uint32_t *)(apic->int_svc_reg + 12);
	    break;
	case ISR_OFFSET4:
	    val = *(uint32_t *)(apic->int_svc_reg + 16);
	    break;
	case ISR_OFFSET5:
	    val = *(uint32_t *)(apic->int_svc_reg + 20);
	    break;
	case ISR_OFFSET6:
	    val = *(uint32_t *)(apic->int_svc_reg + 24);
	    break;
	case ISR_OFFSET7:
	    val = *(uint32_t *)(apic->int_svc_reg + 28);
	    break;
   
	case TRIG_OFFSET0:
	    val = *(uint32_t *)(apic->trig_mode_reg);
	    break;
	case TRIG_OFFSET1:
	    val = *(uint32_t *)(apic->trig_mode_reg + 4);
	    break;
	case TRIG_OFFSET2:
	    val = *(uint32_t *)(apic->trig_mode_reg + 8);
	    break;
	case TRIG_OFFSET3:
	    val = *(uint32_t *)(apic->trig_mode_reg + 12);
	    break;
	case TRIG_OFFSET4:
	    val = *(uint32_t *)(apic->trig_mode_reg + 16);
	    break;
	case TRIG_OFFSET5:
	    val = *(uint32_t *)(apic->trig_mode_reg + 20);
	    break;
	case TRIG_OFFSET6:
	    val = *(uint32_t *)(apic->trig_mode_reg + 24);
	    break;
	case TRIG_OFFSET7:
	    val = *(uint32_t *)(apic->trig_mode_reg + 28);
	    break;

	case IRR_OFFSET0:
	    val = *(uint32_t *)(apic->int_req_reg);
	    break;
	case IRR_OFFSET1:
	    val = *(uint32_t *)(apic->int_req_reg + 4);
	    break;
	case IRR_OFFSET2:
	    val = *(uint32_t *)(apic->int_req_reg + 8);
	    break;
	case IRR_OFFSET3:
	    val = *(uint32_t *)(apic->int_req_reg + 12);
	    break;
	case IRR_OFFSET4:
	    val = *(uint32_t *)(apic->int_req_reg + 16);
	    break;
	case IRR_OFFSET5:
	    val = *(uint32_t *)(apic->int_req_reg + 20);
	    break;
	case IRR_OFFSET6:
	    val = *(uint32_t *)(apic->int_req_reg + 24);
	    break;
	case IRR_OFFSET7:
	    val = *(uint32_t *)(apic->int_req_reg + 28);
	    break;
	case TMR_CUR_CNT_OFFSET:
	    val = apic->tmr_cur_cnt;
	    break;

	    // We are not going to implement these....
	case THERM_LOC_VEC_TBL_OFFSET:
	    val = apic->therm_loc_vec_tbl.val;
	    break;
	case PERF_CTR_LOC_VEC_TBL_OFFSET:
	    val = apic->perf_ctr_loc_vec_tbl.val;
	    break;

 

	    // handled registers
	case INT_CMD_LO_OFFSET:    
	    val = apic->int_cmd.lo;
	    break;
	case INT_CMD_HI_OFFSET:
	    val = apic->int_cmd.hi;
	    break;

	    // handle current timer count

	    // Unhandled Registers
	case EXT_INT_LOC_VEC_TBL_OFFSET0:
	    val = apic->ext_intr_vec_tbl[0].val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET1:
	    val = apic->ext_intr_vec_tbl[1].val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET2:
	    val = apic->ext_intr_vec_tbl[2].val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET3:
	    val = apic->ext_intr_vec_tbl[3].val;
	    break;
    

	case EXT_APIC_FEATURE_OFFSET:
	case EXT_APIC_CMD_OFFSET:
	case SEOI_OFFSET:

	default:
	    PrintError("apic %u: core %u: Read from Unhandled APIC Register: %x (getting zero)\n", 
		       apic->lapic_id.val, core->cpu_id, (uint32_t)reg_addr);
	    goto apic_read_out_bad;
    }


    if (length == 1) {
	uint_t byte_addr = reg_addr & 0x3;
	uint8_t * val_ptr = (uint8_t *)dst;
    
	*val_ptr = *(((uint8_t *)&val) + byte_addr);

    } else if ((length == 2) && 
	       ((reg_addr & 0x3) == 0x3)) {
	uint_t byte_addr = reg_addr & 0x3;
	uint16_t * val_ptr = (uint16_t *)dst;
	*val_ptr = *(((uint16_t *)&val) + byte_addr);

    } else if (length == 4) {
	uint32_t * val_ptr = (uint32_t *)dst;
	*val_ptr = val;

    } else {
	PrintError("apic %u: core %u: Invalid apic read length (%d)\n", 
		   apic->lapic_id.val, core->cpu_id, length);
	goto apic_read_out_bad;
    }

    PrintDebug("apic %u: core %u: Read finished (val=%x)\n", 
	       apic->lapic_id.val, core->cpu_id, *(uint32_t *)dst);


    // apic_read_out_good:
    v3_unlock(apic->lock);
    return length;

 apic_read_out_bad:
    v3_unlock(apic->lock);
    return -1;
}


/**
 *
 */
static int apic_write(struct guest_info * core, addr_t guest_addr, void * src, uint_t length, void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(priv_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 
    addr_t reg_addr  = guest_addr - apic->base_addr;
    struct apic_msr * msr = (struct apic_msr *)&(apic->base_addr_msr.value);
    uint32_t op_val = *(uint32_t *)src;


    v3_lock(apic->lock);

    PrintDebug("apic %u: core %u: at %p and priv_data is at %p\n",
	       apic->lapic_id.val, core->cpu_id, apic, priv_data);

    PrintDebug("apic %u: core %u: write to address space (%p) (val=%x)\n", 
	       apic->lapic_id.val, core->cpu_id, (void *)guest_addr, *(uint32_t *)src);

    if (msr->apic_enable == 0) {
	PrintError("apic %u: core %u: Write to APIC address space with disabled APIC, apic msr=0x%llx\n",
		   apic->lapic_id.val, core->cpu_id, apic->base_addr_msr.value);
	goto apic_write_out_bad;
    }


    if (length != 4) {
	PrintError("apic %u: core %u: Invalid apic write length (%d)\n", 
		   apic->lapic_id.val, length, core->cpu_id);
	goto apic_write_out_bad;
    }

    switch (reg_addr) {
	case REMOTE_READ_OFFSET:
	case APIC_VERSION_OFFSET:
	case APR_OFFSET:
	case IRR_OFFSET0:
	case IRR_OFFSET1:
	case IRR_OFFSET2:
	case IRR_OFFSET3:
	case IRR_OFFSET4:
	case IRR_OFFSET5:
	case IRR_OFFSET6:
	case IRR_OFFSET7:
	case ISR_OFFSET0:
	case ISR_OFFSET1:
	case ISR_OFFSET2:
	case ISR_OFFSET3:
	case ISR_OFFSET4:
	case ISR_OFFSET5:
	case ISR_OFFSET6:
	case ISR_OFFSET7:
	case TRIG_OFFSET0:
	case TRIG_OFFSET1:
	case TRIG_OFFSET2:
	case TRIG_OFFSET3:
	case TRIG_OFFSET4:
	case TRIG_OFFSET5:
	case TRIG_OFFSET6:
	case TRIG_OFFSET7:
	case PPR_OFFSET:
	case EXT_APIC_FEATURE_OFFSET:

	    PrintError("apic %u: core %u: Attempting to write to read only register %p (error)\n", 
		       apic->lapic_id.val, core->cpu_id, (void *)reg_addr);
	    //  goto apic_write_out_bad;

	    break;

	    // Data registers
	case APIC_ID_OFFSET:
	    //V3_Print("apic %u: core %u: my id is being changed to %u\n", 
	    //       apic->lapic_id.val, core->cpu_id, op_val);

	    apic->lapic_id.val = op_val;
	    break;
	case TPR_OFFSET:
	    apic->task_prio.val = op_val;
	    break;
	case LDR_OFFSET:
	    PrintDebug("apic %u: core %u: setting log_dst.val to 0x%x\n",
		       apic->lapic_id.val, core->cpu_id, op_val);
	    apic->log_dst.val = op_val;
	    break;
	case DFR_OFFSET:
	    apic->dst_fmt.val = op_val;
	    break;
	case SPURIOUS_INT_VEC_OFFSET:
	    apic->spurious_int.val = op_val;
	    break;
	case ESR_OFFSET:
	    apic->err_status.val = op_val;
	    break;
	case TMR_LOC_VEC_TBL_OFFSET:
	    apic->tmr_vec_tbl.val = op_val;
	    break;
	case THERM_LOC_VEC_TBL_OFFSET:
	    apic->therm_loc_vec_tbl.val = op_val;
	    break;
	case PERF_CTR_LOC_VEC_TBL_OFFSET:
	    apic->perf_ctr_loc_vec_tbl.val = op_val;
	    break;
	case LINT0_VEC_TBL_OFFSET:
	    apic->lint0_vec_tbl.val = op_val;
	    break;
	case LINT1_VEC_TBL_OFFSET:
	    apic->lint1_vec_tbl.val = op_val;
	    break;
	case ERR_VEC_TBL_OFFSET:
	    apic->err_vec_tbl.val = op_val;
	    break;
	case TMR_INIT_CNT_OFFSET:
	    apic->tmr_init_cnt = op_val;
	    apic->tmr_cur_cnt = op_val;
	    break;
	case TMR_CUR_CNT_OFFSET:
	    apic->tmr_cur_cnt = op_val;
	    break;
	case TMR_DIV_CFG_OFFSET:
	    apic->tmr_div_cfg.val = op_val;
	    break;


	    // Enable mask (256 bits)
	case IER_OFFSET0:
	    *(uint32_t *)(apic->int_en_reg) = op_val;
	    break;
	case IER_OFFSET1:
	    *(uint32_t *)(apic->int_en_reg + 4) = op_val;
	    break;
	case IER_OFFSET2:
	    *(uint32_t *)(apic->int_en_reg + 8) = op_val;
	    break;
	case IER_OFFSET3:
	    *(uint32_t *)(apic->int_en_reg + 12) = op_val;
	    break;
	case IER_OFFSET4:
	    *(uint32_t *)(apic->int_en_reg + 16) = op_val;
	    break;
	case IER_OFFSET5:
	    *(uint32_t *)(apic->int_en_reg + 20) = op_val;
	    break;
	case IER_OFFSET6:
	    *(uint32_t *)(apic->int_en_reg + 24) = op_val;
	    break;
	case IER_OFFSET7:
	    *(uint32_t *)(apic->int_en_reg + 28) = op_val;
	    break;

	case EXT_INT_LOC_VEC_TBL_OFFSET0:
	    apic->ext_intr_vec_tbl[0].val = op_val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET1:
	    apic->ext_intr_vec_tbl[1].val = op_val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET2:
	    apic->ext_intr_vec_tbl[2].val = op_val;
	    break;
	case EXT_INT_LOC_VEC_TBL_OFFSET3:
	    apic->ext_intr_vec_tbl[3].val = op_val;
	    break;


	    // Action Registers
	case EOI_OFFSET:
	    // do eoi (we already have the lock)
	    apic_do_eoi(apic);
	    break;

	case INT_CMD_LO_OFFSET: {
	    // execute command (we already have the lock)

	    struct int_cmd_reg tmp_icr;

	    apic->int_cmd.lo = op_val;

	    tmp_icr=apic->int_cmd;

	    //	    V3_Print("apic %u: core %u: sending cmd 0x%llx to apic %u\n", 
	    //       apic->lapic_id.val, core->cpu_id,
	    //       apic->int_cmd.val, apic->int_cmd.dst);

	    apic->in_icr=0;

	    v3_unlock(apic->lock);

	    // route_ipi is responsible for locking apics, so we go in unlocked)
	    if (route_ipi(apic_dev, apic, &tmp_icr) == -1) { 
		PrintError("IPI Routing failure\n");
		goto apic_write_out_bad;
	    }

	    //	    v3_lock(apic->lock);  // expected for leaving this function

	}
	    break;

	case INT_CMD_HI_OFFSET: {
	    // already have the lock
	    if (apic->in_icr) { 
		PrintError("apic %u: core %u: writing command high=0x%x while in_icr=1\n", apic->lapic_id.val, core->cpu_id,apic->int_cmd.hi);
	    }

	    apic->int_cmd.hi = op_val;
	    //V3_Print("apic %u: core %u: writing command high=0x%x\n", apic->lapic_id.val, core->cpu_id,apic->int_cmd.hi);
	    apic->in_icr=1;
	}
	    break;


	// Unhandled Registers
	case EXT_APIC_CMD_OFFSET:
	case SEOI_OFFSET:
	default:
	    PrintError("apic %u: core %u: Write to Unhandled APIC Register: %x (ignored)\n", 
		       apic->lapic_id.val, core->cpu_id, (uint32_t)reg_addr);

	    goto apic_write_out_bad;
    }

    PrintDebug("apic %u: core %u: Write finished\n", apic->lapic_id.val, core->cpu_id);

    // apic_write_out_good:
    v3_unlock(apic->lock);
    return length;

 apic_write_out_bad:
    v3_unlock(apic->lock);
    return -1;
}



/* Interrupt Controller Functions */

// internally used, expects caller to lock
static int apic_intr_pending_nolock(struct guest_info * core, void * private_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 
    int req_irq = get_highest_irr(apic);
    int svc_irq = get_highest_isr(apic);

    //    PrintDebug("apic %u: core %u: req_irq=%d, svc_irq=%d\n",apic->lapic_id.val,info->cpu_id,req_irq,svc_irq);

    if ((req_irq >= 0) && 
	(req_irq > svc_irq)) {
	return 1;
    }

    return 0;
}

// externally visible, so must lock itself
static int apic_intr_pending(struct guest_info * core, void * private_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 
    int rc;

    v3_lock(apic->lock);
    
    rc=apic_intr_pending_nolock(core,private_data);
    
    v3_unlock(apic->lock);

    return rc;
}

// Internal - no lock
static int apic_get_intr_number_nolock(struct guest_info * core, void * private_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 
    int req_irq = get_highest_irr(apic);
    int svc_irq = get_highest_isr(apic);

    if (svc_irq == -1) {
	return req_irq;
    } else if (svc_irq < req_irq) {
	return req_irq;
    }

    return -1;
}


// Externally visible, so must lock itself
static int apic_get_intr_number(struct guest_info * core, void * private_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 
    int rc;

    v3_lock(apic->lock);
    
    rc=apic_get_intr_number_nolock(core,private_data);

    v3_unlock(apic->lock);

    return rc;
}


//
// Here there is no source APIC, so there is no need to lock it
// Furthermore, the expectation is that route_ipi will lock the destiation apic
int v3_apic_send_ipi(struct v3_vm_info * vm, struct v3_gen_ipi * ipi, void * dev_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)
	(((struct vm_device *)dev_data)->private_data);
    struct int_cmd_reg tmp_icr;

    // zero out all the fields
    tmp_icr.val = 0;

    tmp_icr.vec = ipi->vector;
    tmp_icr.del_mode = ipi->mode;
    tmp_icr.dst_mode = ipi->logical;
    tmp_icr.trig_mode = ipi->trigger_mode;
    tmp_icr.dst_shorthand = ipi->dst_shorthand;
    tmp_icr.dst = ipi->dst;

    // route_ipi is responsible for locking the destination apic
    return route_ipi(apic_dev, NULL, &tmp_icr);
}


int v3_apic_raise_intr(struct v3_vm_info * vm, uint32_t irq, uint32_t dst, void * dev_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)
	(((struct vm_device*)dev_data)->private_data);
    struct apic_state * apic = &(apic_dev->apics[dst]); 
    int do_xcall;

    PrintDebug("apic %u core ?: raising interrupt IRQ %u (dst = %u).\n", apic->lapic_id.val, irq, dst); 

    v3_lock(apic->lock);

    do_xcall=activate_apic_irq_nolock(apic, irq);

    if (do_xcall<0) { 
	PrintError("Failed to activate apic irq\n");
	v3_unlock(apic->lock);
	return -1;
    }
    
    if (do_xcall>0 && (V3_Get_CPU() != dst)) {
#ifdef CONFIG_MULTITHREAD_OS
	v3_interrupt_cpu(vm, dst, 0);
#else
	V3_ASSERT(0);
#endif

    }
    v3_unlock(apic->lock);
    return 0;
}


// internal - caller must lock
static int apic_begin_irq_nolock(struct guest_info * core, void * private_data, int irq) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 
    int major_offset = (irq & ~0x00000007) >> 3;
    int minor_offset = irq & 0x00000007;
    uint8_t *req_location = apic->int_req_reg + major_offset;
    uint8_t *svc_location = apic->int_svc_reg + major_offset;
    uint8_t flag = 0x01 << minor_offset;

    if (*req_location & flag) {
	// we will only pay attention to a begin irq if we
	// know that we initiated it!
	*svc_location |= flag;
	*req_location &= ~flag;
    } else {
	// do nothing... 
	//PrintDebug("apic %u: core %u: begin irq for %d ignored since I don't own it\n",
	//	   apic->lapic_id.val, core->cpu_id, irq);
    }

    return 0;
}

// Since this is called, externally, it should lock the apic
static int apic_begin_irq(struct guest_info * core, void * private_data, int irq) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(private_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 
    int rc;

    v3_lock(apic->lock);

    rc=apic_begin_irq_nolock(core,private_data,irq);

    v3_unlock(apic->lock);

    return rc;
}



/* Timer Functions */
// Caller will lock the apic
static void apic_update_time_nolock(struct guest_info * core, 
			     uint64_t cpu_cycles, uint64_t cpu_freq, 
			     void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(priv_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 

    // The 32 bit GCC runtime is a pile of shit
#ifdef __V3_64BIT__
    uint64_t tmr_ticks = 0;
#else 
    uint32_t tmr_ticks = 0;
#endif

    uint8_t tmr_div = *(uint8_t *)&(apic->tmr_div_cfg.val);
    uint_t shift_num = 0;


    // Check whether this is true:
    //   -> If the Init count is zero then the timer is disabled
    //      and doesn't just blitz interrupts to the CPU
    if ((apic->tmr_init_cnt == 0) || 
	( (apic->tmr_vec_tbl.tmr_mode == APIC_TMR_ONESHOT) &&
	  (apic->tmr_cur_cnt == 0))) {
	//PrintDebug("apic %u: core %u: APIC timer not yet initialized\n",apic->lapic_id.val,info->cpu_id);
	return;
    }


    switch (tmr_div) {
	case APIC_TMR_DIV1:
	    shift_num = 0;
	    break;
	case APIC_TMR_DIV2:
	    shift_num = 1;
	    break;
	case APIC_TMR_DIV4:
	    shift_num = 2;
	    break;
	case APIC_TMR_DIV8:
	    shift_num = 3;
	    break;
	case APIC_TMR_DIV16:
	    shift_num = 4;
	    break;
	case APIC_TMR_DIV32:
	    shift_num = 5;
	    break;
	case APIC_TMR_DIV64:
	    shift_num = 6;
	    break;
	case APIC_TMR_DIV128:
	    shift_num = 7;
	    break;
	default:
	    PrintError("apic %u: core %u: Invalid Timer Divider configuration\n",
		       apic->lapic_id.val, core->cpu_id);
	    return;
    }

    tmr_ticks = cpu_cycles >> shift_num;
    //    PrintDebug("Timer Ticks: %p\n", (void *)tmr_ticks);

    if (tmr_ticks < apic->tmr_cur_cnt) {
	apic->tmr_cur_cnt -= tmr_ticks;
    } else {
	tmr_ticks -= apic->tmr_cur_cnt;
	apic->tmr_cur_cnt = 0;

	// raise irq
	PrintDebug("apic %u: core %u: Raising APIC Timer interrupt (periodic=%d) (icnt=%d) (div=%d)\n",
		   apic->lapic_id.val, core->cpu_id,
		   apic->tmr_vec_tbl.tmr_mode, apic->tmr_init_cnt, shift_num);

	if (apic_intr_pending_nolock(core, priv_data)) {
	    PrintDebug("apic %u: core %u: Overriding pending IRQ %d\n", 
		       apic->lapic_id.val, core->cpu_id, 
		       apic_get_intr_number(core, priv_data));
	}

	if (activate_internal_irq_nolock(apic, APIC_TMR_INT) == -1) {
	    PrintError("apic %u: core %u: Could not raise Timer interrupt\n",
		       apic->lapic_id.val, core->cpu_id);
	}
    
	if (apic->tmr_vec_tbl.tmr_mode == APIC_TMR_PERIODIC) {
	    tmr_ticks = tmr_ticks % apic->tmr_init_cnt;
	    apic->tmr_cur_cnt = apic->tmr_init_cnt - tmr_ticks;
	}
    }

    return;
}


static void apic_update_time(struct guest_info * core, 
			     uint64_t cpu_cycles, uint64_t cpu_freq, 
			     void * priv_data) {
    struct apic_dev_state * apic_dev = (struct apic_dev_state *)(priv_data);
    struct apic_state * apic = &(apic_dev->apics[core->cpu_id]); 

    v3_lock(apic->lock);

    apic_update_time_nolock(core,cpu_cycles,cpu_freq,priv_data);

    v3_unlock(apic->lock);
    
    return;
}

static struct intr_ctrl_ops intr_ops = {
    .intr_pending = apic_intr_pending,
    .get_intr_number = apic_get_intr_number,
    .begin_irq = apic_begin_irq,
};


static struct v3_timer_ops timer_ops = {
    .update_timer = apic_update_time,
};




static int apic_free(struct apic_dev_state * apic_dev) {
    int i = 0;
    struct v3_vm_info * vm = NULL;

    for (i = 0; i < apic_dev->num_apics; i++) {
	struct apic_state * apic = &(apic_dev->apics[i]);
	struct guest_info * core = apic->core;
	
	vm = core->vm_info;

	v3_remove_intr_controller(core, apic->controller_handle);

	if (apic->timer) {
	    v3_remove_timer(core, apic->timer);
	}

	// unhook memory

    }

    v3_unhook_msr(vm, BASE_ADDR_MSR);

    V3_Free(apic_dev);
    return 0;
}


static struct v3_device_ops dev_ops = {
    .free = (int (*)(void *))apic_free,
};



static int apic_init(struct v3_vm_info * vm, v3_cfg_tree_t * cfg) {
    char * dev_id = v3_cfg_val(cfg, "ID");
    struct apic_dev_state * apic_dev = NULL;
    int i = 0;

    PrintDebug("apic: creating an APIC for each core\n");

    apic_dev = (struct apic_dev_state *)V3_Malloc(sizeof(struct apic_dev_state) + 
						  sizeof(struct apic_state) * vm->num_cores);

    apic_dev->num_apics = vm->num_cores;

    //v3_lock_init(&(apic_dev->ipi_lock));

    struct vm_device * dev = v3_add_device(vm, dev_id, &dev_ops, apic_dev);

    if (dev == NULL) {
	PrintError("apic: Could not attach device %s\n", dev_id);
	V3_Free(apic_dev);
	return -1;
    }

    
    for (i = 0; i < vm->num_cores; i++) {
	struct apic_state * apic = &(apic_dev->apics[i]);
	struct guest_info * core = &(vm->cores[i]);

	apic->core = core;

	init_apic_state(apic, i);

    	apic->controller_handle = v3_register_intr_controller(core, &intr_ops, apic_dev);

    	apic->timer = v3_add_timer(core, &timer_ops, apic_dev);

	if (apic->timer == NULL) {
	    PrintError("APIC: Failed to attach timer to core %d\n", i);
	    v3_remove_device(dev);
	    return -1;
	}

	v3_hook_full_mem(vm, core->cpu_id, apic->base_addr, apic->base_addr + PAGE_SIZE_4KB, apic_read, apic_write, apic_dev);

	PrintDebug("apic %u: (setup device): done, my id is %u\n", i, apic->lapic_id.val);
    }

#ifdef CONFIG_DEBUG_APIC
    for (i = 0; i < vm->num_cores; i++) {
	struct apic_state * apic = &(apic_dev->apics[i]);
	PrintDebug("apic: sanity check: apic %u (at %p) has id %u and msr value %llx and core at %p\n",
		   i, apic, apic->lapic_id.val, apic->base_addr_msr.value,apic->core);
    }
#endif


    PrintDebug("apic: priv_data is at %p\n", apic_dev);

    v3_hook_msr(vm, BASE_ADDR_MSR, read_apic_msr, write_apic_msr, apic_dev);

    return 0;
}



device_register("LAPIC", apic_init)
