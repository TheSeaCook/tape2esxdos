# Copyright 2023,24 TIsland Crew
# SPDX-License-Identifier: Apache-2.0

# build tape turbo loader by passing T2ESX_TURBO=1 argument to make

CFLAGS = -O3 --opt-code-size
LDFLAGS = -startup=30 -clib=new
ifdef DEBUG
OPTS +=-debug -DDEBUG -Ca-DDEBUG
endif
ifdef T2ESX_TURBO
OPTS +=-DT2ESX_TURBO -Ca-DT2ESX_TURBO
TURBO_SOURCES += src/tape/tape2esxdos.asm
endif
ifdef T2ESX_NEXT
OPTS +=-DT2ESX_NEXT
UTIL_SOURCES += src/arch-zxn/zxn.c
endif
ifdef T2ESX_CPUFREQ
OPTS +=-DT2ESX_CPUFREQ -Ca-DT2ESX_CPUFREQ
UTIL_SOURCES += src/cpu/cpuspeed.asm src/cpu/cpuspeed.c
endif

all: t2esx t2esx-zx0.tap

tap: t2esx.tap t2esx-zx0.tap

t2esx: src/tape2esxdos.c src/tape/tape2esxdos.asm src/arch-zx/wsalloc.c $(UTIL_SOURCES)
	zcc +zx -vn $(CFLAGS) $(LDFLAGS) $(OPTS) -subtype=dot $^ -o $@ -create-app

%.tap: src/basic/%.bas
	zmakebas -a 90 -n t2esx -o $@ $^

tape2esx_CODE.bin: src/tape2esxdos.c $(TURBO_SOURCES) $(UTIL_SOURCES)
	zcc +zx -vn $(CFLAGS) $(LDFLAGS) $(OPTS) $^ -o tape2esx 

code.tap: tape2esx_CODE.bin
	z88dk-appmake +zx --noloader -b $^ --org 32768 --blockname t2esx -o $@

t2esx.tap: loader.tap code.tap
	cat $^ > $@

t2esx-zx0.tap: loader-zx0.tap code-zx0.tap
	cat $^ > $@

unpack.bin: src/pk/unpack.asm
	zcc +z80 -vn --startup=0 --no-crt -no-stdlib $^ -o $@

code-zx0.bin: unpack.bin tape2esx_CODE.bin.zx0
	cat $^ > $@

code-zx0.tap: code-zx0.bin
	z88dk-appmake +zx --noloader -b $^ --org 45056 --blockname t2esx-zx0 -o $@

tape2esx_CODE.bin.zx0: tape2esx_CODE.bin
	zx0 -f $^

clean:
	rm -f t2esx T2ESX t2esx.tap t2esx-zx0.tap \
		loader-zx0.tap loader.tap code-zx0.tap code.tap \
		code-zx0.bin t2esx_CODE.bin tape2esx_CODE.bin \
		tape2esx tape2esx_CODE.bin.zx0 unpack.bin

dist: t2esx-zx0.tap t2esx
	@mkdir -p dist
	@rm -f dist/t2esx.zip
	zip -r9 dist/t2esx.zip README.md T2ESX t2esx-zx0.tap split.py
	printf '@ t2esx-zx0.tap\n@=t2esx.tap\n' | zipnote -w dist/t2esx.zip

# EOF vim: ts=4:sw=4:noet:ai:
