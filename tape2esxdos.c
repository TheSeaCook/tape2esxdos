// Copyright 2023 TIsland Crew
// SPDX-License-Identifier: Apache-2.0
#include <arch/zx.h>
#include <arch/zx/esxdos.h>
#include <errno.h>
#include <input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <z80.h>

#pragma printf "%s %u"
// we have only one atexit call
#pragma output CLIB_EXIT_STACK_SIZE = 1
#ifndef __ESXDOS_DOT_COMMAND
// make sure stack is below buffer area
#pragma output REGISTER_SP = 45055
#endif // __ESXDOS_DOT_COMMAND

/*
 * compile for esxDOS:
 * zcc +zx -vn -subtype=dot -startup=30 -clib=new -SO3 --opt-code-size esxdos_asm_zx_tape_load_block.asm s.c -o s -create-app
 *  clib=sdcc_iy doesn't work with ASM code
 * compile for 48/128:
 * zcc +zx -v -SO3 -startup=30 -clib=new --opt-code-size s.c && z88dk-appmake +zx -b a_CODE.bin --org 0x8000 -o a.tap
 */

// 4 chars only for the version tag
#ifdef T2ESX_TURBO
#define VER "t1.D"
#else
#define VER "v1.D"
#endif // T2ESX_TURBO

#define BTX_ID_MASK 0xF0u
#define BTX_ID_TYPE 0x80u
#define BTX_OPEN_ID 0x87u
#define BTX_CHUNK_ID 0x88u

#define BUFFER_SIZE 16384
#define RAM_ADDRESS 0xB000

#define buffer ((void *)RAM_ADDRESS)

#define ZXT_H_PROGRAM 0
#define ZXT_H_NUM_ARRAY 1
#define ZXT_H_CHAR_ARRAY 2
#define ZXT_H_CODE 3

static struct zxtapehdr hdr;
static unsigned char file;
static unsigned char overwrite;
#ifdef COMPARE_CHUNK_NAMES
static unsigned char tname[10];
#endif // COMPARE_CHUNK_NAMES

#if defined(T2ESX_TURBO) || defined(__ESXDOS_DOT_COMMAND)
extern unsigned char __LIB__ esxdos_zx_tape_load_block(void *dst,unsigned int len,unsigned char type) __smallc;
extern unsigned char __LIB__ esxdos_zx_tape_load_block_callee(void *dst,unsigned int len,unsigned char type) __smallc __z88dk_callee;
#define esxdos_zx_tape_load_block(a,b,c) esxdos_zx_tape_load_block_callee(a,b,c)
#else
#define esxdos_zx_tape_load_block(a,b,c) zx_tape_load_block(a,b,c)
#endif // __ESXDOS_DOT_COMMAND || T2ESX_TURBO

unsigned char break_pressed(void) __smallc __z88dk_fastcall {
    unsigned char brk = 0x0;
    if (in_key_pressed(IN_KEY_SCANCODE_SPACE | 0x8000)) {
        in_wait_nokey();
        brk = 0xff;
        printf("\n""L Break into Program""\n");
    }
    return brk;
}

void get_fname(unsigned char *fname, unsigned char *hdname) {
    unsigned char i;

    memcpy(fname, hdname, 10);
    fname[10] = 0;
    for (i=9; i>0 && ' '==fname[i]; i--) {
        fname[i] = 0;
    }
}

void load_header(unsigned int expected) __smallc __z88dk_fastcall {
    unsigned char rc;
    printf("?\x08");
    while(1) {
#ifdef __ESXDOS_DOT_COMMAND
        // for dot command vars section is in the DivMMC RAM, not visible from 48k BASIC
        rc = esxdos_zx_tape_load_block(buffer, sizeof(struct zxtapehdr), ZXT_TYPE_HEADER);
        memcpy(&hdr, buffer, sizeof(struct zxtapehdr));
#else
        // tape build resides in the uncontended RAM, no need for memcpy
        rc = esxdos_zx_tape_load_block(&hdr, sizeof(hdr), ZXT_TYPE_HEADER);
#endif // __ESXDOS_DOT_COMMAND
        if (break_pressed()) exit(1);
        if (rc) continue;
        if (BTX_ID_TYPE == (BTX_ID_MASK & hdr.hdtype)) {
            if(hdr.hdlen > BUFFER_SIZE) {
                printf("\nChunk too big %u (max %u)\n", hdr.hdlen, BUFFER_SIZE);
                exit(1);
            }
#ifdef COMPARE_CHUNK_NAMES
            if (expected > 1 && strncmp(hdr.hdname, tname, 10)) {
                printf("*\x08");
                continue;
            }
#endif // COMPARE_CHUNK_NAMES
            if (expected == hdr.hdadd) {
                break;
            } else {
                //putchar(expected > hdr.hdadd ? '>' : '<');
                //putchar('\x08');
                printf("!\x08");
                continue;
            }
        }
    }
}

