#include <stdint.h>
#include <stdbool.h>

#define ELF_NIDENT 16

#define ELFMAG0	0x7F // e_ident[EI_MAG0]
#define ELFMAG1	'E'  // e_ident[EI_MAG1]
#define ELFMAG2	'L'  // e_ident[EI_MAG2]
#define ELFMAG3	'F'  // e_ident[EI_MAG3]

#define ELFDATA2LSB	(1)  // Little Endian
#define ELFCLASS64	(2)  // 64-bit Architecture

#define ET_NONE 0 // no file type
#define ET_REL 1 // relocatable file
#define ET_EXEC 2 // executable file
#define ET_DYN 3 // shared object file
#define ET_CORE 4 // core file

#define ET_LOPROC 0xff00 // processor-specific
#define ET_HIPROC 0xffff // processor-specific

#define EM_NONE   0  // No machine
#define EM_M32    1  // AT&T WE 32100
#define EM_SPARC  2  // SPARC
#define EM_386    3  // Intel 80386
#define EM_68K    4  // Motorola 68000
#define EM_88K    5  // Motorola 88000
#define EM_860    7  // Intel 80860
#define EM_MIPS   8  // MIPS RS3000

#define EM_ARM        40  // ARM
#define EM_SUPERH     42  // Hitachi SH
#define EM_IA_64      50  // Intel IA-64 processor architecture
#define EM_X86_64     62  // AMD x86-64 architecture
#define EM_AARCH64    183 // ARM 64-bit architecture (AArch64)
#define EM_RISCV      243 // RISC-V

#define EV_CURRENT 1

#define SHN_UNDEF	(0x00) // Undefined/Not Present
#define SHN_ABS    0xFFF1 // Absolute symbol

#define Elf64_ST_BIND(INFO)	((INFO) >> 4)
#define Elf64_ST_TYPE(INFO)	((INFO) & 0x0F)

#define PT_LOAD 1

#define PF_X        (1 << 0)    // Executable
#define PF_W        (1 << 1)    // Writable
#define PF_R        (1 << 2)    // Readable

#define PHDR(HDRP, IDX) \
    (void *)((uintptr_t)HDRP + (HDRP)->e_phoff + (HDRP->e_phentsize * IDX))

#define SHDR(HDRP, IDX) \
    (void *)((uintptr_t)HDRP + (HDRP)->e_shoff + (HDRP->e_shentsize * IDX))

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
typedef uint8_t Elf64_UnsignedChar;

typedef long long off_t; // 64-bit signed offset

enum Elf_Ident {
	EI_MAG0		= 0, // 0x7F
	EI_MAG1		= 1, // 'E'
	EI_MAG2		= 2, // 'L'
	EI_MAG3		= 3, // 'F'
	EI_CLASS	= 4, // Architecture (32/64)
	EI_DATA		= 5, // Byte Order
	EI_VERSION	= 6, // ELF Version
	EI_OSABI	= 7, // OS Specific
	EI_ABIVERSION	= 8, // OS Specific
	EI_PAD		= 9  // Padding
};

typedef struct {
	uint8_t		e_ident[ELF_NIDENT];
	Elf64_Half	e_type;
	Elf64_Half	e_machine;
	Elf64_Word	e_version;
	Elf64_Addr	e_entry;
	Elf64_Off	e_phoff;
	Elf64_Off	e_shoff;
	Elf64_Word	e_flags;
	Elf64_Half	e_ehsize;
	Elf64_Half	e_phentsize;
	Elf64_Half	e_phnum;
	Elf64_Half	e_shentsize;
	Elf64_Half	e_shnum;
	Elf64_Half	e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	Elf64_Word	sh_name;
	Elf64_Word	sh_type;
	Elf64_Xword	sh_flags;
	Elf64_Addr	sh_addr;
	Elf64_Off	sh_offset;
	Elf64_Xword	sh_size;
	Elf64_Word	sh_link;
	Elf64_Word	sh_info;
	Elf64_Xword	sh_addralign;
	Elf64_Xword	sh_entsize;
} Elf64_Shdr;

enum ShT_Types {
	SHT_NULL	= 0,   // Null section
	SHT_PROGBITS	= 1,   // Program information
	SHT_SYMTAB	= 2,   // Symbol table
	SHT_STRTAB	= 3,   // String table
	SHT_RELA	= 4,   // Relocation (w/ addend)
	SHT_NOBITS	= 8,   // Not present in file
	SHT_REL		= 9,   // Relocation (no addend)
};

enum ShT_Attributes {
	SHF_WRITE	= 0x01, // Writable section
	SHF_ALLOC	= 0x02  // Exists in memory
};

typedef struct {
	Elf64_Word		st_name;
	Elf64_Addr		st_value;
	Elf64_Word		st_size;
	uint8_t			st_info;
	uint8_t			st_other;
	Elf64_Half		st_shndx;
} Elf64_Sym;

enum StT_Bindings {
	STB_LOCAL		= 0, // Local scope
	STB_GLOBAL		= 1, // Global scope
	STB_WEAK		= 2  // Weak, (ie. __attribute__((weak)))
};

enum StT_Types {
	STT_NOTYPE		= 0, // No type
	STT_OBJECT		= 1, // Variables, arrays, etc.
	STT_FUNC		= 2  // Methods or functions
};

typedef struct {
    Elf64_Word   p_type;
    Elf64_Word   p_flags;
    Elf64_Off    p_offset;
    Elf64_Addr   p_vaddr;
    Elf64_Addr   p_paddr;
    Elf64_Xword  p_filesz;
    Elf64_Xword  p_memsz;
    Elf64_Xword  p_align;
} Elf64_Phdr;

typedef void (*entry_t)(void);

entry_t load_elf(void* file, bool exec);
void load_elf_from_disk(const char fn[11]);
void load_first_elf_from_disk();
void load_module_from_disk();
uint64_t get_size_of_elf();
