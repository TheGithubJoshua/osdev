#include <stddef.h>
#include <stdint.h>

typedef struct rsdp_t {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
} rsdp_t;
typedef struct rsdp_ext_t;
typedef struct fadt_t fadt_t;
typedef struct GenericAddressStructure GenericAddressStructure;

struct rsdp_t *get_acpi_table(void);
struct xsdt_t *get_xsdt_table(void);
fadt_t *get_fadt(void *RootSDT);
struct fadt_t *parse_fadt(void);
void *get_table(const char* signature, size_t index);