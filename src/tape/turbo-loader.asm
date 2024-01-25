; see "Ramsoft The ZX Spectrum Loaders Guide" - http://www.ramsoft.bbk.org/
;     (yes, it's long gone, use archive.org)
; https://www.spectrumcomputing.co.uk/entry/17882/ZX-Spectrum/Turbo_Compress_Copy
; with the timings adjusted to match https://splintergu.itch.io/tzx2turbo
; "convert TZX files into TZX Turbo files compatible with TK90x ROM v3"
; after all the changes it looks similar to LD_BYTES from unofficial 48turbo.zip
; and, of course,
;     "The complete ZX Spectrum ROM disassembly" , Melbourne House 1983.
;     by Dr. Ian Logan & Dr. Franklin O'Hara    ISBN 0 86759 117 X
; TODO: since we have it in RAM, we should optimise it for size

; basic math, from Ramsoft's guide:
; t-states = clock / (2 * f)
; EDGE_2 to EDGE_2 time:
; T-States = 206 + 118 * (B-Reg-Val - B-Reg-Start-Val - 1) + 56 * A-Reg-Val + STUFF

LD_BYTES:
    IN         A,(0xfe)
    RRA
    AND        0x20
    OR         0x2
    LD         C,A
    CP         A
LD_BREAK:
    RET        NZ
LD_START:
    CALL       LD_EDGE_1
    JR         NC,LD_BREAK
    LD         HL,0x20a
LD_WAIT:
    DJNZ       LD_WAIT
    DEC        HL
    LD         A,H
    OR         L
    JR         NZ,LD_WAIT
    CALL       LD_EDGE_2
    JR         NC,LD_BREAK
LD_LEADER:
    LD         B,0x9c
    CALL       LD_EDGE_2
    JR         NC,LD_BREAK
    LD         A,0xb0
    CP         B
    JR         NC,LD_START
    INC        H
    JR         NZ,LD_LEADER
    LD         A,B
    ADD        A,0x39
    JR         C,LD_SYNC	; was it too long? back to normal speed
LD_T_SYNC:
    LD         B,0xe0
    LD         A,0x8
    CALL       LD_DELAY		; that's the same as LD_EDGE_1 with different delay
    JR         NC,LD_BREAK
    LD         A,B
    CP         0xec
    JR         NC,LD_T_SYNC
    CALL       LD_T_EDGE_1
    RET        NC
    LD         A,C
    XOR        0x1		; different colour
    LD         C,A
    LD         H,0x0
    LD         B,0xe4
    JR         LD_T_MARKER
LD_T_LOOP:
    EX         AF,AF'
    JR         NZ,LD_T_FLAG
    LD         (IX+0x0),L
    JR         LD_T_NEXT
LD_T_FLAG:
    RL         C
    XOR        L
    RET        NZ
    LD         A,C
    RRA
    LD         C,A
    INC        DE
    JR         LD_T_DEC
LD_T_NEXT:
    INC        IX
LD_T_DEC:
    DEC        DE
    EX         AF,AF'
    LD         B,0xe6
LD_T_MARKER:
    LD         L,0x1
LD_T_8_BITS:
    CALL       LD_T_EDGE_2
    RET        NC
    LD         A,0xf0
    CP         B
    RL         L
    LD         B,0xe4
    JP         NC,LD_T_8_BITS
    LD         A,H
    XOR        L
    LD         H,A
    LD         A,D
    OR         E
    JR         NZ,LD_T_LOOP
    LD         A,H
    CP         0x1
    RET
LD_T_EDGE_2:
    CALL       LD_T_EDGE_1	; bypass LD_DELAY loop
    RET        NC
    JP         LD_T_EDGE_1	; bypass LD_DELAY loop
LD_SYNC:
    LD         B,0xc9
    CALL       LD_EDGE_1
    JP         NC,LD_BREAK
    LD         A,B
    CP         0xd4
    JR         NC,LD_SYNC
    CALL       LD_EDGE_1
    RET        NC
    LD         A,C
    XOR        0x3
    LD         C,A
    LD         H,0x0
    LD         B,0xb0
    JR         LD_MARKER
LD_LOOP:
    EX         AF,AF'
    JR         NZ,LD_FLAG
    LD         (IX+0x0),L
    JR         LD_NEXT
LD_FLAG:
    RL         C
    XOR        L
    RET        NZ
    LD         A,C
    RRA
    LD         C,A
    INC        DE
    JR         LD_DEC
LD_NEXT:
    INC        IX
LD_DEC:
    DEC        DE
    EX         AF,AF'
    LD         B,0xb2
LD_MARKER:
    LD         L,0x1
LD_8_BITS:
    CALL       LD_EDGE_2
    RET        NC
    LD         A,0xcb
    CP         B
    RL         L
    LD         B,0xb0
    JP         NC,LD_8_BITS
    LD         A,H
    XOR        L
    LD         H,A
    LD         A,D
    OR         E
    JR         NZ,LD_LOOP
    LD         A,H
    CP         0x1
    RET

LD_EDGE_2:
    CALL       LD_EDGE_1
    RET        NC
LD_EDGE_1:
    LD         A,0x16
LD_DELAY:
    DEC        A
    JR         NZ,LD_DELAY
LD_T_EDGE_1:
    AND        A
LD_SAMPLE:
    INC        B
    RET        Z
    LD         A,0x7f
    IN         A,(0xfe)
    RRA
    RET        NC
    XOR        C
    AND        0x20
    JR         Z,LD_SAMPLE
    LD         A,C
    CPL
    LD         C,A
    AND        0x7
    OR         0x8
    OUT        (0xfe),A
    SCF
    RET
; EOF
