// Copyright 2023,24 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

#ifndef __WS_ALLOC__
#define __WS_ALLOC__

#ifdef DEBUG
void dbg_dump_mem_vars(void) __z88dk_fastcall;
#else
#define dbg_dump_mem_vars(...)
#endif
unsigned int free_spaces() __z88dk_fastcall __naked;
unsigned int bc_spaces(unsigned int count) __z88dk_fastcall __naked;
void *allocate_buffer(unsigned int size) __smallc __z88dk_callee;

#endif
// EOF vim: ts=4:sw=4:et:ai:
