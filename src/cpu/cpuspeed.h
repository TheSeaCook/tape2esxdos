// Copyright 2023,24 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

#ifndef __CPU_SPEED_H__
#define __CPU_SPEED_H__

#define CPU_3MHZ    0
#define CPU_7MHZ    1
#define CPU_14MHZ   2
#define CPU_28MHZ   3

extern unsigned int __LIB__ cpu_speed_callee(void *addr) __smallc __z88dk_callee;
#define cpu_speed(b) cpu_speed_callee(b)

unsigned char t_states_to_mhz(unsigned int tstates) __z88dk_fastcall;

#endif // __CPU_SPEED_H__
// EOF vim: ts=4:sw=4:et:ai:
