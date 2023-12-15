all: t2esx t2esx.tap

t2esx: tape2esxdos.c tape2esxdos.asm
	zcc +zx -vn -subtype=dot -startup=30 -clib=new -SO3 --opt-code-size $^ -o $@ -create-app

loader.tap: loader.bas
	zmakebas -a 90 -n t2esx -o $@ $^

t2esx_CODE.bin: tape2esxdos.c
	zcc +zx -vn -SO3 -startup=30 -clib=sdcc_iy --max-allocs-per-node200000 --opt-code-size $^ -o t2esx 

code.tap: t2esx_CODE.bin
	bin2tap -c $^ $@ t2esx 32768

t2esx.tap: loader.tap code.tap
	cat $^ > $@

# EOF vim: ts=4:sw=4:noet:ai:
