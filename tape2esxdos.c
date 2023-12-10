#include <arch/zx.h>
#include <arch/zx/esxdos.h>
#include <stdio.h>
#include <string.h>
#include <z80.h>

#pragma printf "%s %u"

#define BUFFER_SIZE 16384
#define BULK_ID 0x88
#define RAM_ADDRESS 0xB000

#define buffer ((void *)RAM_ADDRESS)

static struct zxtapehdr hdr;
static unsigned char file;

unsigned char* get_fname(unsigned char *fname, unsigned char *hdname);

int main(int argc, char *argv) {
    unsigned int total, blockno;
    unsigned char fname[11];

    z80_bpoke(23692, 255); // disable "scroll ?" prompts
    printf("tap2disk Bulk Transfer \x7f TIsland\n");

    if (z80_wpeek(23730) >= (unsigned int)RAM_ADDRESS) {
        printf("M RAMPTOP no good (%u)", (unsigned int)RAM_ADDRESS);
        return 1;
    }

    do {
        zx_tape_load_block(&hdr, sizeof(hdr), ZXT_TYPE_HEADER);
    } while (BULK_ID != hdr.hdtype);

    get_fname(fname, hdr.hdname);
    blockno = hdr.hdadd;
    total = hdr.hdvars;
    printf("esxDOS name: %s %u blocks\n", fname, total);

    file = esx_f_open(fname, ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    if (0xff != file) {
        do {
            printf("%uL\x08", blockno);
            zx_tape_load_block(buffer, hdr.hdlen, ZXT_TYPE_DATA);
            printf("S\x08");
            esx_f_write(file, buffer, hdr.hdlen);
            putchar(' ');
            blockno++;
            zx_tape_load_block(&hdr, sizeof(hdr), ZXT_TYPE_HEADER);
        } while (blockno < total);

    }

    esxdos_f_close(file);
    puts("\nDONE\n");

    return 0;
}

unsigned char* get_fname(unsigned char *fname, unsigned char *hdname) {
    unsigned char i;

    memcpy(fname, hdr.hdname, 10);
    fname[10] = 0;
    for (i=9; ' '==fname[i]; i--) {
        fname[i] = 0;
    }
    return fname;
}

// EOF vim: ts=4:sw=4:et:ai:
