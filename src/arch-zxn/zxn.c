
// https://www.specnext.com/forum/viewtopic.php?p=13725
unsigned char zx_model_next() __z88dk_fastcall __preserves_regs(bc,de,ix,iy) __naked {
__asm
    ld  a,$80
    DB  $ED, $24, $00, $00   ; mirror a : nop : nop
    and 1
    ld  l,a
    ret
__endasm;
}

unsigned char ZXN_READ_REG(unsigned char reg) __z88dk_fastcall __preserves_regs(af,de,ix,iy) __naked {
// 1. Load subset of DEHL with single argument (L if 8-bit, HL if 16-bit, DEHL if 32-bit)
// 2. call function
// 3. return value is in a subset of DEHL
__asm
    ld bc, 0x243b   ; https://wiki.specnext.dev/TBBlue_Register_Select
    out (c), l
    inc b ; 0x253b  ; https://wiki.specnext.dev/TBBlue_Register_Access
    in l, (c)
    ret
__endasm;
}

void ZXN_WRITE_REG_callee(unsigned char reg, unsigned char val) __smallc __z88dk_callee __preserves_regs(af,ix,iy) __naked {
__asm
    pop hl ; return address
    pop de ; val
    ex (sp), hl ; reg
    ld bc, 0x243b   ; https://wiki.specnext.dev/TBBlue_Register_Select  
    out (c), l
    inc b ; 0x253b  ; https://wiki.specnext.dev/TBBlue_Register_Access
    out (c), e
    ret
__endasm;
}

// EOF vim: ts=4:sw=4:et:ai:
