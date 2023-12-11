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

void get_fname(unsigned char *fname, unsigned char *hdname) {
    unsigned char i;

    memcpy(fname, hdr.hdname, 10);
    fname[10] = 0;
    for (i=9; ' '==fname[i]; i--) {
        fname[i] = 0;
    }
}

void load_header(unsigned int expected) {
    unsigned char rc;
    printf("?\x08");
    do {
        rc = zx_tape_load_block(&hdr, sizeof(hdr), ZXT_TYPE_HEADER);
        if (!rc) continue;
        if (BULK_ID == hdr.hdtype) {
            if (expected != hdr.hdadd) {
                printf("!\x08");
                continue;
            }
        }
        //if (BULK_ID == hdr.hdtype && hdr.hdlen > BUFFER_SIZE) {
            //printf("Block too big %d (max %d)\n", hdr.hdlen, BUFFER_SIZE);
            //continue;
        //}
        // TODO: break
    } while(BULK_ID != hdr.hdtype);
}

unsigned char copy_chunk() {
    unsigned char rc;
    rc = zx_tape_load_block(buffer, hdr.hdlen, ZXT_TYPE_DATA);
    if (0 == rc) {
        printf("S\x08");
        esx_f_write(file, buffer, hdr.hdlen);
        // TODO: check write errors
        putchar(' ');
    } else {
        printf("E\x08");
    }
    return rc;
}

int main(void) {
    unsigned int total, blockno;
    unsigned char fname[11];

    z80_bpoke(23692, 255); // disable "scroll ?" prompts
    printf("tap2disk Bulk Transfer \x7f TIsland\n");

    if (z80_wpeek(23730) >= (unsigned int)RAM_ADDRESS) {
        printf("M RAMPTOP no good (%u)", (unsigned int)RAM_ADDRESS);
        return 1;
    }

    load_header(1);

    get_fname(fname, hdr.hdname);
    blockno = hdr.hdadd;
    total = hdr.hdvars;
    printf("'%s' - %u chunks\n", fname, total);

    file = esx_f_open(fname, ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    if (0xff != file) {
        do {
            printf("%uL\x08", blockno);
            if (0 == copy_chunk()) {
                blockno++;
            }   
            if (blockno < total) {
                load_header(blockno);
            } else {
                break;
            }
        } while (1);

    }

    esxdos_f_close(file);
    printf("\n%s DONE\n", fname);

    return 0;
}

// EOF vim: ts=4:sw=4:et:ai:
