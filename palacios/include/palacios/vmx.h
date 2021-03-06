
/* 
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National 
 * Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at 
 * http://www.v3vee.org
 *
 * Copyright (c) 2008, Peter Dinda <pdinda@northwestern.edu> 
 * Copyright (c) 2008, Jack Lange <jarusl@cs.northwestern.edu> 
 * Copyright (c) 2008, The V3VEE Project <http://www.v3vee.org> 
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 * Author: Jack Lange <jarusl@cs.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "V3VEE_LICENSE".
 */


#ifndef __VMX_H
#define __VMX_H

#ifdef __V3VEE__

#include <palacios/vmm_types.h>
#include <palacios/vmcs.h>

#define IA32_FEATURE_CONTROL_MSR ((unsigned int)0x3a)
#define IA32_VMX_BASIC_MSR ((unsigned int)0x480)
#define IA32_VMX_PINBASED_CTLS_MSR ((unsigned int)0x481)
#define IA32_VMX_PROCBASED_CTLS_MSR ((unsigned int)0x482)
#define IA32_VMX_EXIT_CTLS_MSR ((unsigned int)0x483)
#define IA32_VMX_ENTRY_CTLS_MSR ((unsigned int)0x484)
#define IA32_VMX_MISC_MSR ((unsigned int)0x485)
#define IA32_VMX_CR0_FIXED0_MSR ((unsigned int)0x486)
#define IA32_VMX_CR0_FIXED1_MSR ((unsigned int)0x487)
#define IA32_VMX_CR4_FIXED0_MSR ((unsigned int)0x488)
#define IA32_VMX_CR4_FIXED1_MSR ((unsigned int)0x489)
#define IA32_VMX_VMCS_ENUM_MSR ((unsigned ing)0x48A)

#define VMX_SUCCESS         0
#define VMX_FAIL_INVALID   1
#define VMX_FAIL_VALID     2
#define VMM_ERROR          3

#define FEATURE_CONTROL_LOCK (1)
#define FEATURE_CONTROL_VMXON (1<<2)
#define FEATURE_CONTROL_VALID ( FEATURE_CONTROL_LOCK | FEATURE_CONTROL_VMXON)


#define CPUID_1_ECX_VTXFLAG (1<<5)





typedef void VmxOnRegion;



struct MSR_REGS {
  uint_t low;
  uint_t high;
} __attribute__((packed));

struct VMX_BASIC {
  uint_t revision;
  uint_t regionSize   : 13;
  uint_t rsvd1        : 4; // Always 0
  uint_t physWidth    : 1;
  uint_t smm          : 1; // Always 1
  uint_t memType      : 4;
  uint_t rsvd2        : 10; // Always 0
}  __attribute__((packed));

union VMX_MSR {
  struct MSR_REGS regs;
  struct VMX_BASIC vmxBasic;
}  __attribute__((packed));


struct VMDescriptor {
  uint_t   entry_ip;
  uint_t   exit_eip;
  uint_t   guest_esp;
}  __attribute__((packed));


enum VMState { VM_VMXASSIST_STARTUP, VM_VMXASSIST_V8086_BIOS, VM_VMXASSIST_V8086, VM_NORMAL };

struct VM {
  enum VMState        state;
  struct VMXRegs      registers;
  struct VMDescriptor descriptor;
  struct VMCSData     vmcs;
  struct VMCS         *vmcsregion;
  struct VmxOnRegion  *vmxonregion;
};


enum InstructionType { VM_UNKNOWN_INST, VM_MOV_TO_CR0 } ;

struct Instruction {
  enum InstructionType type;
  uint_t          address;
  uint_t          size;
  uint_t          input1;
  uint_t          input2;
  uint_t          output;
};


void DecodeCurrentInstruction(struct VM *vm, struct Instruction *out);


int is_vmx_capable();

VmxOnRegion * Init_VMX();
VmxOnRegion * CreateVmxOnRegion();

int VMLaunch(struct VMDescriptor *vm);


int Do_VMM(struct VMXRegs regs);


#endif // ! __V3VEE__

#endif 
