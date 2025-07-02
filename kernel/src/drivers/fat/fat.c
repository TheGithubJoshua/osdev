// modified from https://github.com/bztsrc/raspi3-tutorial/blob/master/0D_readfile/main.c

#include "../ahci/ahci.h"
#include "../../timer.h"
#include "../../mm/pmm.h"
#include "fat.h"
#include "../../iodebug.h"

// add memory compare, gcc has a built-in for that, clang needs implementation
#ifdef __clang__
static int memcmp(void *s1, void *s2, int n)
{
    unsigned char *a=s1,*b=s2;
    while(n-->0){ if(*a!=*b) { return *a-*b; } a++; b++; }
    return 0;
}
#else
#define memcmp __builtin_memcmp
#endif

static void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

// get the end of bss segment from linker
extern unsigned char _end;

static unsigned int partitionlba = 0;

// the BIOS Parameter Block (in Volume Boot Record)
typedef struct {
    char            jmp[3];
    char            oem[8];
    unsigned char   bps0;
    unsigned char   bps1;
    unsigned char   spc;
    unsigned short  rsc;
    unsigned char   nf;
    unsigned char   nr0;
    unsigned char   nr1;
    unsigned short  ts16;
    unsigned char   media;
    unsigned short  spf16;
    unsigned short  spt;
    unsigned short  nh;
    unsigned int    hs;
    unsigned int    ts32;
    unsigned int    spf32;
    unsigned int    flg;
    unsigned int    rc;
    char            vol[6];
    char            fst[8];
    char            dmy[20];
    char            fst2[8];
} __attribute__((packed)) bpb_t;

// directory entry structure
typedef struct {
    char            name[8];
    char            ext[3];
    char            attr[9];
    unsigned short  ch;
    unsigned int    attr2;
    unsigned short  cl;
    unsigned int    size;
} __attribute__((packed)) fatdir_t;

static unsigned char fat_workbuf[FAT_WORKBUF_SIZE];

static bpb_t *bpb;

/**
 * Get the starting LBA address of the first partition
 * so that we know where our FAT file system starts, and
 * read that volume's BIOS Parameter Block
 */
int fat_getpartition(void)
{
    unsigned char *mbr = (unsigned char *)palloc(1, true);  // one sector (512 bytes)
    if (!mbr) {
        serial_puts("ERROR: Allocation failed\n");
        return 0;
    }

    if (!ahci_readblock(0, mbr, 1)) {
        serial_puts("ERROR: Cannot read MBR\n");
        return 0;
    }

    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        serial_puts("ERROR: Bad magic in MBR\n");
        return 0;
    }

    if (mbr[0x1C2] != 0xE && mbr[0x1C2] != 0xC) {
        serial_puts("ERROR: Wrong partition type\n");
        return 0;
    }

    partitionlba = mbr[0x1C6] |
                   (mbr[0x1C7] << 8) |
                   (mbr[0x1C8] << 16) |
                   (mbr[0x1C9] << 24);

    // reuse mbr buffer for VBR
    if (!ahci_readblock(partitionlba, mbr, 1)) {
        serial_puts("ERROR: Cannot read VBR\n");
        return 0;
    }

    if (!ahci_readblock(partitionlba, fat_workbuf, 1)) {
        serial_puts("ERROR: Cannot read VBR into fat_workbuf\n");
        return 0;
    }


    bpb = (bpb_t *)mbr;

    // Debug statements to print BPB values
    serial_puts("BPB Values:\n");
    serial_puts("spf16: ");
    serial_puthex(bpb->spf16);
    serial_puts("\n");
    serial_puts("spf32: ");
    serial_puthex(bpb->spf32);
    serial_puts("\n");
    serial_puts("nf: ");
    serial_puthex(bpb->nf);
    serial_puts("\n");
    serial_puts("rsc: ");
    serial_puthex(bpb->rsc);
    serial_puts("\n");
    serial_puts("nr0: ");
    serial_puthex(bpb->nr0);
    serial_puts("\n");
    serial_puts("nr1: ");
    serial_puthex(bpb->nr1);
    serial_puts("\n");
    serial_puts("spc: ");
    serial_puthex(bpb->spc);
    serial_puts("\n");
    serial_puts("partitionlba: ");
    serial_puthex(partitionlba);
    serial_puts("\n");
    serial_puts("rc (Root Cluster): ");
    serial_puthex(bpb->rc);
    serial_puts("\n");

    if (!(bpb->fst[0] == 'F' && bpb->fst[1] == 'A' && bpb->fst[2] == 'T') &&
        !(bpb->fst2[0] == 'F' && bpb->fst2[1] == 'A' && bpb->fst2[2] == 'T')) {
        serial_puts("ERROR: Unknown filesystem type\n");
        return 0;
    }

    unsigned int *fat32 = (unsigned int*)(fat_workbuf + 512); // FAT starts after 1 sector
    unsigned short *fat16 = (unsigned short*)fat32;
    return 1;
}


/**
 * Find a file in root directory entries
 */
unsigned int fat_getcluster(char *fn)
{
    if (!bpb) {
        serial_puts("ERROR: BPB not initialized\n");
        return 0;
    }

    unsigned char t[FAT_WORKBUF_SIZE];

    // 1) Compute FirstDataSector = partitionlba + bpb->rsc + (bpb->nf * sectors_per_FAT)
    //    In FAT32, “sectors_per_FAT” is bpb->spf32, and “number_of_FATs” is bpb->nf.
    unsigned int first_data_sector = 
          partitionlba
        + bpb->rsc
        + (bpb->nf * bpb->spf32);

    serial_puts("FirstDataSector: "); serial_puthex(first_data_sector); serial_puts("\n");

    // 2) Root directory’s first cluster is bpb->rc
    unsigned int root_cluster = bpb->rc;
    serial_puts("Root directory start cluster: "); serial_puthex(root_cluster); serial_puts("\n");

    // 3) Load the first FAT into fat_workbuf so we can follow cluster chains.
    //    The first FAT starts at LBA = partitionlba + bpb->rsc, and is bpb->spf32 sectors long.
    unsigned int fat_lba = partitionlba + bpb->rsc;
    unsigned int fat_sectors = bpb->spf32;
    if (!ahci_readblock(fat_lba, t, fat_sectors)) {
        serial_puts("ERROR: Could not read FAT into memory\n");
        return 0;
    }
    // Now fat_workbuf[0..4*max_clusters] contains the FAT32 table.

    unsigned int *fat32 = (unsigned int *)t; 
    // (Each cluster entry is 4 bytes; valid cluster numbers are < 0x0FFFFFF8.)

    // 4) Walk the root directory’s cluster chain.  In each cluster:
    //    - Compute the first sector of that cluster: FirstSectorOfCluster = first_data_sector + (cluster–2)*bpb->spc
    //    - Read all bpb->spc sectors in that cluster
    //    - In each sector, scan the 16 directory entries of 32 bytes each
    //    - If entry[0] == 0x00, stop searching (end of directory)
    //    - Skip deleted (0xE5) and LFN (attr & 0x0F == 0x0F) entries.
    //    - Compare the 11-byte short name; if match, extract high+low cluster and return.

    unsigned int current_cluster = root_cluster;
    while (current_cluster < 0x0FFFFFF8) {
        unsigned int first_sector_of_cluster = 
              first_data_sector 
            + (current_cluster - 2) * bpb->spc;

        // Scan each sector in this cluster
        for (unsigned int sec_off = 0; sec_off < bpb->spc; sec_off++) {
            unsigned char sector_buf[512];
            if (!ahci_readblock(first_sector_of_cluster + sec_off, sector_buf, 1)) {
                serial_puts("ERROR: Failed to read directory sector\n");
                return 0;
            }

            // Scan each 32-byte entry in this sector
            for (int offset = 0; offset < 512; offset += 32) {
                unsigned char *entry = &sector_buf[offset];

                // 4a) End of directory?
                if (entry[0] == 0x00) {
                    // No more entries in this directory at all
                    return 0;
                }
                // 4b) Deleted entry?
                if (entry[0] == 0xE5) {
                    continue;
                }
                // 4c) Long File Name (LFN) entry?
                if ((entry[11] & 0x0F) == 0x0F) {
                    continue;
                }
                serial_puts("\nentry: ");
                serial_puts((const char*)entry);
                serial_puts("\nfn: ");
                serial_puts(fn);


                // 4d) Short-name slot. Compare its 11 bytes to fn[].
                if (memcmp(entry + 0, fn, 11) == 0) {
                    // Found it.  Extract high and low cluster words.
                    // In FAT32, the high word is at offset 20..21, low word at 26..27.
                    unsigned int high = ((unsigned int)entry[21] << 8) | entry[20];
                    unsigned int low  = ((unsigned int)entry[27] << 8) | entry[26]; // can be inverted idfk
                    unsigned int full_cluster = (high << 16) | low;

                    serial_puts("Found “"); 
                    // Print the short‐name (for debug)
                    for (int c = 0; c < 8; c++) {
                        if (entry[c] != ' ') serial_putc(entry[c]);
                    }
                    serial_puts(".");
                    for (int c = 0; c < 3; c++) {
                        if (entry[8 + c] != ' ') serial_putc(entry[8 + c]);
                    }
                    serial_puts("” at cluster ");
                    serial_puthex(full_cluster);
                    serial_puts("\n");

                    return full_cluster;
                }

                  serial_puts("Found file: ");
                    for (int i = 0; i < 8 && entry[i] != ' '; i++) serial_putc(entry[i]);
                    if (entry[8] != ' ') {
                        serial_putc('.');
                        for (int i = 8; i < 11 && entry[i] != ' '; i++) serial_putc(entry[i]);
                    }
                    serial_puts("\n");
            }
        }

        // 5) Move to the next cluster in the chain (if any)
        unsigned int next = fat32[current_cluster] & 0x0FFFFFFF;
        if (next >= 0x0FFFFFF8) {
            // Reached end‐of‐chain without finding the file
            break;
        }
        current_cluster = next;
    }

    serial_puts("ERROR: file not found in root directory\n");
    return 0;
}

