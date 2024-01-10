# Copyright 2023,24 TIsland Crew
# SPDX-License-Identifier: Apache-2.0

# build tape turbo loader by passing T2ESX_TURBO=1 argument to make

CFLAGS = -SO3 --opt-code-size
LDFLAGS = -startup=30 -clib=new
ifdef DEBUG
OPTS +=-debug -DDEBUG -Ca-DDEBUG
endif
ifdef T2ESX_TURBO
OPTS +=-DT2ESX_TURBO -Ca-DT2ESX_TURBO
TURBO_SOURCES += tape2esxdos.asm
endif
ifdef T2ESX_NEXT
ifdef T2ESX_CPUFREQ
$(error T2ESX_NEXT and T2ESX_CPUFREQ cannot be enabled at the same time!)
endif
OPTS +=-DT2ESX_NEXT
endif
ifdef T2ESX_CPUFREQ
ifdef T2ESX_NEXT
$(error T2ESX_NEXT and T2ESX_CPUFREQ cannot be enabled at the same time!)
endif
OPTS +=-DT2ESX_CPUFREQ -Ca-DT2ESX_CPUFREQ
UTIL_SOURCES += cpuspeed.asm
endif

all: t2esx t2esx-zx0.tap

tap: t2esx.tap t2esx-zx0.tap

t2esx: tape2esxdos.c tape2esxdos.asm $(UTIL_SOURCES)
	zcc +zx -vn $(CFLAGS) $(LDFLAGS) $(OPTS) -subtype=dot $^ -o $@ -create-app

%.tap: %.bas
	zmakebas -a 90 -n t2esx -o $@ $^

tape2esx_CODE.bin: tape2esxdos.c $(TURBO_SOURCES) $(UTIL_SOURCES)
	zcc +zx -vn $(CFLAGS) $(LDFLAGS) $(OPTS) $^ -o tape2esx 

code.tap: tape2esx_CODE.bin
	bin2tap -c $^ $@ t2esx 32768

t2esx.tap: loader.tap code.tap
	cat $^ > $@

t2esx-zx0.tap: loader-zx0.tap code-zx0.tap
	cat $^ > $@

unpack.bin: unpack.asm
	sjasmplus $^

code-zx0.bin: unpack.bin tape2esx_CODE.bin.zx0
	cat $^ > $@

code-zx0.tap: code-zx0.bin
	bin2tap -c $^ $@ t2esx-zx0 45056

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
