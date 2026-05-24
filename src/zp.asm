; CC65 runtime zero-page variable definitions.
; Names and sizes must match what cc65 imports.

.exportzp c_sp, sreg, regsave, regbank
.exportzp tmp1, tmp2, tmp3, tmp4
.exportzp ptr1, ptr2, ptr3, ptr4

.segment "ZEROPAGE"

c_sp:    .res 2
sreg:    .res 2
regsave: .res 4
regbank: .res 6
ptr1:    .res 2
ptr2:    .res 2
ptr3:    .res 2
ptr4:    .res 2
tmp1:    .res 1
tmp2:    .res 1
tmp3:    .res 1
tmp4:    .res 1

.exportzp _sc_num, _sc_p0, _sc_p1, _sc_p2, _sc_p3, _sc_p4, _sc_p5
.exportzp _sc_rv0, _sc_rv1, _sc_rv2, _sc_rv3

_sc_num: .res 1
_sc_p0:  .res 1
_sc_p1:  .res 1
_sc_p2:  .res 1
_sc_p3:  .res 1
_sc_p4:  .res 1
_sc_p5:  .res 1
_sc_rv0: .res 1
_sc_rv1: .res 1
_sc_rv2: .res 1
_sc_rv3: .res 1

.exportzp _sched_cur_pid, _sched_tmp, _sched_ptr, _no_sched

_sched_cur_pid: .res 1   ; currently running PID (0=kernel, 1-3=user)
_sched_tmp:     .res 1   ; scratch byte for scheduler
_sched_ptr:     .res 2   ; ZP pointer used for indirect proc_table access
_no_sched:      .res 1   ; non-zero = do not context-switch this NMI frame

; User-program scratch ZP (not used by OS C code — safe across context switches).
; User programs must use ONLY these for ZP storage; never touch CC65 runtime vars.
.export _usr_ptr, _usr_tmp
_usr_ptr: .res 2         ; 2-byte ZP pointer for indirect addressing ($2A-$2B)
_usr_tmp: .res 4         ; general scratch bytes ($2C-$2F)
