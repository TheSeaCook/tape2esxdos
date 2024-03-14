; Copyright 2023 TIsland Crew
; SPDX-License-Identifier: Apache-2.0
    org 0xB000
unpack:
    ld hl, data     ; source
    ld de, 0x8000   ; destination
    push de
dzx0:
    include "dzx0_standard.asm"

data:
; EOF vim: et:ai:ts=4:sw=4:
