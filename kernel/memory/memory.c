
#include "memory.h"

#define MIN_ALLOC_ADDRESS 0x00100000U
#define MAX_TRACKED_PAGES 32768U

static struct memory_map_entry memory_map[MEMORY_MAP_MAX_ENTRIES];
static unsigned int memory_map_count = 0;

static unsigned int usable_memory_low = 0;
static unsigned int usable_memory_high = 0;

/*
 * One bit per 4 KiB page:
 * 1 = unavailable or allocated
 * 0 = free
 *
 * 32768 pages track 128 MiB.
 */
static unsigned char page_bitmap[MAX_TRACKED_PAGES / 8];
static unsigned char allocated_bitmap[MAX_TRACKED_PAGES / 8];

static unsigned int free_page_count = 0;
static unsigned int allocated_page_count = 0;

static void set_page_used(unsigned int page)
{
    page_bitmap[page / 8] |= (unsigned char)(1U << (page % 8));
}

static void set_page_free(unsigned int page)
{
    page_bitmap[page / 8] &= (unsigned char)~(1U << (page % 8));
}

static int page_is_used(unsigned int page)
{
    return (page_bitmap[page / 8] & (1U << (page % 8))) != 0;
}

static void set_page_allocated(unsigned int page)
{
    allocated_bitmap[page / 8] |=
        (unsigned char)(1U << (page % 8));
}

static void clear_page_allocated(unsigned int page)
{
    allocated_bitmap[page / 8] &=
        (unsigned char)~(1U << (page % 8));
}

static int page_is_allocated(unsigned int page)
{
    return (allocated_bitmap[page / 8] &
            (1U << (page % 8))) != 0;
}

void memory_init(struct memory_map_entry *map, unsigned int count)
{
    memory_map_count = 0;
    usable_memory_low = 0;
    usable_memory_high = 0;
    free_page_count = 0;
    allocated_page_count = 0;

    /* Initially reserve every tracked page. */
    for (unsigned int i = 0; i < sizeof(page_bitmap); i++) {
      page_bitmap[i] = 0xFF;
      allocated_bitmap[i] = 0;
    }

    if (count > MEMORY_MAP_MAX_ENTRIES)
        count = MEMORY_MAP_MAX_ENTRIES;

    for (unsigned int i = 0; i < count; i++) {
        memory_map[i] = map[i];

        if (map[i].type == 1) {
            unsigned int old_low = usable_memory_low;
            usable_memory_low += map[i].length_low;
            usable_memory_high += map[i].length_high;

            if (usable_memory_low < old_low)
                usable_memory_high++;
        }

        memory_map_count++;
    }

    /*
     * Mark pages free only when the entire 4 KiB page is
     * inside a type-1 region, above 1 MiB, and below 128 MiB.
     *
     * This allocator intentionally tracks only the first
     * 128 MiB of physical address space.
     */
    for (unsigned int i = 0; i < memory_map_count; i++) {
        if (memory_map[i].type != 1 || memory_map[i].base_high != 0)
            continue;

        unsigned int base = memory_map[i].base_low;
        unsigned int length = memory_map[i].length_low;

        if (memory_map[i].length_high != 0)
            length = 0xFFFFFFFFU - base;

        if (base < MIN_ALLOC_ADDRESS) {
            unsigned int skipped = MIN_ALLOC_ADDRESS - base;

            if (length <= skipped)
                continue;

            base = MIN_ALLOC_ADDRESS;
            length -= skipped;
        }

        if (base >= MAX_TRACKED_PAGES * PAGE_SIZE)
            continue;

        unsigned int end;

        if (length > 0xFFFFFFFFU - base)
            end = 0xFFFFFFFFU;
        else
            end = base + length;

        if (end > MAX_TRACKED_PAGES * PAGE_SIZE)
            end = MAX_TRACKED_PAGES * PAGE_SIZE;

        /* Round inward: never free a partial page. */
        unsigned int first_page = (base + PAGE_SIZE - 1) / PAGE_SIZE;
        unsigned int end_page = end / PAGE_SIZE;

        for (unsigned int page = first_page; page < end_page; page++) {
            if (page_is_used(page)) {
                set_page_free(page);
                free_page_count++;
            }
        }
    }
}

unsigned int memory_total_usable_low(void)
{
    return usable_memory_low;
}

unsigned int memory_total_usable_high(void)
{
    return usable_memory_high;
}

unsigned int memory_page_alloc(void)
{
    if (free_page_count == 0)
            return 0;
    for (unsigned int page = MIN_ALLOC_ADDRESS / PAGE_SIZE;
         page < MAX_TRACKED_PAGES;
         page++) {
      if (!page_is_used(page)) {
        set_page_used(page);
        set_page_allocated(page);
        free_page_count--;
        allocated_page_count++;
        return page * PAGE_SIZE;
      }
    }

    return 0;
}

int memory_page_free(unsigned int address)
{
    if (address % PAGE_SIZE != 0)
        return 0;

    unsigned int page = address / PAGE_SIZE;

    if (page < MIN_ALLOC_ADDRESS / PAGE_SIZE ||
        page >= MAX_TRACKED_PAGES)
        return 0;

    if (!page_is_allocated(page))
        return 0;

    clear_page_allocated(page);
    set_page_free(page);

    free_page_count++;
    allocated_page_count--;

    return 1;
}
unsigned int memory_pages_free(void)
{
    return free_page_count;
}

unsigned int memory_pages_allocated(void)
{
    return allocated_page_count;
}

