; API compatible LD_BYTES implementation with configurable timings
; see https://skoolkid.github.io/rom/asm/0556.html
; TODO: inspiration for the fast timings?

LD_BYTES:
    IN         A,(0xfe)		; Make an initial read of port '254'.
    RRA				; Rotate the byte obtained but keep only the EAR bit.
    AND        0x20
    OR         0x2		; Signal red border.
    LD         C,A		; Store the value in the C register (+22 for 'off' and +02 for 'on' - the present EAR state).
    CP         A		; Set the zero flag.
; The first stage of reading a tape involves showing that a pulsing signal actually exists (i.e. 'on/off' or 'off/on' edges).
LD_BREAK:
    RET        NZ		; Return if the BREAK key is being pressed.
    LD	       IY, TIMINGS_TURBO
    LD         A,(IY+10)	; LD_EDGE_1 starts with LD A,...
    LD         (LD_EDGE_ROUTE),A; safe to use A
LD_START:
    CALL       LD_EDGE_1	; Return with the carry flag reset if there is no 'edge' within approx.
    JR         NC,LD_BREAK	; 14,000 T states. But if an 'edge' is found the border will go cyan.
; The next stage involves waiting a while and then showing that the signal is still pulsing.
    LD		L,(IY+0)	; The length of this waiting period
    LD		H,(IY+1)
LD_WAIT:
    DJNZ       LD_WAIT
    DEC        HL
    LD         A,H
    OR         L
    JR         NZ,LD_WAIT	; initial waiting loop end
    CALL       LD_EDGE_2	; Continue only if two edges are found within the allowed time period.
    JR         NC,LD_BREAK
; Now accept only a 'leader signal'.
LD_LEADER:
    LD         B,0x9c		; The timing constant.
    CALL       LD_EDGE_2	; Continue only if two edges are found within the allowed time period.
    JR         NC,LD_BREAK
    LD         A,(IY+2)		; However the edges must have been found within about 3,000
    CP         B		; states of each other.
    JR         NC,LD_START
    INC        H		; Count the pair of edges in the H register until '256' pairs
    JR         NZ,LD_LEADER	; pairs have been found.
; fast speed calculation
    LD         A,B
    ADD        A,0x39
    JR         NC,LD_SYNC
    LD         IY, TIMINGS_NORMAL ; switch to normal speed
    LD         A,(IY+10)	; LD_SYNC immediately calls LD_EDGE_1, which calls LD A,..
    LD         (LD_EDGE_ROUTE),A; hence is it safe to use A here
; After the leader come the 'off' and 'on' parts of the sync pulse.
LD_SYNC:
    LD         B,(IY+3)		; The timing constant.
    LD         A,(IY+11)	; LD A,... + LD_DELAY is the same as LD_EDGE_1
    CALL       LD_DELAY		; Every edge is considered until two edges are found close together
    JR         NC,LD_BREAK	; - these will be the start and finishing edges of the 'off' sync pulse.
    LD         A,B
    CP         (IY+4)
    JR         NC,LD_SYNC
    CALL       LD_EDGE_1	; The finishing edge of the 'on' pulse must exist. (Return carry flag reset.)
    RET        NC
; The bytes of the header or the program/data block can now be loaded or verified. But the first byte is the type flag.
    LD         A,C		; The border colours from now on will be blue and yellow.
    XOR        (IY+5)		; fast colours: grey and white
    LD         C,A
    LD         H,0x0		; Initialise the 'parity matching' byte to zero.
    LD         B,(IY+6)		; Set the timing constant for the flag byte.
    JR         LD_MARKER	; Jump forward into the byte loading loop.
; The byte loading loop is used to fetch the bytes one at a time. The flag byte is first.
; This is followed by the data bytes and the last byte is the 'parity' byte.
LD_LOOP:
    EX         AF,AF'		; Fetch the flags.
    JR         NZ,LD_FLAG	; Jump forward only when handling the first byte.
    ; no need to verify JR         NC,LD_VERIFY	; Jump forward if verifying a tape.
    LD         (IX+0x0),L	; Make the actual load when required.
    JR         LD_NEXT		; Jump forward to load the next byte.
LD_FLAG:
    RL         C		; Keep the carry flag in a safe place temporarily.
    XOR        L		; Return now if the type flag does not match the first byte
    RET        NZ		; on the tape. (Carry flag reset.)
    LD         A,C		; Restore the carry flag now.
    RRA
    LD         C,A
    INC        DE		; Increase the counter to compensate for its 'decrease' after the jump.
    JR         LD_DEC
; no need to verify LD_VERIFY:
    ; no need to verify LD         A,(IX+0x0)	; Fetch the original byte.
    ; no need to verify XOR        L		; Match it against the new byte.
    ; no need to verify RET        NZ		; Return if 'no match'. (Carry flag reset.)
; A new byte can now be collected from the tape.
LD_NEXT:
    INC        IX		; Increase the 'destination'.
LD_DEC:
    DEC        DE		; Decrease the 'counter'.
    EX         AF,AF'		; Save the flags.
    LD         B,(IY+7)		; Set the timing constant.
LD_MARKER:
    LD         L,0x1		; Clear the 'object' register apart from a 'marker' bit.
; The following loop is used to build up a byte in the L register.
LD_8_BITS:
    CALL       LD_EDGE_2	; Find the length of the 'off' and 'on' pulses of the next bit.
    RET        NC		; Return if the time period is exceeded. (Carry flag reset.)
    LD         A,(IY+8)		; Compare the length against approx. 2,400 T states,
    CP         B		; resetting the carry flag for a '0' and setting it for a '1'.
    RL         L		; Include the new bit in the L register.
    LD         B,(IY+9)		; Set the timing constant for the next bit.
    jr         NC,LD_8_BITS	; Jump back whilst there are still bits to be fetched.
; The 'parity matching' byte has to be updated with each new byte.
    LD         A,H		; Fetch the 'parity matching' byte and include the new byte.
    XOR        L
    LD         H,A		; Save it once again.
; Passes round the loop are made until the 'counter' reaches zero. At that point the 'parity matching' byte should be holding zero.
    LD         A,D		; Make a further pass if the DE register pair does not hold zero.
    OR         E
    JR         NZ,LD_LOOP
    LD         A,H		; Fetch the 'parity matching' byte.
    CP         0x1		; Return with the carry flag set if the value is zero.
    RET				; (Carry flag reset if in error.)
; The subroutines are entered with a timing constant in the B register, and the previous border colour and 'edge-type' in the C register.
; The subroutines return with the carry flag set if the required number of 'edges' have been found in the time allowed, and the change to the value in the B register shows just how long it took to find the 'edge(s)'.
; The carry flag will be reset if there is an error. The zero flag then signals 'BREAK pressed' by being reset, or 'time-up' by being set.
; The entry point LD_EDGE_2 is used when the length of a complete pulse is required and LD_EDGE_1 is used to find the time before the next 'edge'.
LD_EDGE_2:	; self-modifying code, switch between FAST and NORMAL
LD_EDGE_ROUTE equ $+1	; FIXME: any way to avoid it? all seems to be slower
    jr LD_T_EDGE_2
; FAST routine skips initial delay controlled by the LD_DELAY loop
LD_T_EDGE_2:
    CALL       LD_T_EDGE_1
    RET        NC
    JR         LD_T_EDGE_1
; NORMAL speed routine
LD_N_EDGE_2:
    CALL       LD_EDGE_1	; In effect call LD_EDGE_1 twice, returning in between if there is an error.
    RET        NC
; This entry point is used by the routine at LD_BYTES [at normal speed].
LD_EDGE_1:
    LD         A,0x16		; Wait 358 T states before entering the sampling loop.
LD_DELAY:
    DEC        A
    JR         NZ,LD_DELAY
LD_T_EDGE_1:			; entry point for FAST speed, bypass LD_DELAY loop
    AND        A
; The sampling loop is now entered. The value in the B register is incremented for each pass; 'time-up' is given when B reaches zero.
LD_SAMPLE:
    INC        B		; Count each pass.
    RET        Z		; Return carry reset and zero set if 'time-up'.
    LD         A,0x7f		; Read from port +7FFE, i.e. BREAK and EAR.
    IN         A,(0xfe)
    RRA				; Shift the byte.
    RET        NC		; Return carry reset and zero reset if BREAK was pressed.
    XOR        C		; Now test the byte against the 'last edge-type';
    AND        0x20		; jump back unless it has changed.
    JR         Z,LD_SAMPLE
; A new 'edge' has been found within the time period allowed for the search. So change the border colour and set the carry flag.
    LD         A,C		; Change the 'last edge-type' and border colour.
    CPL
    LD         C,A
    AND        0x7		; Keep only the border colour.
    OR         0x8		; Signal 'MIC off'.
    OUT        (0xfe),A		; Change the border colour (red/cyan or blue/yellow).
    SCF				; Signal the successful search before returning.
    RET

TIMINGS_NORMAL:
    DW		0x0415	; +0    LD_WAIT timing
    DB		0xc6	; +2	LD_LEADER
    DB		0xC9	; +3	LD_SYNC
    DB		0xd4	; +4	LD_SYNC
    DB		0x03	; +5
    DB		0xb0	; +6
    DB		0xb2	; +7
    DB		0xcb	; +8
    DB		0xb0	; +9
    DB		LD_N_EDGE_2-LD_T_EDGE_2
    DB          0x16	; +11 ; this one much match L_EDGE_1 LD A,0x16
TIMINGS_TURBO:
    DW		0x020a  ; +0
    DB		0xb0	; +2
    DB		0xe0	; +3
    DB		0xec	; +4
    DB		0x02	; +5
    DB		0xe4	; +6
    DB		0xe6	; +7
    DB		0xf0	; +8
    DB		0xe4	; +9
    DB		0x00	; +10
    DB          0x08	; +11

; EOF
