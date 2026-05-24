#include "syscall.h"
#include "fs.h"
#include "proc.h"

ProcEntry     proc_table[PROC_MAX];
unsigned char proc_user_count;

/* Hardware stack base SPs for each slot (slot 0 = kernel, uses $FF). */
static const unsigned char proc_base_sp[PROC_MAX] = { 0xFF, 0xBF, 0x7F, 0x3F };

void proc_init(void) {
    unsigned char i;
    for (i = 0; i < PROC_MAX; i++) {
        proc_table[i].flags    = 0;
        proc_table[i].saved_sp = 0;
        proc_table[i].rom_bank = 0;
        proc_table[i].pad      = 0;
        proc_table[i].ipc[0]   = 0;
        proc_table[i].ipc[1]   = 0;
        proc_table[i].ipc[2]   = 0;
        proc_table[i].ipc[3]   = 0;
    }
    /* Kernel is always active in slot 0. */
    proc_table[0].flags    = PROC_FLAG_ACTIVE;
    proc_table[0].rom_bank = 0;   /* kernel runs in fixed bank 7; $8000 window unused */
    proc_user_count = 0;
    /* _sched_cur_pid is in ZP, initialised to 0 by the cc65 runtime BSS clear. */
}

/* -----------------------------------------------------------------------
 * SYS_START_PROCESS (11)
 *
 * sc_p0 = mount slot
 * sc_p1 = name ptr lo
 * sc_p2 = name ptr hi
 * → sc_rv0 = PID (1-3) or $FF on error
 * ----------------------------------------------------------------------- */
void start_process(void) {
    unsigned char slot, name_lo, name_hi;
    unsigned char va[8];
    unsigned char hdr[8];
    unsigned char pid, i;
    unsigned char bank;
    unsigned int  entry_off;
    SuborFS1Extent *ext;
    volatile unsigned char *hw_stack = (volatile unsigned char *)0x0100;
    unsigned char base;
    unsigned int  ep;

    slot    = sc_p0;
    name_lo = sc_p1;
    name_hi = sc_p2;

    /* Find a free user process slot (1-3). */
    pid = 0xFF;
    for (i = PROC_USER_FIRST; i <= PROC_USER_LAST; i++) {
        if (!(proc_table[i].flags & PROC_FLAG_ACTIVE)) {
            pid = i;
            break;
        }
    }
    if (pid == 0xFF) {
        sc_rv0 = 0xFF;
        sc_rv1 = 0x01;  /* E1: no free process slot */
        return;
    }

    /* Open the file (direct C call, not nested BRK). */
    sc_p0 = slot;
    sc_p1 = name_lo;
    sc_p2 = name_hi;
    PTR_UNPACK(va, sc_p3, sc_p4);
    fs_open();
    if (sc_rv0 == FS_SLOT_EMPTY) {
        sc_rv0 = 0xFF;
        sc_rv1 = 0x02;  /* E2: file not found */
        return;
    }

    /* Grab the first extent's bank while the handle is still open. */
    {
        unsigned char h = va[0];
        if (fs_handle_table[h].mount_slot == FS_HANDLE_EMPTY) {
            sc_rv0 = 0xFF;
            sc_rv1 = 0x03;  /* E3: bad handle after open */
            return;
        }
        ext = (SuborFS1Extent *)
            ((unsigned int)fs_handle_table[h].extents_hi << 8 |
                           fs_handle_table[h].extents_lo);
        bank = ext->bank;
    }

    /* Read the 8-byte PRG header into hdr[]. */
    PTR_UNPACK(va,  sc_p0, sc_p1);
    PTR_UNPACK(hdr, sc_p2, sc_p3);
    sc_p4 = 8;
    sc_p5 = 0;
    fs_read();
    if (sc_rv0 < 8) {
        PTR_UNPACK(va, sc_p0, sc_p1);
        fs_close();
        sc_rv0 = 0xFF;
        sc_rv1 = 0x04;  /* E4: header read returned < 8 bytes */
        return;
    }

    /* Close the file handle. */
    PTR_UNPACK(va, sc_p0, sc_p1);
    fs_close();

    /* Verify PRG magic 'P', 'R'. */
    if (hdr[0] != 'P' || hdr[1] != 'R') {
        sc_rv0 = 0xFF;
        sc_rv1 = 0x05;  /* E5: bad PRG magic */
        return;
    }

    /* entry_offset (bytes 4-5, LE). */
    entry_off = (unsigned int)hdr[5] << 8 | (unsigned int)hdr[4];

    /* Set up initial stack frame in the slot's hardware stack region.
     * Frame layout (top-to-bottom in memory):
     *   hw_stack[base]   = PCH
     *   hw_stack[base-1] = PCL
     *   hw_stack[base-2] = P (flags, 0)
     *   hw_stack[base-3] = A (0)
     *   hw_stack[base-4] = X (0)
     *   hw_stack[base-5] = Y (0)
     * saved_sp = base - 6 (SP points to last-pushed byte).
     */
    base = proc_base_sp[pid];
    ep   = 0x8000u + entry_off;

    hw_stack[base]     = (unsigned char)(ep >> 8);
    hw_stack[base - 1] = (unsigned char)(ep & 0xFF);
    hw_stack[base - 2] = 0;   /* P */
    hw_stack[base - 3] = 0;   /* A */
    hw_stack[base - 4] = 0;   /* X */
    hw_stack[base - 5] = 0;   /* Y */

    proc_table[pid].saved_sp = base - 6;
    proc_table[pid].rom_bank = bank;
    proc_table[pid].flags    = PROC_FLAG_ACTIVE;
    proc_table[pid].ipc[0]   = 0;
    proc_table[pid].ipc[1]   = 0;
    proc_table[pid].ipc[2]   = 0;
    proc_table[pid].ipc[3]   = 0;

    proc_user_count++;
    sc_rv0 = pid;
    sc_rv1 = 0;
}

/* -----------------------------------------------------------------------
 * SYS_IPC_WRITE (12)
 *
 * sc_p0 = target PID, sc_p1..p4 = 4 data bytes
 * ----------------------------------------------------------------------- */
void ipc_write(void) {
    unsigned char pid = sc_p0;
    if (pid >= PROC_MAX || !(proc_table[pid].flags & PROC_FLAG_ACTIVE)) return;
    proc_table[pid].ipc[0] = sc_p1;
    proc_table[pid].ipc[1] = sc_p2;
    proc_table[pid].ipc[2] = sc_p3;
    proc_table[pid].ipc[3] = sc_p4;
}

/* -----------------------------------------------------------------------
 * SYS_IPC_GET (13)
 *
 * sc_p0 = source PID → sc_rv0..rv3 = ipc[0..3]
 * ----------------------------------------------------------------------- */
void ipc_get(void) {
    unsigned char pid = sc_p0;
    if (pid >= PROC_MAX || !(proc_table[pid].flags & PROC_FLAG_ACTIVE)) {
        sc_rv0 = sc_rv1 = sc_rv2 = sc_rv3 = 0xFF;
        return;
    }
    sc_rv0 = proc_table[pid].ipc[0];
    sc_rv1 = proc_table[pid].ipc[1];
    sc_rv2 = proc_table[pid].ipc[2];
    sc_rv3 = proc_table[pid].ipc[3];
}
