.export _ascii_chr_data

.segment "RODATA"

_ascii_chr_data:
    .incbin "../chr/ascii.chr"
