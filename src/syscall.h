#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_PUT_STRING    0
#define SYS_BEEP          1
#define SYS_SET_PRG_BANK  2

#define SYS_START_PROCESS 11
#define SYS_IPC_WRITE     12
#define SYS_IPC_GET       13
#define SYS_EXIT_PROCESS  14
#define SYS_ALLOC         15
#define SYS_FREE          16

/* Syscall registers in zero page.
   Caller fills sc_num and sc_p0..p5 before BRK.
   Syscall handler writes results into sc_rv0..rv3. */
extern unsigned char sc_num;
extern unsigned char sc_p0;
extern unsigned char sc_p1;
extern unsigned char sc_p2;
extern unsigned char sc_p3;
extern unsigned char sc_p4;
extern unsigned char sc_p5;
extern unsigned char sc_rv0;
extern unsigned char sc_rv1;
extern unsigned char sc_rv2;
extern unsigned char sc_rv3;

#pragma zpsym ("sc_num")
#pragma zpsym ("sc_p0")
#pragma zpsym ("sc_p1")
#pragma zpsym ("sc_p2")
#pragma zpsym ("sc_p3")
#pragma zpsym ("sc_p4")
#pragma zpsym ("sc_p5")
#pragma zpsym ("sc_rv0")
#pragma zpsym ("sc_rv1")
#pragma zpsym ("sc_rv2")
#pragma zpsym ("sc_rv3")

/* Split a 16-bit pointer into lo/hi bytes. */
#define PTR_UNPACK(ptr, lo, hi) \
    (lo) = (unsigned char)((unsigned int)(ptr) & 0xFF); \
    (hi) = (unsigned char)((unsigned int)(ptr) >> 8)

/* Basic syscall macros. */
#define SC_PRINT(x, y, ptr) \
    sc_num = SYS_PUT_STRING; \
    sc_p0  = (unsigned char)(x); \
    sc_p1  = (unsigned char)(y); \
    PTR_UNPACK((ptr), sc_p2, sc_p3); \
    __asm__("brk #%b", 0)

#define SC_BEEP(freq, dur) \
    sc_num = SYS_BEEP; \
    sc_p0  = (unsigned char)(freq); \
    sc_p1  = (unsigned char)(dur); \
    __asm__("brk #%b", 0)

#define SC_SET_PRG_BANK(bank) \
    sc_num = SYS_SET_PRG_BANK; \
    sc_p0  = (unsigned char)(bank); \
    __asm__("brk #%b", 0)

#endif
