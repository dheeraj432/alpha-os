
#ifndef OUR_OS_HEAP_H
#define OUR_OS_HEAP_H

void heap_init(void);
void *kmalloc(unsigned int size);
int kfree(void *ptr);

#endif

