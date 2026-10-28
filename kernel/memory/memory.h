#ifndef OUR_OS_MEMORY_H
#define OUR_OS_MEMORY_H

#define MEMORY_MAP_MAX_ENTRIES 32
#define PAGE_SIZE 4096U

struct memory_map_entry {
    unsigned int base_low;
    unsigned int base_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
    unsigned int acpi;
};

void memory_init(struct memory_map_entry *map, unsigned int count);

unsigned int memory_total_usable_low(void);
unsigned int memory_total_usable_high(void);

unsigned int memory_page_alloc(void);
int memory_page_free(unsigned int address);
unsigned int memory_pages_free(void);
unsigned int memory_pages_allocated(void);

#endif
