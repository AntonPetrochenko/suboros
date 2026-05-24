#include <string.h>
#include "nes.h"
#include "syscall.h"
#include "fs.h"

unsigned char      prg_bank_cur;
SuborFS1Mount      fs_mount_table[FS_MAX_MOUNTS];
SuborFS1Handle     fs_handle_table[FS_MAX_MOUNTS];

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

static SuborFS1Extent *handle_extents(unsigned char h) {
    return (SuborFS1Extent *)
        ((unsigned int)FS_HANDLE_TABLE[h].extents_hi << 8 |
                       FS_HANDLE_TABLE[h].extents_lo);
}

/* Resolve a 24-bit linear file offset to {extent_idx, pos_lo, pos_hi}.
   Returns 0 on success, 1 if offset is past EOF. */
static unsigned char resolve_offset(SuborFS1Extent *exts,
                                    unsigned char   off_lo,
                                    unsigned char   off_mid,
                                    unsigned char   off_hi,
                                    unsigned char  *out_ext,
                                    unsigned char  *out_pos_lo,
                                    unsigned char  *out_pos_hi) {
    unsigned long offset = ((unsigned long)off_hi  << 16) |
                           ((unsigned long)off_mid <<  8) |
                            (unsigned long)off_lo;
    unsigned char idx = 0;

    while (exts->bank != FS_SLOT_EMPTY) {
        unsigned int ext_size =
            ((unsigned int)exts->end_hi   << 8 | exts->end_lo) -
            ((unsigned int)exts->start_hi << 8 | exts->start_lo);
        if (offset < (unsigned long)ext_size) {
            *out_ext    = idx;
            *out_pos_lo = (unsigned char)(offset & 0xFF);
            *out_pos_hi = (unsigned char)((offset >> 8) & 0xFF);
            return 0;
        }
        offset -= ext_size;
        exts++;
        idx++;
    }
    return 1;
}

/* Copy n bytes from NES address src (bank) to dest.
   Skips bank switching for internal RAM or the fixed ROM bank. */
static void banked_copy(unsigned char  bank,
                        unsigned int   src,
                        unsigned char *dest,
                        unsigned int   n,
                        unsigned char  device) {
    volatile unsigned char *s = (volatile unsigned char *)src;
    unsigned char saved;

    if (device == 0 || bank == 7 || src >= 0xC000u) {
        while (n--) *dest++ = *s++;
        return;
    }

    saved = prg_bank_cur;
    __asm__("sei");
    mmc1_write_reg(0xE000, bank);
    while (n--) *dest++ = *s++;
    mmc1_write_reg(0xE000, saved);
    prg_bank_cur = saved;
    __asm__("cli");
}

/* Walk a TOC entry's extent list forward one full entry (name + extents + sentinel). */
static unsigned char *next_toc_entry(unsigned char *entry) {
    SuborFS1Extent *e = (SuborFS1Extent *)(entry + FS_NAME_LEN);
    while (e->bank != FS_SLOT_EMPTY) e++;
    return (unsigned char *)(e + 1);
}

/* -----------------------------------------------------------------------
 * fs_init
 * ----------------------------------------------------------------------- */
void fs_init(void) {
    unsigned char i;
    for (i = 0; i < FS_MAX_MOUNTS; i++) {
        FS_MOUNT_TABLE[i].device_num  = FS_SLOT_EMPTY;
        FS_HANDLE_TABLE[i].mount_slot = FS_HANDLE_EMPTY;
    }
}

/* -----------------------------------------------------------------------
 * SYS_FS_MOUNT (3)
 * ----------------------------------------------------------------------- */
void fs_mount(void) {
    unsigned char i;
    for (i = 0; i < FS_MAX_MOUNTS; i++) {
        if (FS_MOUNT_TABLE[i].device_num == FS_SLOT_EMPTY) {
            FS_MOUNT_TABLE[i].toc_lo     = sc_p0;
            FS_MOUNT_TABLE[i].toc_hi     = sc_p1;
            FS_MOUNT_TABLE[i].device_num = sc_p2;
            sc_rv0 = i;
            sc_rv3 = FS_ERR_OK;
            return;
        }
    }
    sc_rv0 = FS_SLOT_EMPTY;
    sc_rv3 = FS_ERR_BAD_SLOT;
}

/* -----------------------------------------------------------------------
 * SYS_FS_OPEN (4)
 * ----------------------------------------------------------------------- */
