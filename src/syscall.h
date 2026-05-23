#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_PUT_STRING 0
#define SYS_BEEP       1

/* Syscall parameter registers — five sequential zero-page bytes.
   Caller fills these before executing BRK.
   sc_num: syscall number (SC_* constant above)
   sc_p0..sc_p3: general-purpose parameters (meaning is syscall-specific) */
extern unsigned char sc_num;
extern unsigned char sc_p0;
extern unsigned char sc_p1;
extern unsigned char sc_p2;
extern unsigned char sc_p3;

#define PTR_UNPACK(ptr, lo, hi) \
    (lo) = (unsigned char)((unsigned int)(ptr) & 0xFF); \
    (hi) = (unsigned char)((unsigned int)(ptr) >> 8)

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

/* Tell cc65 these live in zero page so it can use ZP addressing modes. */
#pragma zpsym ("sc_num")
#pragma zpsym ("sc_p0")
#pragma zpsym ("sc_p1")
#pragma zpsym ("sc_p2")
#pragma zpsym ("sc_p3")

#endif
