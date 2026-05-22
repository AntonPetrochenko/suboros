.import _nmi_ready
.import _frame_count

.export nmi_handler, irq_handler

.segment "CODE"

.proc nmi_handler
    pha
    txa
    pha
    tya
    pha

    inc _frame_count
    bne :+
    inc _frame_count+1
:
    lda #1
    sta _nmi_ready

    pla
    tay
    pla
    tax
    pla
    rti
.endproc

.proc irq_handler
    rti
.endproc