void fs_open(void) {
    unsigned char  slot  = sc_p0;
    const char    *query = (const char *)((unsigned int)sc_p2 << 8 | sc_p1);
    unsigned char *va    = (unsigned char *)((unsigned int)sc_p4 << 8 | sc_p3);
    unsigned char *entry;
    unsigned char  handle, i;

    if (slot >= FS_MAX_MOUNTS ||
        FS_MOUNT_TABLE[slot].device_num == FS_SLOT_EMPTY) {
        sc_rv0 = FS_SLOT_EMPTY;
        sc_rv3 = FS_ERR_BAD_SLOT;
        return;
    }

    handle = FS_HANDLE_EMPTY;
    for (i = 0; i < FS_MAX_MOUNTS; i++) {
        if (FS_HANDLE_TABLE[i].mount_slot == FS_HANDLE_EMPTY) {
            handle = i;
            break;
        }
    }
    if (handle == FS_HANDLE_EMPTY) {
        sc_rv0 = FS_SLOT_EMPTY;
        sc_rv3 = FS_ERR_NO_HANDLE;
        return;
    }

    entry = (unsigned char *)
        ((unsigned int)FS_MOUNT_TABLE[slot].toc_hi << 8 |
                       FS_MOUNT_TABLE[slot].toc_lo);

    while (entry[0] != '\0') {
        if (strncmp((const char *)entry, query, FS_NAME_LEN) == 0) {
            unsigned char *exts_ptr = entry + FS_NAME_LEN;

            FS_HANDLE_TABLE[handle].mount_slot = slot;
            FS_HANDLE_TABLE[handle].extents_lo =
                (unsigned char)((unsigned int)exts_ptr & 0xFF);
            FS_HANDLE_TABLE[handle].extents_hi =
                (unsigned char)((unsigned int)exts_ptr >> 8);

            va[0] = handle;
            va[1] = 0; va[2] = 0; va[3] = 0;
            va[4] = 0; va[5] = 0; va[6] = 0; va[7] = 0;

            sc_rv0 = handle;
            sc_rv3 = FS_ERR_OK;
            return;
        }
        entry = next_toc_entry(entry);
    }

    sc_rv0 = FS_SLOT_EMPTY;
    sc_rv3 = FS_ERR_NOT_FOUND;
}

/* -----------------------------------------------------------------------
 * SYS_FS_UNMOUNT (5)
 * ----------------------------------------------------------------------- */
void fs_unmount(void) {
    unsigned char slot = sc_p0;
    unsigned char i;

    if (slot >= FS_MAX_MOUNTS ||
        FS_MOUNT_TABLE[slot].device_num == FS_SLOT_EMPTY) {
        sc_rv0 = FS_SLOT_EMPTY;
        sc_rv3 = FS_ERR_BAD_SLOT;
        return;
    }

    for (i = 0; i < FS_MAX_MOUNTS; i++) {
        if (FS_HANDLE_TABLE[i].mount_slot == slot)
            FS_HANDLE_TABLE[i].mount_slot = FS_HANDLE_EMPTY;
    }

    FS_MOUNT_TABLE[slot].device_num = FS_SLOT_EMPTY;
    sc_rv0 = 0;
    sc_rv3 = FS_ERR_OK;
}

/* -----------------------------------------------------------------------
 * SYS_FS_STAT (6)
 * ----------------------------------------------------------------------- */
void fs_stat(void) {
    unsigned char  slot  = sc_p0;
    const char    *query = (const char *)((unsigned int)sc_p2 << 8 | sc_p1);
    unsigned char *entry;

    if (slot >= FS_MAX_MOUNTS ||
        FS_MOUNT_TABLE[slot].device_num == FS_SLOT_EMPTY) {
        sc_rv0 = FS_SLOT_EMPTY;
        sc_rv3 = FS_ERR_BAD_SLOT;
        return;
    }

    entry = (unsigned char *)
        ((unsigned int)FS_MOUNT_TABLE[slot].toc_hi << 8 |
                       FS_MOUNT_TABLE[slot].toc_lo);

    while (entry[0] != '\0') {
        if (strncmp((const char *)entry, query, FS_NAME_LEN) == 0) {
            SuborFS1Extent *e = (SuborFS1Extent *)(entry + FS_NAME_LEN);
            unsigned int total = 0;
            while (e->bank != FS_SLOT_EMPTY) {
                total += ((unsigned int)e->end_hi   << 8 | e->end_lo) -
                         ((unsigned int)e->start_hi << 8 | e->start_lo);
                e++;
            }
            sc_rv0 = (unsigned char)(total & 0xFF);
            sc_rv1 = (unsigned char)(total >> 8);
            sc_rv3 = FS_ERR_OK;
            return;
        }
        entry = next_toc_entry(entry);
    }

    sc_rv0 = FS_SLOT_EMPTY;
    sc_rv3 = FS_ERR_NOT_FOUND;
}

/* -----------------------------------------------------------------------
 * SYS_FS_CLOSE (7)
 * ----------------------------------------------------------------------- */
void fs_close(void) {
    unsigned char *va = (unsigned char *)((unsigned int)sc_p1 << 8 | sc_p0);
    unsigned char  h  = va[0];
    unsigned char  i;

    if (h >= FS_MAX_MOUNTS ||
        FS_HANDLE_TABLE[h].mount_slot == FS_HANDLE_EMPTY) {
        sc_rv0 = FS_SLOT_EMPTY;
        sc_rv3 = FS_ERR_BAD_HANDLE;
        return;
    }

    FS_HANDLE_TABLE[h].mount_slot = FS_HANDLE_EMPTY;
    for (i = 0; i < 8; i++) va[i] = 0;
    sc_rv0 = 0;
    sc_rv3 = FS_ERR_OK;
}

/* -----------------------------------------------------------------------
 * SYS_FS_SEEK (8)
 * ----------------------------------------------------------------------- */
void fs_seek(void) {
    unsigned char *va    = (unsigned char *)((unsigned int)sc_p1 << 8 | sc_p0);
    unsigned char  h     = va[0];
    SuborFS1Extent *exts;
    unsigned char ext_idx, pos_lo, pos_hi;

    if (h >= FS_MAX_MOUNTS ||
        FS_HANDLE_TABLE[h].mount_slot == FS_HANDLE_EMPTY) {
        sc_rv3 = FS_ERR_BAD_HANDLE;
        return;
    }

    exts = handle_extents(h);

    if (resolve_offset(exts, sc_p2, sc_p3, sc_p4,
                       &ext_idx, &pos_lo, &pos_hi) != 0) {
        sc_rv3 = FS_ERR_EOF;
        return;
    }

    va[1] = ext_idx;
    va[2] = pos_lo;
    va[3] = pos_hi;
    va[4] = sc_p2;
    va[5] = sc_p3;
    va[6] = sc_p4;
    sc_rv3 = FS_ERR_OK;
}

/* -----------------------------------------------------------------------
 * SYS_FS_READ (9)
 * ----------------------------------------------------------------------- */
void fs_read(void) {
    unsigned char *va   = (unsigned char *)((unsigned int)sc_p1 << 8 | sc_p0);
    unsigned char *dest = (unsigned char *)((unsigned int)sc_p3 << 8 | sc_p2);
    unsigned int   n    = (unsigned int)sc_p5 << 8 | sc_p4;

    unsigned char   h       = va[0];
    unsigned char   ext_idx = va[1];
    unsigned int    pos     = (unsigned int)va[3] << 8 | va[2];
    unsigned long   off24   = ((unsigned long)va[6] << 16) |
                              ((unsigned long)va[5] <<  8) |
                               (unsigned long)va[4];
    SuborFS1Extent *exts, *e;
    unsigned char   device;
    unsigned int    copied = 0;

    if (h >= FS_MAX_MOUNTS ||
        FS_HANDLE_TABLE[h].mount_slot == FS_HANDLE_EMPTY) {
        sc_rv0 = sc_rv1 = 0;
        sc_rv3 = FS_ERR_BAD_HANDLE;
        return;
    }

    device = FS_MOUNT_TABLE[FS_HANDLE_TABLE[h].mount_slot].device_num;
    exts   = handle_extents(h);
    e      = exts + ext_idx;

    while (n > 0 && e->bank != FS_SLOT_EMPTY) {
        unsigned int ext_start  = (unsigned int)e->start_hi << 8 | e->start_lo;
        unsigned int ext_end    = (unsigned int)e->end_hi   << 8 | e->end_lo;
        unsigned int ext_size   = ext_end - ext_start;
        unsigned int bytes_left = ext_size - pos;
        unsigned int chunk      = (n < bytes_left) ? n : bytes_left;

        banked_copy(e->bank, ext_start + pos, dest, chunk, device);

        dest   += chunk;
        copied += chunk;
        n      -= chunk;
        pos    += chunk;
        off24  += chunk;

        if (pos >= ext_size) {
            ext_idx++;
            pos = 0;
            e++;
        }
    }

    va[1] = ext_idx;
    va[2] = (unsigned char)(pos & 0xFF);
    va[3] = (unsigned char)(pos >> 8);
    va[4] = (unsigned char)(off24        & 0xFF);
    va[5] = (unsigned char)((off24 >> 8) & 0xFF);
    va[6] = (unsigned char)((off24 >>16) & 0xFF);

    sc_rv0 = (unsigned char)(copied & 0xFF);
    sc_rv1 = (unsigned char)(copied >> 8);
    sc_rv3 = (n == 0) ? FS_ERR_OK : FS_ERR_EOF;
}

/* -----------------------------------------------------------------------
 * SYS_FS_GETBYTE (10)
 * ----------------------------------------------------------------------- */
void fs_getbyte(void) {
    unsigned char buf;
    sc_p2 = (unsigned char)((unsigned int)&buf & 0xFF);
    sc_p3 = (unsigned char)((unsigned int)&buf >> 8);
    sc_p4 = 1;
    sc_p5 = 0;
    fs_read();
    if (sc_rv0 == 0) {
        sc_rv1 = FS_ERR_EOF;
        sc_rv3 = FS_ERR_EOF;
    } else {
        sc_rv0 = buf;
        sc_rv1 = FS_ERR_OK;
        sc_rv3 = FS_ERR_OK;
    }
}
