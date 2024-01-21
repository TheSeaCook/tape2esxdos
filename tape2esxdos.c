// Copyright 2023,24 TIsland Crew
// SPDX-License-Identifier: Apache-2.0
#include <arch/zx.h>
#include <arch/zx/esxdos.h>
#include <errno.h>
#include <input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <z80.h>

#ifdef DEBUG
#define debugpf(...) printf("DEBUG: "__VA_ARGS__)
#else
#define debugpf(...)
#endif // DEBUG

#ifndef DEBUG
#pragma printf "%s %u"
#endif // DEBUG
#ifdef T2ESX_NEXT
// two atexit calls for Next (close + cpu freq)
#pragma output CLIB_EXIT_STACK_SIZE = 2
#else // not T2ESX_NEXT
// we have only one atexit call for esxdos
#pragma output CLIB_EXIT_STACK_SIZE = 1
#endif // T2ESX_NEXT
#ifndef __ESXDOS_DOT_COMMAND
// make sure stack is below buffer area
#pragma output REGISTER_SP = 45055
#endif // __ESXDOS_DOT_COMMAND

#ifdef T2ESX_TURBO
#define VTAG "t"
#elif defined(T2ESX_CPUFREQ)
#define VTAG "V"
#elif defined(T2ESX_NEXT)
#define VTAG "n"
#else
#define VTAG "v"
#endif // T2ESX_TURBO / T2ESX_CPUFREQ

// 4 chars only for the version tag
#define VER VTAG"2.0"

#define BTX_ID_MASK 0xF0u
#define BTX_ID_TYPE 0x80u
#define BTX_OPEN_ID 0x87u
#define BTX_CHUNK_ID 0x88u

#define BUFFER_SIZE 16384
#define RAM_ADDRESS 0xB000

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
#ifdef __ESXDOS_DOT_COMMAND
static void *buffer;
#else
static void *buffer = (void *)RAM_ADDRESS;
#endif // __ESXDOS_DOT_COMMAND

#ifdef T2ESX_CPUFREQ

#define CPU_3MHZ    0
#define CPU_7MHZ    1
#define CPU_14MHZ   2
#define CPU_28MHZ   3

extern unsigned int __LIB__ cpu_speed_callee(void *addr) __smallc __z88dk_callee;
#define cpu_speed(b) cpu_speed_callee(b)

#endif // T2ESX_CPUFREQ

#ifdef T2ESX_NEXT

#define REG_TURBO_MODE 7
#define RTM_UNDEF 0x7f
#define RTM_3MHZ  0x00
#define RTM_7MHZ  0x01
#define RTM_14MHZ  0x02
#define RTM_28MHZ  0x03

static unsigned char next_current_speed;
#ifdef __ESXDOS_DOT_COMMAND
static unsigned char next_required_speed = RTM_28MHZ;
#else
static unsigned char next_required_speed = RTM_UNDEF;
#endif // __ESXDOS_DOT_COMMAND

#endif // T2ESX_NEXT

#if defined(T2ESX_TURBO) || defined(__ESXDOS_DOT_COMMAND)
extern unsigned char __LIB__ esxdos_zx_tape_load_block(void *dst,unsigned int len,unsigned char type) __smallc;
extern unsigned char __LIB__ esxdos_zx_tape_load_block_callee(void *dst,unsigned int len,unsigned char type) __smallc __z88dk_callee;
#define esxdos_zx_tape_load_block(a,b,c) esxdos_zx_tape_load_block_callee(a,b,c)
#else
#define esxdos_zx_tape_load_block(a,b,c) zx_tape_load_block(a,b,c)
#endif // __ESXDOS_DOT_COMMAND || T2ESX_TURBO

