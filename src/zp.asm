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

.exportzp _sc_num, _sc_p0, _sc_p1, _sc_p2, _sc_p3

_sc_num: .res 1
_sc_p0:  .res 1
_sc_p1:  .res 1
_sc_p2:  .res 1
_sc_p3:  .res 1
