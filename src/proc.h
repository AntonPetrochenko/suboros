#ifndef PROC_H
#define PROC_H

#include "syscall.h"

/* Process table: 4 entries × 8 bytes = 32 bytes in BSS.
 *
 * Slot 0 = kernel/OS (always active; fixed ROM bank 7; hardware stack $01C0-$01FF).
 * Slots 1-3 = user processes (hardware stack regions below: $0180/$0140/$0100).
 *
 * Hardware stack partition:
 *   Slot 0 (kernel): $01C0-$01FF  base SP = $FF
 *   Slot 1:          $0180-$01BF  base SP = $BF
 *   Slot 2:          $0140-$017F  base SP = $7F
 *   Slot 3:          $0100-$013F  base SP = $3F
 */

#define PROC_FLAG_ACTIVE  0x01
#define PROC_FLAG_RAM     0x02   /* reserved for future RAM-loaded processes */

#define PROC_MAX          4
#define PROC_USER_FIRST   1      /* first user-process slot */
#define PROC_USER_LAST    3

/* 8-byte process table entry (must stay 8 bytes — scheduler uses stride-8 indexing). */
typedef struct {
    unsigned char flags;     /* PROC_FLAG_* */
    unsigned char saved_sp;  /* hardware SP after A/X/Y/P/PCL/PCH pushed on interrupt */
    unsigned char rom_bank;  /* PRG ROM bank written to MMC1 $E000 for this process */
    unsigned char pad;
    unsigned char ipc[4];    /* per-process IPC registers, freely written by owner */
} ProcEntry;

extern ProcEntry      proc_table[PROC_MAX];
extern unsigned char  proc_user_count;   /* active user slots (1-3); 0 = no scheduling */

/* Called at boot to initialise slot 0 (kernel) and clear user slots. */
void proc_init(void);

/* Syscall handlers (called from IRQ dispatcher). */
void start_process(void);
void ipc_write(void);
void ipc_get(void);
/* exit_process() is implemented in nmi.asm — it must manipulate SP directly. */

/* --- Caller macros ---
 *
 * SC_START_PROCESS(slot, name_ptr)
 *   rv0 = PID (1-3), or $FF on error (table full / file not found / bad magic).
 *
 * SC_IPC_WRITE(pid, b0, b1, b2, b3)
 *   Write 4 IPC bytes into proc_table[pid].ipc[].
 *
 * SC_IPC_GET(pid)
 *   rv0..rv3 = proc_table[pid].ipc[0..3].
 *
 * SC_EXIT_PROCESS()
 *   Terminate the calling process; never returns.
 */

#define SC_START_PROCESS(slot, name_ptr) \
    sc_num = SYS_START_PROCESS; \
    sc_p0  = (unsigned char)(slot); \
    PTR_UNPACK((name_ptr), sc_p1, sc_p2); \
    __asm__("brk #%b", 0)

#define SC_IPC_WRITE(pid, b0, b1, b2, b3) \
    sc_num = SYS_IPC_WRITE; \
    sc_p0  = (unsigned char)(pid); \
    sc_p1  = (unsigned char)(b0); \
    sc_p2  = (unsigned char)(b1); \
    sc_p3  = (unsigned char)(b2); \
    sc_p4  = (unsigned char)(b3); \
    __asm__("brk #%b", 0)

#define SC_IPC_GET(pid) \
    sc_num = SYS_IPC_GET; \
    sc_p0  = (unsigned char)(pid); \
    __asm__("brk #%b", 0)

#define SC_EXIT_PROCESS() \
    sc_num = SYS_EXIT_PROCESS; \
    __asm__("brk #%b", 0)

#endif
