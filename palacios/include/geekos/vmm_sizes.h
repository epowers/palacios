#ifndef __vmm_sizes
#define __vmm_sizes
#define KERNEL_LOAD_ADDRESS  0x2fe9d000
#define KERNEL_START (KERNEL_LOAD_ADDRESS)
#define KERNEL_CORE_LENGTH ( 230 *512)
#define KERNEL_END (KERNEL_LOAD_ADDRESS+KERNEL_CORE_LENGTH-1)
#define VM_KERNEL_LENGTH ( 97 *512)
#define VM_KERNEL_START (KERNEL_LOAD_ADDRESS + KERNEL_CORE_LENGTH)
#define VM_BOOT_PACKAGE_START (VM_KERNEL_START) 
#define VM_BOOT_PACKAGE_END  (VM_KERNEL_START+VM_KERNEL_LENGTH-1) 
#endif
