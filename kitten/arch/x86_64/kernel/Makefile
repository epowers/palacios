extra-y		:= head.o head64.o init_task.o vmlwk.lds
EXTRA_FLAGS	:= -traditional
obj-y		:= percpu.o setup.o e820.o cpuinfo.o resource.o \
		   mpparse.o entry.o show.o syscall.o i387.o cpu.o \
		   lapic.o ioapic.o trampoline.o interrupts.o mpboot.o \
		   time.o sys_arch_prctl.o vsyscall.o xcall.o \
		   task.o sched.o

obj-$(CONFIG_CRAY_XT) += rca/
obj-$(CONFIG_V3VEE) += bios.o


