/*
 *  BSP startup
 *
 *  This routine starts the application.  It includes application,
 *  board, and monitor specific initialization and configuration.
 *  The generic CPU dependent initialization has been performed
 *  before this routine is invoked.
 *
 *  Author:
 *    David Fiddes, D.J@fiddes.surfaid.org
 *    http://www.calm.hw.ac.uk/davidf/coldfire/
 *
 *  COPYRIGHT (c) 1989-1998.
 *  On-Line Applications Research Corporation (OAR).
 *  Copyright assigned to U.S. Government, 1994.
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *
 *  http://www.OARcorp.com/rtems/license.html.
 * 
 *  $Id$
 */

#include <bsp.h>
#include <rtems/libio.h>
#include <rtems/libcsupport.h>
#include <string.h>
#include <errno.h>
 
/*
 *  The original table from the application and our copy of it with
 *  some changes.
 */
extern rtems_configuration_table Configuration;
rtems_configuration_table  BSP_Configuration;
rtems_cpu_table Cpu_table;
char *rtems_progname;

/*
 * Location of 'VME' access
 */
#define VME_ONE_BASE    0x30000000
#define VME_TWO_BASE    0x31000000

/*
 * Cacheable areas
 */
#define SDRAM_BASE      0
#define SDRAM_SIZE      (16*1024*1024)
#define FLASH_BASE      0x10000000
#define FLASH_SIZE      (4*1024*1024)

/*
 * CPU-space access
 */
#define m68k_set_cacr(_cacr) asm volatile ("movec %0,%%cacr" : : "d" (_cacr))
#define m68k_set_acr0(_acr0) asm volatile ("movec %0,%%acr0" : : "d" (_acr0))
#define m68k_set_acr1(_acr1) asm volatile ("movec %0,%%acr1" : : "d" (_acr1))

/*
 * Read/write copy of common cache
 *   Split I/D cache
 *   Allow CPUSHL to invalidate a cache line
 *   Enable buffered writes
 *   No burst transfers on non-cacheable accesses
 *   Default cache mode is *disabled* (cache only ACRx areas)
 */
static uint32_t cacr_mode = MCF5XXX_CACR_CENB |
                              MCF5XXX_CACR_DBWE |
                              MCF5XXX_CACR_DCM;
/*
 * Cannot be frozen
 */
void _CPU_cache_freeze_data(void) {}
void _CPU_cache_unfreeze_data(void) {}
void _CPU_cache_freeze_instruction(void) {}
void _CPU_cache_unfreeze_instruction(void) {}

/*
 * Write-through data cache -- flushes are unnecessary
 */
void _CPU_cache_flush_1_data_line(const void *d_addr) {}
void _CPU_cache_flush_entire_data(void) {}

void _CPU_cache_enable_instruction(void)
{
    rtems_interrupt_level level;

    rtems_interrupt_disable(level);
    cacr_mode &= ~MCF5XXX_CACR_DIDI;
    m68k_set_cacr(cacr_mode);
    rtems_interrupt_enable(level);
}

void _CPU_cache_disable_instruction(void)
{
    rtems_interrupt_level level;

    rtems_interrupt_disable(level);
    cacr_mode |= MCF5XXX_CACR_DIDI;
    m68k_set_cacr(cacr_mode);
    rtems_interrupt_enable(level);
}

void _CPU_cache_invalidate_entire_instruction(void)
{
    m68k_set_cacr(cacr_mode | MCF5XXX_CACR_CINV | MCF5XXX_CACR_INVI);
}

void _CPU_cache_invalidate_1_instruction_line(const void *addr)
{
    /*
     * Top half of cache is I-space
     */
    addr = (void *)((int)addr | 0x400);
    asm volatile ("cpushl %%bc,(%0)" :: "a" (addr));
}

void _CPU_cache_enable_data(void)
{
    rtems_interrupt_level level;

    rtems_interrupt_disable(level);
    cacr_mode &= ~MCF5XXX_CACR_DISD;
    m68k_set_cacr(cacr_mode);
    rtems_interrupt_enable(level);
}

void _CPU_cache_disable_data(void)
{
    rtems_interrupt_level level;

    rtems_interrupt_disable(level);
    rtems_interrupt_disable(level);
    cacr_mode |= MCF5XXX_CACR_DISD;
    m68k_set_cacr(cacr_mode);
    rtems_interrupt_enable(level);
}

void _CPU_cache_invalidate_entire_data(void)
{
    m68k_set_cacr(cacr_mode | MCF5XXX_CACR_CINV | MCF5XXX_CACR_INVD);
}

void _CPU_cache_invalidate_1_data_line(const void *addr)
{
    /*
     * Bottom half of cache is D-space
     */
    addr = (void *)((int)addr & ~0x400);
    asm volatile ("cpushl %%bc,(%0)" :: "a" (addr));
}

/*
 *  Use the shared implementations of the following routines
 */
void bsp_postdriver_hook(void);
void bsp_libc_init( void *, uint32_t, int );
void bsp_pretasking_hook(void);         /* m68k version */

/*
 *  bsp_start
 *
 *  This routine does the bulk of the system initialisation.
 */
void bsp_start( void )
{
  extern char _WorkspaceBase[];
  extern char _RamSize[];
  extern unsigned long  _M68k_Ramsize;

  _M68k_Ramsize = (unsigned long)_RamSize;      /* RAM size set in linker script */

  /*
   *  Allocate the memory for the RTEMS Work Space.  This can come from
   *  a variety of places: hard coded address, malloc'ed from outside
   *  RTEMS world (e.g. simulator or primitive memory manager), or (as
   *  typically done by stock BSPs) by subtracting the required amount
   *  of work space from the last physical address on the CPU board.
   */

  /*
   *  Need to "allocate" the memory for the RTEMS Workspace and
   *  tell the RTEMS configuration where it is.  This memory is
   *  not malloc'ed.  It is just "pulled from the air".
   */

  BSP_Configuration.work_space_start = (void *)_WorkspaceBase;

  /*
   *  initialize the CPU table for this BSP
   */
  Cpu_table.pretasking_hook = bsp_pretasking_hook;  /* init libc, etc. */
  Cpu_table.postdriver_hook = bsp_postdriver_hook;
  Cpu_table.do_zero_of_workspace = TRUE;
  Cpu_table.interrupt_stack_size = 4096;

  Cpu_table.interrupt_vector_table = (m68k_isr *)0; /* vectors at start of RAM */

    /*
     * Invalidate the cache and disable it
     */
    m68k_set_acr0(0);
    m68k_set_acr1(0);
    m68k_set_cacr(MCF5XXX_CACR_CINV);

    /*
     * Cache SDRAM and FLASH
     */
    m68k_set_acr0(MCF5XXX_ACR_AB(SDRAM_BASE)    |
                  MCF5XXX_ACR_AM(SDRAM_SIZE-1)  |
                  MCF5XXX_ACR_EN                |
                  MCF5XXX_ACR_BWE               |
                  MCF5XXX_ACR_SM_IGNORE);
    m68k_set_acr1(MCF5XXX_ACR_AB(FLASH_BASE)    |
                  MCF5XXX_ACR_AM(FLASH_SIZE-1)  |
                  MCF5XXX_ACR_EN                |
                  MCF5XXX_ACR_BWE               |
                  MCF5XXX_ACR_SM_IGNORE);

    /*
     * Enable the cache
     */
    m68k_set_cacr(cacr_mode);

    /*
     * Set up CS* space (fake 'VME')
     *   Two A24/D16 spaces, supervisor data acces
     */
    MCF5282_CS1_CSAR = MCF5282_CS_CSAR_BA(VME_ONE_BASE);
    MCF5282_CS1_CSMR = MCF5282_CS_CSMR_BAM_16M | 
                       MCF5282_CS_CSMR_CI |
                       MCF5282_CS_CSMR_SC |
                       MCF5282_CS_CSMR_UC |
                       MCF5282_CS_CSMR_UD |
                       MCF5282_CS_CSMR_V;
    MCF5282_CS1_CSCR = MCF5282_CS_CSCR_PS_16;
    MCF5282_CS2_CSAR = MCF5282_CS_CSAR_BA(VME_TWO_BASE);
    MCF5282_CS2_CSMR = MCF5282_CS_CSMR_BAM_16M | 
                       MCF5282_CS_CSMR_CI |
                       MCF5282_CS_CSMR_SC |
                       MCF5282_CS_CSMR_UC |
                       MCF5282_CS_CSMR_UD |
                       MCF5282_CS_CSMR_V;
    MCF5282_CS2_CSCR = MCF5282_CS_CSCR_PS_16;
}

uint32_t bsp_get_CPU_clock_speed(void)
{
    extern char _CPUClockSpeed[];
    return( (uint32_t)_CPUClockSpeed);
}

/*
 * Interrupt controller allocation
 */
rtems_status_code
bsp_allocate_interrupt(int level, int priority)
{
    static char used[7];
    rtems_interrupt_level l;
    rtems_status_code ret = RTEMS_RESOURCE_IN_USE;

    if ((level < 1) || (level > 7) || (priority < 0) || (priority > 7))
        return RTEMS_INVALID_NUMBER;
    rtems_interrupt_disable(l);
    if ((used[level-1] & (1 << priority)) == 0) {
        used[level-1] |= (1 << priority);
        ret = RTEMS_SUCCESSFUL;
    }
    rtems_interrupt_enable(l);
    return ret;
}

/*
 * Arcturus bootloader system calls
 */
#define syscall_return(type, ret)                      \
do {                                                   \
   if ((unsigned long)(ret) >= (unsigned long)(-64)) { \
      errno = -(ret);                                  \
      ret = -1;                                        \
   }                                                   \
   return (type)(ret);                                 \
} while (0)
#define syscall_1(type,name,d1type,d1)                      \
type uC5282_##name(d1type d1)                               \
{                                                           \
   long ret;                                                \
   register long __d1 __asm__ ("%d1") = (long)d1;           \
   __asm__ __volatile__ ("move.l %0,%%d0\n\t"               \
                         "trap #2\n\t"                      \
                         "move.l %%d0,%0"                   \
                         : "=g" (ret)                       \
                         : "d" (SysCode_##name), "d" (__d1) \
                         : "d0" );                          \
   syscall_return(type,ret);                                \
}
#define SysCode_gethwaddr    12 /* get hardware address */
#define SysCode_getbenv      14 /* get bootloader environment variable */
#define SysCode_setbenv      15 /* get bootloader environment variable */
syscall_1(unsigned const char *, gethwaddr, int, a)
syscall_1(const char *, getbenv, const char *, a)


/*
 * 'Extended BSP' routines
 * Should move to cpukit/score/cpu/m68k/cpu.c someday.
 */

rtems_status_code bspExtInit(void) { return RTEMS_SUCCESSFUL; }
int BSP_enableVME_int_lvl(unsigned int level) { return 0; }
int BSP_disableVME_int_lvl(unsigned int level) { return 0; }

/*
 * VME interrupt support
 */
#define NVECTOR 256

static struct handlerTab {
    BSP_VME_ISR_t func;
    void         *arg;
} handlerTab[NVECTOR];

BSP_VME_ISR_t
BSP_getVME_isr(unsigned long vector, void **pusrArg)
{
    if (vector >= NVECTOR)
        return (BSP_VME_ISR_t)NULL;
    if (pusrArg)
        *pusrArg = handlerTab[vector].arg;
    return handlerTab[vector].func;
}

static rtems_isr
trampoline (rtems_vector_number v)
{
    if (handlerTab[v].func) 
        (*handlerTab[v].func)(handlerTab[v].arg, (unsigned long)v);
}

int
BSP_installVME_isr(unsigned long vector, BSP_VME_ISR_t handler, void *usrArg)
{
    rtems_isr_entry old_handler;

    if (vector >= NVECTOR)
        return -1;
    handlerTab[vector].func = handler;
    handlerTab[vector].arg = usrArg;
    rtems_interrupt_catch(trampoline, vector, &old_handler);

    /*
     * Find an unused level/priority if this is an on-chip (INTC0)
     * source and this is the first time the source is being used.
     * Interrupt sources 1 through 7 are fixed level/priority
     */
    if ((vector >= 65) && (vector <= 127)) {
        int l, p;
        int source = vector - 64;
        rtems_interrupt_level level;
        static unsigned char installed[8];

        if (installed[source/8] & (1 << (source % 8)))
            return 0;
        installed[source/8] |= (1 << (source % 8));
        for (l = 1 ; l < 7 ; l++) {
            for (p = 0 ; p < 7 ; p++) {
                if ((source < 8)
                 || (bsp_allocate_interrupt(l,p) == RTEMS_SUCCESSFUL)) {
                    if (source < 8)
                        *(&MCF5282_INTC0_ICR1 + (source - 1)) = 
                                                       MCF5282_INTC_ICR_IL(l) |
                                                       MCF5282_INTC_ICR_IP(p);
                    rtems_interrupt_disable(level);
                    if (source >= 32)
                        MCF5282_INTC0_IMRH &= ~(1 << (source - 32));
                    else
                        MCF5282_INTC0_IMRL &= ~((1 << source) |
                                                MCF5282_INTC_IMRL_MASKALL);
                    rtems_interrupt_enable(level);
                    return 0;
                }
            }
        }
        return -1;
    }
    return 0;
}

int
BSP_removeVME_isr(unsigned long vector, BSP_VME_ISR_t handler, void *usrArg)
{
    if (vector >= NVECTOR)
        return -1;
    if ((handlerTab[vector].func != handler)
     || (handlerTab[vector].arg != usrArg))
        return -1;
    handlerTab[vector].func = (BSP_VME_ISR_t)NULL;
    return 0;
}

int
BSP_vme2local_adrs(unsigned am, unsigned long vmeaddr, unsigned long *plocaladdr)
{
    unsigned long offset;

    switch (am) {
    default:    return -1;
    case VME_AM_SUP_SHORT_IO: offset = 0x31000000; break; /* A16/D16 */
    case VME_AM_STD_SUP_DATA: offset = 0x30000000; break; /* A24/D16 */
    case VME_AM_EXT_SUP_DATA: return -1;                  /* A32/D32 */
    }
    *plocaladdr = vmeaddr + offset;
    return 0;
}
