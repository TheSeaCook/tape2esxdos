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

#define T2ESX_BUFFER

#ifdef T2ESX_NEXT
#include "src/arch-zxn/zxn.h"
#endif
#ifdef T2ESX_CPUFREQ
#include "src/cpu/cpuspeed.h"
#endif
#ifdef __ESXDOS_DOT_COMMAND
#include "src/arch-zx/wsalloc.h"
#endif // __ESXDOS_DOT_COMMAND
#if defined(T2ESX_TURBO) || defined(__ESXDOS_DOT_COMMAND)
#include "src/tape/t2esx-tape.h"
#else
#define esxdos_zx_tape_load_block(a,b,c) zx_tape_load_block(a,b,c)
#endif // __ESXDOS_DOT_COMMAND || T2ESX_TURBO

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

#if defined(T2ESX_TURBO) && defined(T2ESX_CPUFREQ) && defined(T2ESX_NEXT)
#define VTAG "a"
#elif defined(T2ESX_TURBO)
#define VTAG "t"
#elif defined(T2ESX_CPUFREQ)
#define VTAG "V"
#elif defined(T2ESX_NEXT)
#define VTAG "n"
#else
#define VTAG "v"
#endif // T2ESX_TURBO / T2ESX_CPUFREQ

// 4 chars only for the version tag
#define VER VTAG"2.1"

#define BTX_ID_MASK 0xF0u
#define BTX_ID_TYPE 0x80u
#define BTX_OPEN_ID 0x87u
#define BTX_CHUNK_ID 0x88u

#define DEFAULT_BUFFER_SIZE 16384
#define RAM_ADDRESS 0xB000

static unsigned int buffer_size = DEFAULT_BUFFER_SIZE;
static struct zxtapehdr hdr;
static unsigned char file;
static unsigned char overwrite; // overwrite target file
static unsigned char wsonly;    // allocate memory in WORKSPACE only
#define WS_UNCONTENDED_MASK 0x10
// wsonly: bit 5 - require unconteded ram, bit 0 - use WS only
// 0x11 -- WS only, unconteded only, 0x01 -- WS, allow conteded
#ifdef COMPARE_CHUNK_NAMES
static unsigned char tname[10];
#endif // COMPARE_CHUNK_NAMES
#ifdef __ESXDOS_DOT_COMMAND
static void *buffer;
#else
static void *buffer = (void *)RAM_ADDRESS;
#endif // __ESXDOS_DOT_COMMAND

#ifdef T2ESX_NEXT
static unsigned char next_current_speed;
#ifdef __ESXDOS_DOT_COMMAND
static unsigned char next_required_speed = RTM_28MHZ;
#else
static unsigned char next_required_speed = RTM_UNDEF;
#endif // __ESXDOS_DOT_COMMAND
#endif // T2ESX_NEXT

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
            if(hdr.hdlen > buffer_size) {
                printf("\nChunk too big %u (max %u)\n", hdr.hdlen, buffer_size);
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
void check_args(unsigned int argc, const char *argv[]) {
    unsigned char i, flg, al;

    for (i=1; i<(unsigned char)argc && '-' == *argv[i] && (al=strlen(argv[i])) > 1; i++) {
        flg = argv[i][1];
        if ('f' == flg)
            overwrite = 1;
        else if ('w' == flg) {
            wsonly = 0x11; // bits 5+1, require unconteded RAM + WS only
            if (al > 2 && 'l' == argv[i][2]) {
                wsonly = 0x01; // allow lower RAM for WS allocation
            }
        }
#ifdef T2ESX_BUFFER
        else if ('b' == flg) {
            if (al > 5) {
                buffer_size = atoi((char *)argv[i]+2);
                debugpf("b: %s %u\n", argv[i]+2, buffer_size);
                if (buffer_size < 1024 || buffer_size > DEFAULT_BUFFER_SIZE) {
                    buffer_size = DEFAULT_BUFFER_SIZE;
                    printf("E: %s\n", argv[i]);
                }
            } else {
                buffer_size = free_spaces() & 0xff00;
                wsonly = 0x01;
            }
        }
#define USAGE_BUF " [-bSIZE]"
#else
#define USAGE_BUF
#endif
#ifdef T2ESX_NEXT
        else if ('t' == flg) {
            if (al > 2) {
                next_required_speed = 0x03 & (argv[i][2] - '0');
            } else {
                next_required_speed = RTM_3MHZ;
            }
            debugpf("requested speed: %u\n", next_required_speed);
        }
#define USAGE_NEXT " [-t[0-3]]"
#else
#define USAGE_NEXT
#endif
        else if ('h' == flg) {
            printf(" .t2esx"USAGE_BUF" [-f] [-w[l]]"USAGE_NEXT"\n");
            exit(0);
        }
    }
    debugpf("- %u f:%d w:%d\n", buffer_size, overwrite, wsonly);
}
#endif // __ESXDOS_DOT_COMMAND

#ifdef T2ESX_NEXT
void speed_restore() __z88dk_fastcall {
    ZXN_WRITE_REG(REG_TURBO_MODE, next_current_speed);
}
#endif // T2ESX_NEXT

#if defined(T2ESX_NEXT) || defined(T2ESX_CPUFREQ)
void check_cpu_speed() __z88dk_fastcall {
    unsigned char speed;
#ifdef T2ESX_NEXT
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
#ifdef T2ESX_CPUFREQ
        else
#endif
#endif // T2ESX_NEXT
#ifdef T2ESX_CPUFREQ
    {
#ifdef DEBUG
        unsigned int tstates = cpu_speed(buffer);
        debugpf("T-states %u\n", tstates);
#define tstates() tstates
#else
#define tstates() cpu_speed(buffer)
#endif // DEBUG
        if ((speed = t_states_to_mhz(tstates())) > CPU_3MHZ) {
#ifdef T2ESX_TURBO
            printf("W: flaky 2x\x06""CPU@%uMHz\n", 7<<(speed-1));
#else
            printf("\x06""CPU@%uMHz\n", 7<<(speed-1));
#endif // T2ESX_TURBO
        }
    }
#endif // T2ESX_CPUFREQ
}
#endif // T2ESX_NEXT || T2ESX_CPUFREQ

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
    check_args(argc, argv);

    if(wsonly || !(buffer = allocate_above_ramtop(buffer_size))) {
        buffer = allocate_from_workspace(buffer_size, wsonly&WS_UNCONTENDED_MASK);
    }
    debugpf("buffer @%x\n", buffer);
    #ifdef DEBUG
    dbg_dump_mem_vars();
    #endif
    if (!buffer) {
        printf("M RAMPTOP no good (%u)\n", (unsigned int)RAM_ADDRESS-1u);
        return 1;
    }
#ifdef T2ESX_BUFFER
    printf("\x06""%u\n", buffer_size);
#endif // T2ESX_BUFFER
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
