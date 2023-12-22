# Copyright 2023 TIsland Crew
# SPDX-License-Identifier: Apache-2.0
all: T2ESX t2esx-zx0.tap

tap: t2esx.tap t2esx-zx0.tap

T2ESX: tape2esxdos.c tape2esxdos.asm
	zcc +zx -vn -subtype=dot -startup=30 -clib=new -SO3 --opt-code-size $^ -o $@ -create-app

%.tap: %.bas
	zmakebas -a 90 -n t2esx -o $@ $^

t2esx_CODE.bin: tape2esxdos.c
	zcc +zx -vn -SO3 -startup=30 -clib=sdcc_iy --max-allocs-per-node200000 --opt-code-size $^ -o t2esx 

code.tap: t2esx_CODE.bin
	bin2tap -c $^ $@ t2esx 32768

t2esx.tap: loader.tap code.tap
	cat $^ > $@

t2esx-zx0.tap: loader-zx0.tap code-zx0.tap
	cat $^ > $@

unpack.bin: unpack.asm
	sjasmplus $^

code-zx0.bin: unpack.bin t2esx_CODE.bin.zx0
	cat $^ > $@

code-zx0.tap: code-zx0.bin
	bin2tap -c $^ $@ t2esx-zx0 45056

t2esx_CODE.bin.zx0: t2esx_CODE.bin
	zx0 -f $^

clean:
	rm -f t2esx.tap t2esx-zx0.tap loader-zx0.tap loader.tap code-zx0.tap code.tap code-zx0.bin t2esx_CODE.bin t2esx_CODE.bin.zx0 unpack.bin T2ESX

dist: t2esx-zx0.tap t2esx
	mkdir -p dist
	@rm -f dist/t2esx.zip
	zip -r9 dist/t2esx.zip README.md t2esx t2esx-zx0.tap split.py
	printf '@ t2esx-zx0.tap\n@=t2esx.tap\n' | zipnote -w dist/t2esx.zip

# EOF vim: ts=4:sw=4:noet:ai:
