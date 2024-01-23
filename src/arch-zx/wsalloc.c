// Copyright 2023,24 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

#include "wsalloc.h"

unsigned int free_spaces() __z88dk_fastcall __naked {
// see also https://skoolkid.github.io/rom/asm/1F1A.html
__asm
    ld bc, 0
    rst 0x18
    dw 0x1f05   ; https://skoolkid.github.io/rom/asm/1F05.html
    ; HL    STKEND+BC+80-SP (free space - 1) CF=1
    ex de, hl
    ld hl, 0xffff
    sbc hl, de
    ret
__endasm;
}

// allocate COUNT bytes from BASIC's workspace
// NOTE: code will terminate and exit to BASIC if there is
// not enough space between STKEND and RAMTOP
unsigned int bc_spaces(unsigned int count) __z88dk_fastcall __naked {
    // https://skoolkid.github.io/rom/asm/0030.html
__asm
    push hl
    pop bc
    ; BC    Number of free locations to create
    rst 0x18
    dw 0x0030
    ; DE    Address of the first byte of new free space
    ; HL    Address of the last byte of new free space
    push de
    pop hl
    ret
__endasm;
}

void *allocate_buffer(unsigned int size) __smallc __z88dk_callee {
    unsigned int buf = 0;   // return value
    unsigned int pos;       // potential buffer address
    // https://worldofspectrum.org/ZXBasicManual/zxmanchap24.html
    unsigned int WORKSP, STKEND, RAMTOP, UDG, P_RAMT;

    WORKSP = z80_wpeek(0x5c61);
    STKEND = z80_wpeek(0x5c65);
    RAMTOP = z80_wpeek(0x5cb2);
    UDG    = z80_wpeek(0x5c7b);
    P_RAMT = z80_wpeek(0x5cb4);

    // watch for overlows! 65535+1=0 for unsigned int!!!
    debugpf("WORKSP: %x STKEND: %x RAMTOP: %x UDG: %x P_RAMT: %x SPACES: %x\n",
        WORKSP, STKEND, RAMTOP, UDG, P_RAMT, free_spaces());

    // R_RAMT points to the last byte, len = last_byte + 1
    pos = P_RAMT-BUFFER_SIZE+1;
    debugpf("pos: %x %u\n", pos, pos);
    // do we have enough memory above RAMTOP? it's uncontended by definition
    if (!wsonly && RAMTOP < pos) {
        // there MAY be enough bytes between RAMTOP and P_RAMT
        debugpf("RAMTOP %u UDG %u, has free mem %u\n", RAMTOP, UDG, UDG-RAMTOP);
        // it would be MUCH easier if we let ourselves trash UDGs
        if (UDG > RAMTOP) {
            unsigned int below_udg = UDG-BUFFER_SIZE;
            if ((below_udg-1)>RAMTOP) {
                buf = below_udg; // RAMTOP -> buf -> UDG
            } else if (pos-8*21>UDG) { // 21 UDG's
                buf = pos; // UDG+168 -> buf -> P_RAMT
            }
        } else {
            buf = pos;
        }
        // it looks like there is no point trying WORKSPACE here, there won't
        // be 16384 (BUFFER_SIZE) between 0x8000 and RAMTOP
    } else {
        // data will be allocated at WORKSP, we need it to reach uncontended area
        unsigned int offset = WORKSP < 0x8000 ? 0x8000-WORKSP : 0;
        debugpf("BC_SPACES %u %u %u\n", WORKSP, STKEND, offset);
        // does it fit in the spare area AND can it be in the uncontended RAM?
        if (free_spaces() > BUFFER_SIZE+offset) {
            buf = bc_spaces(BUFFER_SIZE+offset) + offset;
        }
    }
    return buf;
}

#ifdef DEBUG
void dbg_dump_mem_vars(void) __z88dk_fastcall {
    unsigned int spaces = free_spaces();
    printf("WORKSP: %u\x06STKBOT: %u\nSTKEND: %u\x06RAMTOP: %u\n",
        z80_wpeek(0x5c61), z80_wpeek(0x5c63), z80_wpeek(0x5c65), z80_wpeek(0x5cb2));
    printf("FREE SPACES: %x %u\n", spaces, spaces);
}
#endif // DEBUG

// EOF vim: ts=4:sw=4:et:ai:
