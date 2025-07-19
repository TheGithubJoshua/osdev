#include "elf.h"
#include "../iodebug.h"
#include "memory.h"
#include "../thread/thread.h"
#include "../mm/pmm.h"
#include "../userspace/enter.h"
#include "../drivers/fat/fat.h"
#include <limine.h>
#include <stddef.h>
#include <stdint.h>

Elf64_Phdr *ph;
uint64_t elf_size;

static struct limine_internal_module internal_module = {
    .path = "module",
    //.cmdline = NULL,
};

// Array of pointers to internal_module
static struct limine_internal_module *internal_modules[] = {
    &internal_module,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,

    .internal_module_count = 1,
    .internal_modules = internal_modules,
};

static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

bool elf_check_file(Elf64_Ehdr *hdr) {
	if(!hdr) return false;
	if(hdr->e_ident[EI_MAG0] != ELFMAG0) {
		serial_puts("ELF Header EI_MAG0 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG1] != ELFMAG1) {
		serial_puts("ELF Header EI_MAG1 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG2] != ELFMAG2) {
		serial_puts("ELF Header EI_MAG2 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG3] != ELFMAG3) {
		serial_puts("ELF Header EI_MAG3 incorrect.\n");
		return false;
	}
	return true;
}

bool elf_check_supported(Elf64_Ehdr *hdr) {
	if(!elf_check_file(hdr)) {
		serial_puts("Invalid ELF File.\n");
		return false;
	}
	if(hdr->e_ident[EI_CLASS] != ELFCLASS64) {
		serial_puts("Unsupported ELF File Class.\n");
		return false;
	}
	if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
		serial_puts("Unsupported ELF File byte order.\n");
		return false;
	}
	if(hdr->e_machine != EM_X86_64) {
		serial_puts("Unsupported ELF File target.\n");
		return false;
	}
	if(hdr->e_ident[EI_VERSION] != EV_CURRENT) {
		serial_puts("Unsupported ELF File version.\n");
		return false;
	}
	if(hdr->e_type != ET_REL && hdr->e_type != ET_EXEC) {
		serial_puts("Unsupported ELF File type.\n");
		return false;
	}
	return true;
}

static inline Elf64_Shdr *elf_sheader(Elf64_Ehdr *hdr) {
	return (Elf64_Shdr *)((int)hdr + hdr->e_shoff);
}

static inline Elf64_Shdr *elf_section(Elf64_Ehdr *hdr, int idx) {
	return &elf_sheader(hdr)[idx];
}

Elf64_Shdr *elf_find_section_by_type(Elf64_Ehdr *hdr, uint32_t type) {
    for (int i = 0; i < hdr->e_shnum; i++) {
        Elf64_Shdr *sh = elf_section(hdr, i);
        if (sh->sh_type == type)
            return sh;
    }
    return NULL;
}

Elf64_Shdr *elf_find_symtab(Elf64_Ehdr *hdr) {
    return elf_find_section_by_type(hdr, SHT_SYMTAB);
}

Elf64_Shdr *elf_find_strtab_for_symtab(Elf64_Ehdr *hdr, Elf64_Shdr *symtab) {
    if (!symtab)
        return NULL;
    return elf_section(hdr, symtab->sh_link);
}

const Elf64_Sym *elf_lookup_symbol(Elf64_Ehdr *hdr, const char *name) {
    Elf64_Shdr *symtab = elf_find_symtab(hdr);
    Elf64_Shdr *strtab = elf_find_strtab_for_symtab(hdr, symtab);

    if (!symtab || !strtab)
        return NULL;

    Elf64_Sym *sym = (Elf64_Sym *)((uint8_t *)hdr + symtab->sh_offset);
    size_t sym_count = symtab->sh_size / sizeof(Elf64_Sym);
    const char *strtab_base = (const char *)hdr + strtab->sh_offset;

    for (size_t i = 0; i < sym_count; i++, sym++) {
        const char *sym_name = strtab_base + sym->st_name;

        if (sym_name && strcmp(sym_name, name) == 0) {
            // Found symbol, return pointer to it
            return sym;
        }
    }

    return NULL;
}

static int elf_get_symval(Elf64_Ehdr *hdr, int table, unsigned int idx) {
	if(table == SHN_UNDEF || idx == SHN_UNDEF) return 0;
	Elf64_Shdr *symtab = elf_section(hdr, table);

	uint32_t symtab_entries = symtab->sh_size / symtab->sh_entsize;
	if(idx >= symtab_entries) {
		serial_puts("Symbol Index out of Range\n");
		return 1;
	}

	int symaddr = (int)hdr + symtab->sh_offset;
	Elf64_Sym *symbol = &((Elf64_Sym *)symaddr)[idx];

		if(symbol->st_shndx == SHN_UNDEF) {
		// External symbol, lookup value
		Elf64_Shdr *strtab = elf_section(hdr, symtab->sh_link);
		const char *name = (const char *)hdr + strtab->sh_offset + symbol->st_name;

		void *target = elf_lookup_symbol(hdr, name);

		if(target == NULL) {
			// Extern symbol not found
			if(Elf64_ST_BIND(symbol->st_info) & STB_WEAK) {
				// Weak symbol initialized as 0
				return 0;
			} else {
				serial_puts("Undefined External Symbol");
				return 1;
			}
		} else {
			return (int)target;
		}

			} else if(symbol->st_shndx == SHN_ABS) {
		// Absolute symbol
		return symbol->st_value;
	} else {
		// Internally defined symbol
		Elf64_Shdr *target = elf_section(hdr, symbol->st_shndx);
		return (int)hdr + symbol->st_value + target->sh_offset;
	}
}

static int elf_load_stage1(Elf64_Ehdr *hdr) {
	Elf64_Shdr *shdr = elf_sheader(hdr);

	unsigned int i;
	// Iterate over section headers
	for(i = 0; i < hdr->e_shnum; i++) {
		Elf64_Shdr *section = &shdr[i];

		// If the section isn't present in the file
		if(section->sh_type == SHT_NOBITS) {
			// Skip if it the section is empty
			if(!section->sh_size) continue;
			// If the section should appear in memory
			if(section->sh_flags & SHF_ALLOC) {
				// Allocate and zero some memory
				void *mem = palloc(section->sh_size, true);
				memset(mem, 0, section->sh_size);

				// Assign the memory offset to the section offset
				section->sh_offset = (int)mem - (int)hdr;
				serial_puts("Allocated memory for a section.\n");
			}
		}
	}
	return 0;
}

static inline Elf64_Phdr *elf_program_header(Elf64_Ehdr *hdr, int idx) {
    return (Elf64_Phdr *)((uint8_t *)hdr + hdr->e_phoff) + idx;
}

#define PAGE_SIZE 0x1000
#define ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#define ALIGN_UP(x)   (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

void *elf_load_exec(Elf64_Ehdr *hdr) {

	for (int i = 0; i < hdr->e_phnum; i++) {
	    Elf64_Phdr *ph = elf_program_header(hdr, i);
	    serial_puts("Segment ");
	    serial_puthex(i);
	    serial_puts(": type=");
	    serial_puthex(ph->p_type);
	    serial_puts(" vaddr=");
	    serial_puthex(ph->p_vaddr);
	    serial_puts(" memsz=");
	    serial_puthex(ph->p_memsz);
	    serial_puts(" filesz=");
	    serial_puthex(ph->p_filesz);
	    serial_puts("\n");
	}

    // Map all PT_LOAD segments first
    for (int i = 0; i < hdr->e_phnum; i++) {
        Elf64_Phdr *ph = elf_program_header(hdr, i);
        if (ph->p_type != PT_LOAD)
            continue;

        uint64_t start = ALIGN_DOWN(ph->p_vaddr);
        uint64_t end = ALIGN_UP(ph->p_vaddr + ph->p_memsz);
        if (end == start) end = start + PAGE_SIZE; // map at least one page

        for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
            quickmap(addr, addr);
        }

    }

        if (hdr->e_phnum == 0) {
        serial_puts("No program headers found.\n");
        return NULL;
    }

    // Copy segment contents and zero .bss
    for (int i = 0; i < hdr->e_phnum; i++) {
        Elf64_Phdr *ph = elf_program_header(hdr, i);
        if (ph->p_type != PT_LOAD)
            continue;

        void *dest = (void *)(uintptr_t)ph->p_vaddr; // virtual address
        void *src = (uint8_t *)hdr + ph->p_offset;
serial_puts("\nsrc: ");
        serial_puthex((uint64_t)src);

        memcpy(dest, src, ph->p_filesz);

        if (ph->p_memsz > ph->p_filesz) {
            memset((uint8_t *)dest + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
        }
    }
    serial_puts("e_entry from load_exec: ");
    serial_puthex(hdr->e_entry);
    serial_puts("\n");
    return (void *)(uintptr_t)hdr->e_entry;
}


static inline void *elf_load_rel(Elf64_Ehdr *hdr) {
	int result;
	result = elf_load_stage1(hdr);
	if(result == 1) {
		serial_puts("Unable to load ELF file.\n");
		return NULL;
	}
	/*result = elf_load_stage2(hdr);
	if(result == 1) {
		serial_puts("Unable to load ELF file.\n");
		return NULL;
	}
	*/
	// TODO : Parse the program header (if present)
	return (void *)hdr->e_entry;
}

void *elf_load_file(void *file) {
	Elf64_Ehdr *hdr = (Elf64_Ehdr *)file;
	if(!elf_check_supported(hdr)) {
		serial_puts("ELF File cannot be loaded.\n");
		return 0;
	}
	switch(hdr->e_type) {
		case ET_EXEC:
			return elf_load_exec(hdr);
		case ET_REL:
			return elf_load_rel(hdr);
	}
	return NULL;
}

entry_t load_segment_to_memory(uint64_t pml4_phys, Elf64_Phdr *phdr, uint64_t segment_phys_addr) {
    uint64_t vaddr_start = phdr->p_vaddr;
    uint64_t memsz = phdr->p_memsz;
    uint64_t filesz = phdr->p_filesz;

    // Calculate page-aligned start and end
    uint64_t vaddr_page_start = vaddr_start & ~(PAGE_SIZE - 1);
    uint64_t vaddr_page_end = (vaddr_start + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Offset inside first page
    uint64_t offset_in_first_page = vaddr_start & (PAGE_SIZE - 1);

    // Determine flags
    uint64_t flags = 0;
    if (phdr->p_flags & PF_R) flags |= PAGE_PRESENT;
    if (phdr->p_flags & PF_W) flags |= PAGE_WRITABLE;
    if (!(phdr->p_flags & PF_X)) flags |= PAGE_NO_EXECUTE;

    // Map each page
    uint64_t num_pages = (vaddr_page_end - vaddr_page_start) / PAGE_SIZE;

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t va = vaddr_page_start + i * PAGE_SIZE;
        uint64_t pa = segment_phys_addr - offset_in_first_page + i * PAGE_SIZE;
        
        map_page(pml4_phys, va, pa, flags);
    }

    // Zero BSS: if memsz > filesz, zero from (vaddr_start + filesz) to (vaddr_start + memsz)
    if (memsz > filesz) {
        uint64_t bss_start = vaddr_start + filesz;
        uint64_t bss_size = memsz - filesz;

        // Zero the memory region starting at bss_start for bss_size bytes.
        // You have to be sure this memory is writable and mapped (already mapped above).
        memset((void *)bss_start, 0, bss_size);
    }
}

// takes the address of an ELF file in memory and executes it
entry_t load_elf(void *file, bool exec) {
uint64_t pml4_phys_addr = read_cr3();

Elf64_Ehdr *ehdr = file;
quickmap(ehdr->e_entry, ehdr->e_entry);

serial_puts("\ne_version: ");
serial_puthex(ehdr->e_version); 

bool result = elf_check_file(ehdr);
if (result) { serial_puts("elf is good!"); }
result = elf_check_supported(ehdr);
if (result) { serial_puts("elf is supported!"); }
result = elf_load_file(file);
if (result) { serial_puts("file loaded!"); } else { serial_puts("loading failed!"); }
//void *entry = elf_load_file(ehdr);

uintptr_t entry_offset = /*ehdr->e_entry*/1000;  // ELF entry point relative to load base // fix me
uintptr_t base = (uintptr_t)file;
serial_puts("\nbase: ");
serial_puthex(base);
serial_puts("\nentry_offset: ");
serial_puthex(entry_offset);
entry_t entry = (entry_t)(base + entry_offset);

serial_puthex((uint64_t)(uintptr_t)entry);

Elf64_Phdr *phdrs = (Elf64_Phdr *)((uint8_t *)ehdr + ehdr->e_phoff);
for (int i = 0; i < ehdr->e_phnum; i++) {
    Elf64_Phdr *ph = &phdrs[i];
    if (ph->p_type != PT_LOAD) continue;

    //load_segment_to_memory(pml4_phys_addr, ph, (uint64_t)(uintptr_t)entry);

    void *dest = (void *)(uintptr_t)ph->p_vaddr;
    void *src = (uint8_t *)ehdr + ph->p_offset;


serial_puts("p_vaddr: ");
serial_puthex(ph->p_vaddr);
serial_puts(" p_memsz: ");
serial_puthex(ph->p_memsz);
serial_puts("\n");

   // Copy segment contents
    memcpy(dest, src, ph->p_filesz);

    // Zero the rest (.bss)
    //memset((uint8_t *)dest + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
}


for (int i = 0; i < ehdr->e_phnum; i++) {
    ph = &phdrs[i];
    if (ph->p_type != PT_LOAD) continue;

    uint64_t seg_start = ph->p_vaddr & ~0xFFF;
    uint64_t seg_end   = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFF;

    for (uint64_t addr = seg_start; addr < seg_end; addr += 0x1000) {
        uint64_t phys = (uint64_t)palloc(1, false); // Allocate physical memory
        uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        map_page(pml4_phys_addr, addr, phys, flags);
    }

    // Now copy segment data and zero .bss
    void *dest = (void *)(uintptr_t)ph->p_vaddr;
    void *src = (uint8_t *)ehdr + ph->p_offset;
    memcpy(dest, src, ph->p_filesz);

    if (ph->p_memsz > ph->p_filesz) {
        memset((uint8_t *)dest + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
    }

    serial_puts("Mapped segment at vaddr: ");
    serial_puthex(ph->p_vaddr);
    serial_puts("\n");

    uint8_t *code = (uint8_t *)ph->p_vaddr;
    serial_puts("Code bytes: ");
    for (int j = 0; j < 16; j++) {
        serial_puthex(code[j]);
        serial_puts(" ");
    }
    serial_puts("\n");

    elf_size = ph->p_filesz; // fix me
    serial_puts("file_size: ");
    serial_puthex(elf_size);

}

if (entry != NULL) {
	//quickmap(0x0000000000403010,0x0000000000403010);
	//asm volatile ("jmp %[entry]" : : [entry] "r"((uint64_t)(uintptr_t)0xFFFF80007BC18000)); // Jump to the entry point
    //task_create_wrap(entry);
    //Elf64_Phdr *ph = elf_program_header(ehdr, i);
    uint8_t *bytePointer = (uint8_t *)entry;
    serial_puts("\nentry: ");
    for (int i = 0; i < 16; i++) {
        serial_puthex((uint64_t)bytePointer[i]);
        serial_puts(" ");
    }
    serial_puts("addr of entry(): ");
    serial_puthex((uint64_t)entry);
    serial_puts("task created!");
        if (exec) {
    serial_puts("file: ");
    serial_puthex((uint64_t)file);
}
    //quickmap(0x00000000000003F8, 0x00000000000003F8);
    if (exec) {
    entry();  // jump into the loaded ELF executable
} else {
	serial_puts("(elf loaded, will not execute)");
	serial_puts("e_entry: ");
	serial_puthex(ehdr->e_entry);
	return entry + 0x0C18; // why (im not questioning it)
}
} else {
    serial_puts("Failed to load ELF.\n");
}
//task_exit();
}

void load_elf_from_disk(const char fn[11]) {

task_exit();
}

// demo of loading elf from disk
void load_module_from_disk() {
	const char *fn = "module";
	char *fd = fat_read(fn);
	load_elf(fd, true);
	task_exit();
}

uint64_t get_size_of_elf() {
	uint64_t vaddr_start = ph->p_vaddr;
	uint64_t memsz = ph->p_memsz;
	uint64_t filesz = ph->p_filesz;

	// Calculate page-aligned start and end
	uint64_t vaddr_page_start = vaddr_start & ~(PAGE_SIZE - 1);
	uint64_t vaddr_page_end = (vaddr_start + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	elf_size = vaddr_page_end - vaddr_page_start;
	serial_puts("elf size before ret: ");
	serial_puthex(elf_size);
	return elf_size;
}