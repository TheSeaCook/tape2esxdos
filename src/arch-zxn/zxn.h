// Copyright 2023,24 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

#ifndef __T2ESX_ARCH_ZXN__
#define __T2ESX_ARCH_ZXN__

#define REG_TURBO_MODE 7
#define RTM_UNDEF 0x7f
#define RTM_3MHZ  0x00
#define RTM_7MHZ  0x01
#define RTM_14MHZ  0x02
#define RTM_28MHZ  0x03

unsigned char zx_model_next() __z88dk_fastcall __preserves_regs(bc,de,ix,iy) __naked;
unsigned char ZXN_READ_REG(unsigned char reg) __z88dk_fastcall __preserves_regs(af,de,ix,iy) __naked;
void ZXN_WRITE_REG_callee(unsigned char reg, unsigned char val) __smallc __z88dk_callee __preserves_regs(af,ix,iy) __naked;
#define ZXN_WRITE_REG(a,b) ZXN_WRITE_REG_callee(a,b)

#endif // __T2ESX_ARCH_ZXN__
// EOF vim: ts=4:sw=4:et:ai:
