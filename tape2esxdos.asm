SECTION code_clib
SECTION code_arch

PUBLIC esxdos_zx_tape_load_block
PUBLIC esxdos_zx_tape_load_block_callee

EXTERN _GLOBAL_ZX_PORT_FE
EXTERN asm_cpu_push_di, asm_cpu_pop_ei
EXTERN error_znc, error_mc

esxdos_zx_tape_load_block:

   pop af
   pop bc
   pop de
   pop ix

   push de
   push de
   push bc
   push af

   ld a,c
   jp asm_zx_tape_load_block_esxdos

esxdos_zx_tape_load_block_callee:

   pop af
   pop bc
   pop de
   pop ix
   push af

   ld a,c
;;   jp asm_zx_tape_load_block_esxdos

;;asm_zx_tape_verify_block_esxdos:
;;
;;   or a
;;   jr asm_zx_tape_load_block_rejoin

asm_zx_tape_load_block_esxdos:

   ; enter : ix = destination address
   ;         de = block length
   ;          a = 0 (header block), 0xff (data block)
   ;
   ; exit  : success
   ;
   ;            hl = 0, carry reset
   ;
   ;         failure
   ;
   ;            hl = -1, carry set
   ;
   ; uses  : af, bc, de, hl, ix, af'

   scf

;;asm_zx_tape_load_block_rejoin:

   inc d                       ; set nz flag
   ex af,af'
   dec d

   call asm_cpu_push_di        ; push ei/di state, di

   ld a,$0f
   out ($fe),a                 ; make border white

; enable with -Ca-DT2ESX_TURBO
IF T2ESX_TURBO
  call LD_BYTES
else
if __ESXDOS_DOT_COMMAND
   IN A,($FE)			; code at 0x562, correct entry point
   rst $18
   dw 0x0564
else
   ld hl,exit
   push hl

   jp 0x0562                   ; rom tape load, trapped by divmmc
exit:
endif ; __ESXDOS_DOT_COMMAND
endif ; T2ESX_TURBO

   ex af,af'                   ; carry flag set on success

   ld a,(_GLOBAL_ZX_PORT_FE)
   out ($fe),a                 ; restore border colour

   call asm_cpu_pop_ei         ; ei, ret

   ex af,af'

   jp c, error_znc
   jp error_mc

if T2ESX_TURBO
#include "turbo-loader.asm"
endif ; T2ESX_TURBO

; EOF
