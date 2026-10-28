#include "heap.h"
#include "memory.h"

#define HEAP_MAGIC 0x48454150U
#define HEAP_ALIGNMENT 4U

struct heap_block {
    unsigned int size;
    unsigned int magic;
    unsigned int free;
    struct heap_block *next;
};

static struct heap_block *heap_head = 0;

static unsigned int align_size(unsigned int size)
{
    return (size + (HEAP_ALIGNMENT - 1U)) & ~(HEAP_ALIGNMENT - 1U);
}

void heap_init(void)
{
    heap_head = 0;
}

void *kmalloc(unsigned int size)
{
    if (size == 0)
        return 0;

    if (size > PAGE_SIZE - sizeof(struct heap_block))
        return 0;

    size = align_size(size);

    struct heap_block *block = heap_head;

    while (block != 0) {
        if (block->magic != HEAP_MAGIC)
            return 0;

        if (block->free && block->size >= size) {
            unsigned int remaining =
                block->size - size;

            if (remaining >=
                sizeof(struct heap_block) + HEAP_ALIGNMENT) {
                struct heap_block *split =
                    (struct heap_block *)
                    ((unsigned char *)(block + 1) + size);

                split->size =
                    remaining - sizeof(struct heap_block);
                split->magic = HEAP_MAGIC;
                split->free = 1;
                split->next = block->next;

                block->size = size;
                block->next = split;
            }

            block->free = 0;
            return (void *)(block + 1);
        }

        block = block->next;
    }

    unsigned int address = memory_page_alloc();

    if (address == 0)
        return 0;

    block = (struct heap_block *)address;
    block->size = PAGE_SIZE - sizeof(struct heap_block);
    block->magic = HEAP_MAGIC;
    block->free = 1;
    block->next = heap_head;
    heap_head = block;

    /*
     * Allocate from the newly created free block.
     */
    unsigned int remaining = block->size - size;

    if (remaining >=
        sizeof(struct heap_block) + HEAP_ALIGNMENT) {
        struct heap_block *split =
            (struct heap_block *)
            ((unsigned char *)(block + 1) + size);

        split->size = remaining - sizeof(struct heap_block);
        split->magic = HEAP_MAGIC;
        split->free = 1;
        split->next = block->next;

        block->size = size;
        block->next = split;
    }

    block->free = 0;
    return (void *)(block + 1);
}

int kfree(void *ptr)
{
    if (ptr == 0)
        return 0;

    struct heap_block *block = heap_head;

    while (block != 0) {
        if (block->magic != HEAP_MAGIC)
            return 0;

        if ((void *)(block + 1) == ptr) {
            if (block->free)
                return 0;

            block->free = 1;

            /*
             * Merge adjacent free blocks in the same
             * page when their metadata is contiguous.
             */
            struct heap_block *current = heap_head;

            while (current != 0) {
                if (current->magic != HEAP_MAGIC)
                    return 0;

                while (current->free &&
                       current->next != 0 &&
                       current->next->magic == HEAP_MAGIC &&
                       current->next->free &&
                       (unsigned char *)(current + 1)
                           + current->size ==
                       (unsigned char *)current->next) {
                    struct heap_block *next = current->next;

                    current->size +=
                        sizeof(struct heap_block) + next->size;
                    current->next = next->next;
                }

                current = current->next;
            }

            return 1;
        }

        block = block->next;
    }

    return 0;
}
