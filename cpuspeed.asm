; Copyright 2023,24 TIsland Crew
; SPDX-License-Identifier: Apache-2.0
SECTION code_user

PUBLIC cpu_speed_callee

EXTERN asm_cpu_push_di, asm_z80_pop_ei_jp

#undef PRESEVE_REG_I

; FIXME: contrary to the "popular maths" below, 6 seems to be
; enough for Sizif (there are some known bugs in current rev)
; IRQ handler duration: 34 + 13*(slowdown_delay-1)
initial_delay   equ     1

irq_vec    equ        0xc000
irq        equ        0xc1c1
assert irq / 256 == irq % 256

cpu_speed_callee:       ; uses i, af, bc, de, hl
        call asm_cpu_push_di
        ; setup irq vector table
        ld hl, irq_vec
        ld de, irq_vec+1
        ld bc, 256
        ld (hl), irq/256
        ldir
        ; setup handler routine
        ld hl, irq
        ld (hl), 0xc3 ; JP
        inc hl
        ld (hl), interrupt%256
        inc hl
        ld (hl), interrupt/256
        ; init Mode 2
ifdef PRESEVE_REG_I
        ld a, i
        push af
endif ; PRESEVE_REG_I
        ld a, irq_vec/256
        ld i, a
        im 2
        ; counters/delays config
        ld c, initial_delay
_restart:
        xor a   ; we start with 0
        ld hl, 0 ; our counter
        ei
        halt    ; handler increases A, now we have 1
_measure:
        inc hl
        cp 1    ; if we still have 1, wait and count
        jp z, _measure
        di
        xor a
        cp h
        jr nz,_exit
        inc c           ; underflow? increase delay +13T
ifdef DEBUG
        ld a, 0b10111000 ; FLASH + WHITE bg BLACK fg
        ld (16384+6144),a
endif
        jr _restart
_exit:
ifdef DEBUG
        ld a, c
        or 0b10000000
        ld (16384+6145), a
endif
ifdef PRESEVE_REG_I
        pop af
        ld i,a
endif ; PRESEVE_REG_I
        im 1

        jp asm_z80_pop_ei_jp

; Z80 needs /INT for at least 23 T-states (from Chris Smith's
; book) ULA holds /INT for 32 T-states (p.226), but those are
; "physical" T-states, ULA derives those from the VSync signal
; (+ V2-0 + C8-6) and those aren't changed, right?
; Hence, at the highest CPU clock we need to spend enough time
; in the interrupt handler to let "ULA" drop the /INT

; and all the above doesn't seem to apply to Next, it appears
; to be reducing /INT duration according to the CPU clock
; but we use Next's own API to read clock frequency there anyway

interrupt:      ; 4+4+8+13*(C-1)+4+14 = 34 + 13*(C-1)
        inc a           ; 4
        ld b, c         ; 4
_delay:
        djnz _delay     ; 13/8
        ei              ; 4
        reti            ; 14

; EOF vim: et:ts=8:sw=8:ai