unsigned char break_pressed(void) __z88dk_fastcall {
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

void load_header(unsigned int expected) __z88dk_fastcall {
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

unsigned char copy_chunk() __z88dk_fastcall {
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

void cleanup(void) __z88dk_fastcall {
    esxdos_f_close(file);
}

#ifdef __ESXDOS_DOT_COMMAND
unsigned int free_spaces() __z88dk_fastcall __naked {
// see also https://skoolkid.github.io/rom/asm/1F1A.html
__asm
    ld bc, 0
    rst 0x18
    dw 0x1f05   ; https://skoolkid.github.io/rom/asm/1F05.html
    ; HL	STKEND+BC+80-SP (free space - 1) CF=1
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

#ifdef DEBUG
void dbg_dump_mem_vars(void) __z88dk_fastcall {
    unsigned int spaces = free_spaces();
    printf("WORKSP: %u\x06STKBOT: %u\nSTKEND: %u\x06RAMTOP: %u\n",
        z80_wpeek(0x5c61), z80_wpeek(0x5c63), z80_wpeek(0x5c65), z80_wpeek(0x5cb2));
    printf("FREE SPACES: %x %u\n", spaces, spaces);
}
#endif // DEBUG

void *allocate_buffer(unsigned int size) __smallc __z88dk_callee {
    unsigned int buf = 0;
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

    // do we have enough memory above RAMTOP? it's uncontended
    if (RAMTOP < (P_RAMT-BUFFER_SIZE+1)) {
        // there MAY be enough bytes between RAMTOP and P_RAMT
        debugpf("RAMTOP %u UDG %u, has free mem\n", RAMTOP, UDG);
        // TODO: optimise, pre-calculate P_RAMT-BUFFER_SIZE
        // it would be MUCH easier if we let ourselves to trash UDGs
        if (UDG > RAMTOP) {
            if ((UDG-BUFFER_SIZE-1)>RAMTOP) {
                buf = (UDG-BUFFER_SIZE); // RAMTOP -> buf -> UDG
            } else if (P_RAMT-BUFFER_SIZE+1+8*21>UDG) { // 21 UDG's
                buf = P_RAMT-BUFFER_SIZE+1; // UDG -> buf -> P_RAMT
            }
        } else {
            // R_RAMT point to the last byte, len = last_byte + 1
            buf = P_RAMT-BUFFER_SIZE+1;
        }
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

void check_args(unsigned int argc, const char *argv[]) {
    unsigned char i;

    for (i=1; i<(unsigned char)argc && '-' == *argv[i] && strlen(argv[i]) > 1; i++) {
        if ('f' == argv[i][1])
            overwrite = 1;
#ifdef T2ESX_NEXT
        else if ('t' == argv[i][1]) {
            if (strlen(argv[i]) > 2) {
                next_required_speed = 0x03 & (argv[i][2] - '0');
            } else {
                next_required_speed = RTM_3MHZ;
            }
            debugpf("requested speed: %u\n", next_required_speed);
        }
#endif
    }
}
#endif // __ESXDOS_DOT_COMMAND

#ifdef T2ESX_NEXT
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
    inc b ;0x253b   ; https://wiki.specnext.dev/TBBlue_Register_Access
    in l, (c)
    ret
__endasm;
}

void ZXN_WRITE_REG_callee(unsigned char reg, unsigned char val) __smallc __z88dk_callee __preserves_regs(af,ix,iy) __naked {
__asm
    pop hl ; return address
    pop de ; val
    ex (sp), hl ; reg
    ld bc, 0x243b
    out (c), l
    inc b
    out (c), e
    ret
__endasm;
}
#define ZXN_WRITE_REG(a,b) ZXN_WRITE_REG_callee(a,b)

void speed_restore() __z88dk_fastcall {
    ZXN_WRITE_REG(REG_TURBO_MODE, next_current_speed);
}

void check_cpu_speed() __z88dk_fastcall {
    unsigned char speed;
    if (zx_model_next()) {
        if (RTM_UNDEF != next_required_speed) {
            next_current_speed = ZXN_READ_REG(REG_TURBO_MODE)&0x03;
            atexit(speed_restore);
            ZXN_WRITE_REG(REG_TURBO_MODE, next_required_speed);
            debugpf("%u -> REG7\n", next_required_speed);
        }
        // https://wiki.specnext.dev/CPU_Speed_control
        // https://wiki.specnext.dev/CPU_Speed_Register
        // bits 5-4 "Current actual CPU speed", 1-0 - configured speed
        if(RTM_3MHZ != (speed = (ZXN_READ_REG(REG_TURBO_MODE)>>4)&0x03)) {
            printf("\x06""CPU @ %uMHz\n", 7<<(speed-1));
        }
    }
}
#endif // T2ESX_NEXT

#ifdef T2ESX_CPUFREQ
unsigned char t_states_to_mhz(unsigned int tstates) __z88dk_fastcall {
    if (tstates<3116) { // 3036/3080 48/128
        return CPU_3MHZ;
    } else if (tstates<6233) {  // 6163
        return CPU_7MHZ;
    } else if (tstates<12466) { // 9777
        return CPU_14MHZ;
    //} else if (tstates<24932) { // 19559
        //return CPU_28MHZ;
    } else {
        return CPU_28MHZ;
    }
}

void check_cpu_speed() __z88dk_fastcall {
    unsigned char speed;
#ifdef DEBUG
    unsigned int tstates = cpu_speed(buffer);
    debugpf("T-states %u\n", tstates);
    if ((speed = t_states_to_mhz(tstates)) > CPU_3MHZ) {
#else
    if ((speed = t_states_to_mhz(cpu_speed(buffer))) > CPU_3MHZ) {
#endif
#ifdef T2ESX_TURBO
        printf("W: flaky 2x\x06""CPU@%uMHz\n", 7<<(speed-1));
#else
        printf("\x06""CPU@%uMHz\n", 7<<(speed-1));
#endif // T2ESX_TURBO
    }
}
#endif // T2ESX_CPUFREQ

#ifdef __ESXDOS_DOT_COMMAND
unsigned int main(unsigned int argc, const char *argv[]) {
#else
unsigned int main() {
#endif // __ESXDOS_DOT_COMMAND
    unsigned int total, blockno;
    unsigned char fname[11];

    z80_bpoke(23692, 255); // disable "scroll ?" prompts
    //                1           2            3
    //      123456 7890 123456789 0   123456789012
    printf("t2esx " VER" BulkTX \x7f""23,24 TIsland\n");

#ifdef __ESXDOS_DOT_COMMAND
    buffer = allocate_buffer(BUFFER_SIZE);
    debugpf("buffer @%x\n", buffer);
    #ifdef DEBUG
    dbg_dump_mem_vars();
    #endif
    if (!buffer) {
        printf("M RAMPTOP no good (%u)", (unsigned int)RAM_ADDRESS-1u);
        return 1;
    }

    check_args(argc, argv);
#endif // __ESXDOS_DOT_COMMAND
#if defined(T2ESX_NEXT) || defined(T2ESX_CPUFREQ)
    check_cpu_speed();
#endif // T2ESX_NEXT || T2ESX_CPUFREQ

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
