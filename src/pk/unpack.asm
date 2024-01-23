; Copyright 2023 TIsland Crew
; SPDX-License-Identifier: Apache-2.0
	DEVICE ZXSPECTRUM48
	org 0xB000
unpack:
	ld hl, data	; source
	ld de, 0x8000	; destination
	push de
dzx0:
	include dzx0_standard.asm

data:
	DEVICE ZXSPECTRUM48
	savebin "unpack.bin",unpack,$-unpack