unsigned char copy_chunk() __smallc __z88dk_fastcall {
    unsigned char rc;
    rc = esxdos_zx_tape_load_block(buffer, hdr.hdlen, ZXT_TYPE_DATA);
    if (0 == rc) {
        printf("S\x08");
        esx_f_write(file, buffer, hdr.hdlen);
        // TODO: check write errors
        putchar(' ');
    } else {
        printf("E");
    }
    return rc;
}

void cleanup(void) __smallc __z88dk_fastcall {
    esxdos_f_close(file);
}

#ifdef __ESXDOS_DOT_COMMAND
void check_args(unsigned int argc, const char *argv[]) {
    unsigned char i;

    for (i=1; i<argc && '-' == *argv[i]; i++) {
        if (!stricmp(argv[i], "-f"))
            overwrite = 1;
    }
}
#endif // __ESXDOS_DOT_COMMAND

#ifdef T2ESX_NEXT
// https://www.specnext.com/forum/viewtopic.php?p=13725
unsigned char zx_model_next() __smallc __z88dk_fastcall {
__asm
    ld  l,0
    ld  a,$80
    DB  $ED, $24, $00, $00   ; mirror a : nop : nop
    dec a
    jr  nz,_not_z80n
    ld  l,0x7f
_not_z80n:
__endasm;
}

#define REG_TURBO_MODE 7
#define RTM_3MHZ  0x00
#define RTM_7MHZ  0x01
#define RTM_14MHZ  0x02
#define RTM_28MHZ  0x03

unsigned char ZXN_READ_REG(unsigned char reg) __smallc __z88dk_fastcall {
// Load subset of DEHL with single argument (L if 8-bit, HL if 16-bit, DEHL if 32-bit) 2. call function 3. return value is in a subset of DEHL
// https://wiki.specnext.dev/CPU_Speed_control
// https://wiki.specnext.dev/CPU_Speed_Register
__asm
    ld bc, 0x243b   ; https://wiki.specnext.dev/TBBlue_Register_Select
    out (c), l
    inc b ;0x253b   ; https://wiki.specnext.dev/TBBlue_Register_Access
    in a, (c)       ; rrAArrPP - Actual, Programmed, reserved
    ld l, a         ; 76543210
__endasm;
}

void check_cpu_speed() __smallc __z88dk_fastcall {
    unsigned char speed;
    if (zx_model_next()) {
        // bits 5-4 "Current actual CPU speed", 1-0 - configured speed
        if(RTM_3MHZ != (speed = (ZXN_READ_REG(REG_TURBO_MODE)>>4)&0x03)) {
            printf("W: CPU@%uMHz\n", 7<<(speed-1));
        }
    }
}
#endif // T2ESX_NEXT

#ifdef __ESXDOS_DOT_COMMAND
unsigned int main(unsigned int argc, const char *argv[]) {
#else
unsigned int main() {
#endif // __ESXDOS_DOT_COMMAND
    unsigned int total, blockno;
    unsigned char fname[11];

    z80_bpoke(23692, 255); // disable "scroll ?" prompts
    //               1            2         3
    //      123456 7890 123456789   0123456789012
    printf("t2esx " VER" BulkTX \x7f 2023 TIsland\n");

#ifdef __ESXDOS_DOT_COMMAND
    if (z80_wpeek(23730) >= (unsigned int)RAM_ADDRESS) {
        printf("M RAMPTOP no good (%u)", (unsigned int)RAM_ADDRESS-1u);
        return 1;
    }

    check_args(argc, argv);
#endif // __ESXDOS_DOT_COMMAND
#ifdef T2ESX_NEXT
    check_cpu_speed();
#endif // T2ESX_NEXT

    load_header(1);

#ifdef COMPARE_CHUNK_NAMES
    strncpy(tname, hdr.hdname, 10);
#endif // COMPARE_CHUNK_NAMES
    get_fname(fname, hdr.hdname);
    blockno = hdr.hdadd;
    total = hdr.hdvars;
    printf("'%s' - %u chunks\nO\x08", fname, total);

    file = esx_f_open(fname, overwrite
            ? ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC
            : ESX_MODE_WRITE|ESXDOS_MODE_CREAT_NOEXIST);
    if (0xff != file) {
        atexit(cleanup);
        // was that a special OPEN file chunk?
        if (BTX_OPEN_ID == hdr.hdtype) {
            // skip everything until the first data chunk
            load_header(1);
        }
        while(1) {
            printf("%uL\x08", blockno);
            if (0 == copy_chunk()) {
                blockno++;
            }
            if (break_pressed()) exit(1);
            if (blockno <= total) {
                load_header(blockno);
            } else {
                break;
            }
        }
    } else {
        printf("Can't open '%s' %u", fname, errno);
    }

    esxdos_f_close(file);
    printf("\n'%s' DONE\n", fname);

    return 0;
}

// EOF vim: ts=4:sw=4:et:ai:
