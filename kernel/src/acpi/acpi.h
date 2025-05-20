struct rsdp_t *get_acpi_table(void);
struct xsdt_t *get_xsdt_table(void);
void get_fadt (struct xsdt_t *xsdt);
struct fadt_t *parse_fadt(void);

typedef struct rsdp_t rsdp_t;
typedef struct rsdp_ext_t;
typedef struct fadt_t fadt_t;
typedef struct GenericAddressStructure GenericAddressStructure;