ccflags-y += -I$(srctree)/kernel/nanokernel/include
ccflags-y +=-I$(srctree)/arch/$(ARCH)/include
ccflags-y += -I$(srctree)/kernel/microkernel/include

obj-y += thread.o thread_entry_wrapper.o \
	cpu_idle.o  fast_irq.o  fatal.o  fault.o  \
	fault_s.o  irq_manage.o  \
	isr_wrapper.o  regular_irq.o  swap_macros.h  swap.o \
	sys_fatal_error_handler.o

obj-y += prep_c.o \
         reset.o \
         vector_table.o

obj-$(CONFIG_IRQ_OFFLOAD) += irq_offload.o
obj-$(CONFIG_ERRNO) += errno.o

ifeq ($(CONFIG_ARC_ATOMIC_ASM),y)
obj-y += atomic.o
else
obj-y += atomic_native.o
endif

obj-$(CONFIG_IRQ_VECTOR_TABLE_BSP) += irq_vector_table.o
obj-$(CONFIG_SW_ISR_TABLE) += sw_isr_table.o