unsigned int fat_read_entry(unsigned int cluster) {
    unsigned int fat_offset, fat_sector, ent_offset;
    unsigned char FAT_table[512];  // Assuming 512 bytes per sector

    if (bpb->spf16) {
        // FAT16 entry is 2 bytes each
        fat_offset = cluster * 2;
    } else {
        // FAT32 entry is 4 bytes each
        fat_offset = cluster * 4;
    }

    fat_sector = partitionlba + bpb->rsc + (fat_offset / 512);
    ent_offset = fat_offset % 512;

    // Read the sector containing the FAT entry
    if (!ahci_readblock(fat_sector, FAT_table, 1)) {
        serial_puts("ERROR reading FAT sector\n");
        return 0xFFFFFFFF; // Error sentinel
    }

    unsigned int entry_value;

    if (bpb->spf16) {
        // FAT16: read 2 bytes as unsigned short
        unsigned short *fat16 = (unsigned short *)FAT_table;
        entry_value = fat16[ent_offset / 2];
    } else {
        // FAT32: read 4 bytes as unsigned int, mask upper 4 bits
        unsigned int *fat32 = (unsigned int *)FAT_table;
        entry_value = fat32[ent_offset / 4] & 0x0FFFFFFF;
    }

    return entry_value;
}

/**
 * Read a file into memory
 */
char *fat_readfile(unsigned int cluster)
{
    serial_puts("\n(bpb->spf16 ? bpb->spf16 : bpb->spf32)");
    serial_puthex((bpb->spf16 ? bpb->spf16 : bpb->spf32));

    ahci_readblock(partitionlba + bpb->rsc, fat_workbuf + 512, (bpb->spf16 ? bpb->spf16 : bpb->spf32));
    // static probably not good
    unsigned char *t = palloc((FAT_WORKBUF_SIZE + PAGE_SIZE - 1) / PAGE_SIZE, true);

    ahci_readblock(partitionlba + bpb->rsc, t + 512, (bpb->spf16 ? bpb->spf16 : bpb->spf32));

    serial_puts("\n");
    unsigned int *fat32 = (unsigned int*)(t + 512); // FAT starts after 1 sector
    unsigned short *fat16 = (unsigned short*)fat32;
    unsigned int data_sec;
    unsigned char *data = t + 32 * 1024;  // Half for FAT, half for file
    unsigned char *ptr = data;

    // Compute where the data section begins
    data_sec = partitionlba + bpb->rsc + (bpb->nf * (bpb->spf16 ? bpb->spf16 : bpb->spf32));

    // Load the FAT
    serial_puts("\nbpb->spc: ");
    serial_puthex(bpb->spc);

    serial_puts("\nfatworkbuf: ");
    for (int i = 0; i < 16; i++) {
        serial_puthex(((uint8_t *)fat_workbuf)[i]);
        serial_puts(" ");
    }
    serial_puts("\n");

    serial_puts("partitionlba = ");
    serial_puthex(partitionlba);
    serial_puts("\n");

    serial_puts("bpb->rsc = ");
    serial_puthex(bpb->rsc);
    serial_puts("\n");

    serial_puts("FAT starts at LBA: ");
    serial_puthex(partitionlba + bpb->rsc);
    serial_puts("\n");

    unsigned int fat_sectors = bpb->spf16 ? bpb->spf16 : bpb->spf32;
    serial_puts("FAT sectors: ");
    serial_puthex(fat_sectors);
    serial_puts("\n");


    serial_puts("\nFAT count: ");
    serial_puthex(bpb->nf);           // Should be ≥1
    serial_puts("\nSectors per FAT: ");
    unsigned int spf = bpb->spf16 ? bpb->spf16 : bpb->spf32;
    serial_puthex(spf);              // Should be ≥1

    // Check hidden sectors (offset 0x1C-0x1F)
    unsigned hidden_sec = *(unsigned*)(t + 0x1C);
    serial_puts("Hidden sectors: ");
    serial_puthex(hidden_sec);  // Should match partition offset

    serial_puts("Raw FAT (16 bytes): ");
    for (int i = 0; i < 16; i++) {
        serial_puthex(t[512 + i]);
        serial_puts(" ");
    }
    serial_puts("\n");

    for (int i = 0; i < 16; i++) {
        serial_puts("FAT[");
        serial_puthex(i);
        serial_puts("] = ");
        serial_puthex((bpb->spf16 ? fat16[i] : fat32[i]));
        serial_puts("\n");
    }

    serial_puts("FAT16 entries:\n");
    for (int i = 0; i < 10; i++) {
        serial_puthex(fat16[i]);
        serial_puts(" ");
    }
    serial_puts("\n");

    serial_puts("FAT32 entries:\n");
    for (int i = 0; i < 10; i++) {
        serial_puthex(fat32[i]);
        serial_puts(" ");
    }
    serial_puts("\n");

    serial_puts("bps0: ");
    serial_puthex(bpb->bps0);
    serial_puts("\nbps1: ");
    serial_puthex(bpb->bps1);

    // Traverse cluster chain and load file content
   while (cluster > 1 && cluster < (bpb->spf16 ? 0xFFF8 : 0x0FFFFFF8)) {
    unsigned int lba = data_sec + (cluster - 2) * bpb->spc;
    ahci_readblock(lba, ptr, bpb->spc);

    ptr += bpb->spc * (bpb->bps0 + (bpb->bps1 << 8));

    // Debug print before reading FAT entry
    serial_puts("Current cluster: ");
    serial_puthex(cluster);
    serial_puts("\n");

        unsigned int next_cluster = fat_read_entry(cluster);

    serial_puts("Next cluster from FAT: ");
    serial_puthex(next_cluster);
    serial_puts("\n");

    // Sanity check:
    if (next_cluster == 0) {
        serial_puts("WARNING: FAT entry is zero — possible free cluster or corruption.\n");
        break;  // Or handle error
    }

    cluster = next_cluster;
}
    return (char*)data;
}

void convert_to_fat8_3(const char *input, char fn[11]) {
    // Initialize the output array with spaces
    memset(fn, ' ', sizeof(fn));

    // Variables to track positions in base name and extension
    int base_pos = 0;
    int ext_pos = 9; // Start of extension (8.3 format has a space between base and extension)

    while (*input) {
        if (*input == '.') {
            // If a dot is found, switch to the extension part
            input++;
            continue;
        }

        if (base_pos < 8 && *input != ' ') {
            fn[base_pos++] = *input;
        } else if (ext_pos < 11) {
            fn[ext_pos++] = *input;
        }

        input++;
    }
}
