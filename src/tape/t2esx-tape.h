// Copyright 2023,24 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

#ifndef __T2ESX_TAPE__
#define __T2ESX_TAPE__

#define ZXT_H_PROGRAM 0
#define ZXT_H_NUM_ARRAY 1
#define ZXT_H_CHAR_ARRAY 2
#define ZXT_H_CODE 3

extern unsigned char __LIB__ esxdos_zx_tape_load_block(void *dst,unsigned int len,unsigned char type) __smallc;
extern unsigned char __LIB__ esxdos_zx_tape_load_block_callee(void *dst,unsigned int len,unsigned char type) __smallc __z88dk_callee;
#define esxdos_zx_tape_load_block(a,b,c) esxdos_zx_tape_load_block_callee(a,b,c)

#endif
// EOF vim: ts=4:sw=4:et:ai:
