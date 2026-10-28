#include "memory/memory.h"
#include "memory/heap.h"
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_SIZE (VGA_WIDTH * VGA_HEIGHT)
#define INPUT_MAX 79

static volatile unsigned short *const VGA =
    (unsigned short *)0xB8000;

static int cursor = 0;
static int history_key = 0;
static int shift_pressed = 0;
static int caps_lock = 0;


struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short offset_high;
} __attribute__((packed));

struct idt_pointer {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_pointer idtr;

extern void idt_load(unsigned int idtr_address);
extern void exception_stub_table(void);
extern void timer_irq_stub(void);

static volatile unsigned int timer_ticks = 0;

void timer_handler(void)
{
    timer_ticks++;
}

static void exception_write(unsigned int *position,
                            const char *text)
{
    while (*text) {
        VGA[(*position)++] =
            (unsigned short)(0x4F00 | (unsigned char)*text++);
    }
}

static void exception_write_hex(unsigned int *position,
                                unsigned int value)
{
    const char hex[] = "0123456789ABCDEF";

    for (int shift = 28; shift >= 0; shift -= 4) {
        VGA[(*position)++] =
            (unsigned short)(
                0x4F00 | hex[(value >> shift) & 0xF]
            );
    }
}

__attribute__((noreturn))
void exception_handler(unsigned int vector,
                       unsigned int error_code)
{
    __asm__ volatile ("cli");

    unsigned int position = 0;

    exception_write(&position, "FATAL CPU EXCEPTION");

    position = VGA_WIDTH;
    exception_write(&position, "Exception vector: 0x");
    exception_write_hex(&position, vector);

    position = VGA_WIDTH * 2;
    exception_write(&position, "Error code:      0x");
    exception_write_hex(&position, error_code);

    position = VGA_WIDTH * 3;
    exception_write(&position, "System halted.");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
static void idt_set_gate(unsigned char vector,
                         unsigned int handler,
                         unsigned short selector,
                         unsigned char flags)
{
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = flags;
    idt[vector].offset_high = (handler >> 16) & 0xFFFF;
}


static void idt_init(void)
{
    unsigned int *exception_stubs =
        (unsigned int *)exception_stub_table;

    for (int i = 0; i < 256; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    /* Install CPU exception handlers: vectors 0-31. */
    for (int i = 0; i < 32; i++) {
        idt_set_gate(
            (unsigned char)i,
            exception_stubs[i],
            0x08,
            0x8E
        );
    }

    /* Install the timer interrupt gate at vector 32. */
    idt_set_gate(
        32,
        (unsigned int)timer_irq_stub,
        0x08,
        0x8E
    );

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (unsigned int)&idt;

    idt_load((unsigned int)&idtr);
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1"
                      : : "a"(value), "Nd"(port));
}


static inline void io_wait(void)
{
    outb(0x80, 0);
}

static void pic_remap(void)
{
    /* Keep CPU interrupts disabled during PIC setup. */
    __asm__ volatile ("cli");

    /* Start PIC initialization sequence. */
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    /* Move master IRQs to vectors 32-39 and slave IRQs to 40-47. */
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();

    /* Tell the master and slave PICs how they are connected. */
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();

    /* Set 8086 mode. */
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    /* Unmask only timer IRQ0; mask all other hardware IRQs. */
    outb(0x21, 0xFE);
    outb(0xA1, 0xFF);
}

static void pit_init(void)
{
    /* PIT channel 0, low byte then high byte, square-wave mode. */
    unsigned int divisor = 11932; /* Approximately 100 Hz */

    outb(0x43, 0x34);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

static unsigned char rtc_read(unsigned char reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static unsigned char from_bcd(unsigned char value)
{
    return (value & 0x0F) + ((value >> 4) * 10);
}

static void read_rtc(unsigned char *hour,
                     unsigned char *minute,
                     unsigned char *second,
                     unsigned char *day,
                     unsigned char *month,
                     unsigned char *year)
{
    unsigned char status_b;
    unsigned char h, m, s, d, mo, y;

    /* Wait until the RTC is not updating. */
    while (rtc_read(0x0A) & 0x80)
        ;

    s  = rtc_read(0x00);
    m  = rtc_read(0x02);
    h  = rtc_read(0x04);
    d  = rtc_read(0x07);
    mo = rtc_read(0x08);
    y  = rtc_read(0x09);
    status_b = rtc_read(0x0B);

    /* If the RTC uses BCD, convert values to decimal. */
    if (!(status_b & 0x04)) {
        s  = from_bcd(s);
        m  = from_bcd(m);
        h  = (h & 0x80) | from_bcd(h & 0x7F);
        d  = from_bcd(d);
        mo = from_bcd(mo);
        y  = from_bcd(y);
    }

    /* Convert 12-hour format to 24-hour format if needed. */
    if (!(status_b & 0x02)) {
        unsigned char pm = h & 0x80;
        h &= 0x7F;

        if (pm)
            h = (h % 12) + 12;
        else
            h = h % 12;
    }

    *hour = h;
    *minute = m;
    *second = s;
    *day = d;
    *month = mo;
    *year = y;
}


static void update_cursor(void)
{
    unsigned short position = (unsigned short)cursor;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position & 0xFF));

    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));
}

static void clear_screen(void)
{
    for (int i = 0; i < VGA_SIZE; i++)
        VGA[i] = 0x0F20;

    cursor = 0;
    update_cursor();
}

static void scroll_screen(void)
{
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            VGA[(row - 1) * VGA_WIDTH + col] =
                VGA[row * VGA_WIDTH + col];
        }
    }

    for (int col = 0; col < VGA_WIDTH; col++)
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = 0x0F20;

    cursor = (VGA_HEIGHT - 1) * VGA_WIDTH;
}

static void putchar(char c)
{
    if (c == '\n') {
        cursor = ((cursor / VGA_WIDTH) + 1) * VGA_WIDTH;
    } else if (c == '\b') {
        if (cursor > 0) {
            cursor--;
            VGA[cursor] = 0x0F20;
        }
    } else {
        if (cursor >= VGA_SIZE)
            scroll_screen();

        VGA[cursor++] = 0x0F00 | (unsigned char)c;
    }

    if (cursor >= VGA_SIZE)
        scroll_screen();
    update_cursor();
}

static void print(const char *s)
{
    while (*s)
        putchar(*s++);
}

static void print_number(unsigned int n)
{
    char digits[10];
    int count = 0;

    if (n == 0) {
        putchar('0');
        return;
    }

    while (n > 0 && count < 10) {
        digits[count++] = '0' + (n % 10);
        n /= 10;
    }

    while (count > 0)
        putchar(digits[--count]);
}


static void print_hex32(unsigned int value)
{
    const char hex[] = "0123456789ABCDEF";

    for (int shift = 28; shift >= 0; shift -= 4) {
        putchar(hex[(value >> shift) & 0xF]);
    }
}

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

static const char keymap[128] = {
    [0x02]='1', [0x03]='2', [0x04]='3',
    [0x05]='4', [0x06]='5', [0x07]='6',
    [0x08]='7', [0x09]='8', [0x0A]='9',
    [0x0B]='0', [0x0C]='-', [0x0D]='=',
    [0x0E]='\b', [0x0F]='\t',
    [0x10]='q', [0x11]='w', [0x12]='e',
    [0x13]='r', [0x14]='t', [0x15]='y',
    [0x16]='u', [0x17]='i', [0x18]='o',
    [0x19]='p', [0x1A]='[', [0x1B]=']',
    [0x1C]='\n',
    [0x1E]='a', [0x1F]='s', [0x20]='d',
    [0x21]='f', [0x22]='g', [0x23]='h',
    [0x24]='j', [0x25]='k', [0x26]='l',
    [0x27]=';', [0x28]='\'', [0x29]='`',
    [0x2B]='\\', [0x2C]='z', [0x2D]='x',
    [0x2E]='c', [0x2F]='v', [0x30]='b',
    [0x31]='n', [0x32]='m', [0x33]=',',
    [0x34]='.', [0x35]='/', [0x39]=' '
};

static char get_key(void)
{
    while (!(inb(0x64) & 1))
        ;

    unsigned char scancode = inb(0x60);

    /* Extended keys: Up and Down arrows */
    if (scancode == 0xE0) {
        while (!(inb(0x64) & 1))
            ;

        unsigned char ext = inb(0x60);

        if (ext == 0x48)
            history_key = 1;
        else if (ext == 0x50)
            history_key = 2;

        return 0;
    }

    /* Shift pressed */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }

    /* Shift released */
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return 0;
    }

    /* Ignore other key releases */
    if (scancode & 0x80)
        return 0;

    /* Caps Lock */
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return 0;
    }

    if (scancode >= 128)
        return 0;

    char c = keymap[scancode];

    if (c >= 'a' && c <= 'z') {
        if (shift_pressed != caps_lock)
            c = c - 'a' + 'A';
    } else if (shift_pressed) {
        switch (scancode) {
            case 0x02: return '!';
            case 0x03: return '@';
            case 0x04: return '#';
            case 0x05: return '$';
            case 0x06: return '%';
            case 0x07: return '^';
            case 0x08: return '&';
            case 0x09: return '*';
            case 0x0A: return '(';
            case 0x0B: return ')';
            case 0x0C: return '_';
            case 0x0D: return '+';
            default: break;
        }
    }

    return c;
}

static void prompt(void)
{
    print("OurOS> ");
}

/* Erase only the characters in the current input. */
static void erase_input(char *command, int *length)
{
    while (*length > 0) {
        (*length)--;
        cursor--;
        VGA[cursor] = 0x0F20;
    }

    command[0] = '\0';
}

static void execute_command(const char *cmd)
{
    if (streq(cmd, "help")) {
        print("Available commands:\n");
        print("  help   - Show commands\n");
        print("  about  - About OurOS\n");
        print("  sysinfo - Show system information\n");
        print("  meminfo - Show detected usable RAM\n");
        print("E820 entries: ");
        print_number(*(volatile unsigned short *)0x7000);
        putchar('\n');
        print("  time    - Show hardware date and time\n");
        print("  uptime - Show system uptime\n");
        print("  clear  - Clear the screen\n");
        print("  reboot - Restart OurOS\n");
    } else if (streq(cmd, "about")) {
        print("OurOS - a custom operating system.\n");
        print("Built with Assembly and C.\n");
    } else if (streq(cmd, "sysinfo")) {
        print("OurOS System Information\n");
        print("Architecture : x86 (32-bit)\n");
        print("Boot Mode    : BIOS\n");
        print("Terminal     : VGA Text Mode\n");
        print("Kernel       : OurOS C Kernel\n");
    } else if (streq(cmd, "time")) {        unsigned char hour, minute, second;
        unsigned char day, month, year;

        read_rtc(&hour, &minute, &second,
                &day, &month, &year);

        print("Hardware RTC Date and Time:\n");

        print("Date: ");
        putchar('0' + day / 10);
        putchar('0' + day % 10);
        putchar('/');
        putchar('0' + month / 10);
        putchar('0' + month % 10);
        putchar('/');
        putchar('2');
        putchar('0');
        putchar('0' + year / 10);
        putchar('0' + year % 10);

        print("\nTime: ");
        putchar('0' + hour / 10);
        putchar('0' + hour % 10);
        putchar(':');
        putchar('0' + minute / 10);
        putchar('0' + minute % 10);
        putchar(':');
        putchar('0' + second / 10);
        putchar('0' + second % 10);
        putchar('\n');

    } else if (streq(cmd, "uptime")) {
        unsigned int ticks = timer_ticks;
        unsigned int seconds = ticks / 100;

        print("OurOS uptime: ");
        print_number(seconds);
        print(" seconds\n");
    } else if (streq(cmd, "meminfo")) {
        unsigned int low = memory_total_usable_low();
        unsigned int high = memory_total_usable_high();

        print("OurOS Memory Information\n");
        print("E820 entry count: ");
        print_number(*(volatile unsigned short *)0x7000);
        putchar('\n');
        print("Usable RAM (bytes, hexadecimal): 0x");
        print_hex32(high);
        print_hex32(low);
        putchar('\n');
    volatile unsigned short *map_count =
        (volatile unsigned short *)0x7000;

    volatile unsigned int *map =
        (volatile unsigned int *)0x7004;

    for (unsigned int i = 0; i < *map_count; i++) {
      unsigned int offset = i * 6;

      print("\nEntry ");
      print_number(i);
      print("\n  Base: 0x");
      print_hex32(map[offset + 1]);
      print_hex32(map[offset]);

      print("\n  Length: 0x");
      print_hex32(map[offset + 3]);
      print_hex32(map[offset + 2]);

      print("\n  Type: ");
      print_number(map[offset + 4]);
      putchar('\n');

      if ((i + 1) % 2 == 0 && i + 1 < *map_count) {
        print("\nPress any key for next entries...");
          while (!(inb(0x64) & 0x01))
                      ;
          inb(0x60);
          putchar('\n');
      }
    }

    } else if (streq(cmd, "heaplimit")) {
        print("Heap Boundary Test\n");

        void *zero = kmalloc(0);

        if (zero == 0) {
            print("PASS: Zero-size allocation rejected\n");
        } else {
            print("FAIL: Zero-size allocation accepted\n");
        }

        unsigned int max_size =
            PAGE_SIZE - 16U;

        void *largest = kmalloc(max_size);

        if (largest != 0) {
            print("PASS: Maximum-size allocation succeeded\n");
            kfree(largest);
        } else {
            print("FAIL: Maximum-size allocation failed\n");
        }

        void *too_large = kmalloc(PAGE_SIZE);

        if (too_large == 0) {
            print("PASS: Oversized allocation rejected\n");
        } else {
            print("FAIL: Oversized allocation accepted\n");
            kfree(too_large);
        }

        void *huge = kmalloc(0xFFFFFFFFU);

        if (huge == 0) {
            print("PASS: Integer-overflow request rejected\n");
        } else {
            print("FAIL: Integer-overflow request accepted\n");
            kfree(huge);
        }

  } else if (streq(cmd, "heaptest")) {
    print("Kernel Heap Test\n");

    unsigned int free_before = memory_pages_free();
    unsigned int allocated_before = memory_pages_allocated();

    unsigned int *a = (unsigned int *)kmalloc(sizeof(unsigned int));
    unsigned int *b = (unsigned int *)kmalloc(64);

    if (a == 0 || b == 0) {
        print("FAIL: Heap allocation failed\n");

        if (a != 0)
            kfree(a);

        if (b != 0)
            kfree(b);
    } else {
        *a = 0x12345678U;
        ((unsigned char *)b)[0] = 0x5A;

        if (*a == 0x12345678U &&
            ((unsigned char *)b)[0] == 0x5A) {
            print("PASS: Memory read/write succeeded\n");
        } else {
            print("FAIL: Memory read/write failed\n");
        }

        int freed_a = kfree(a);
        int freed_b = kfree(b);
        int double_free = kfree(a);
        int invalid_free = kfree((void *)0x1234U);

        if (freed_a == 1 &&
            freed_b == 1 &&
            double_free == 0 &&
            invalid_free == 0) {
            print("PASS: Free validation succeeded\n");
        } else {
            print("FAIL: Free validation failed\n");
        }
          /*
          * Verify that a freed heap block can be reused
          * without consuming another physical page.
          */
          unsigned int free_before_reuse = memory_pages_free();
          unsigned int allocated_before_reuse = memory_pages_allocated();

          unsigned int *c = (unsigned int *)kmalloc(4);

          unsigned int free_after_reuse = memory_pages_free();
          unsigned int allocated_after_reuse = memory_pages_allocated();

          if (c != 0 &&
             free_after_reuse == free_before_reuse &&
             allocated_after_reuse == allocated_before_reuse) {
             print("PASS: Freed heap memory reused without a new page\n");
          } else {
               print("FAIL: Heap reuse test failed\n");
          }

           if (c != 0)
           kfree(c);
 }

    /*
     * Check that page accounting remains consistent.
     * Heap blocks can be reused, so do not assume each
     * heaptest run allocates exactly two new pages.
     */
    unsigned int free_after = memory_pages_free();
    unsigned int allocated_after = memory_pages_allocated();

    if (allocated_after >= allocated_before &&
        free_after <= free_before &&
        (allocated_after - allocated_before) ==
        (free_before - free_after)) {
        print("PASS: Heap page accounting correct\n");
    } else {
        print("FAIL: Heap page accounting incorrect\n");
    }
    } else if (streq(cmd, "heapstress")) {
        print("Heap Stress Test\n");

        unsigned int free_before = memory_pages_free();
        unsigned int allocated_before = memory_pages_allocated();

        unsigned char *a = (unsigned char *)kmalloc(64);
        unsigned char *b = (unsigned char *)kmalloc(64);
        unsigned char *c = (unsigned char *)kmalloc(64);

        if (a == 0 || b == 0 || c == 0) {
            print("FAIL: Initial allocations failed\n");

            if (a != 0) kfree(a);
            if (b != 0) kfree(b);
            if (c != 0) kfree(c);
        } else {
            if (a != b && a != c && b != c) {
                print("PASS: Allocations are separate\n");
            } else {
                print("FAIL: Allocations overlap\n");
            }

            a[0] = 0x11;
            b[0] = 0x22;
            c[0] = 0x33;

            if (a[0] == 0x11 &&
                b[0] == 0x22 &&
                c[0] == 0x33) {
                print("PASS: Allocation data is isolated\n");
            } else {
                print("FAIL: Allocation data was corrupted\n");
            }

            unsigned int allocated_during =
                memory_pages_allocated() - allocated_before;

            if (allocated_during == 1) {
                print("PASS: Small allocations share one page\n");
            } else {
                print("FAIL: Unexpected number of heap pages\n");
            }

            int free_a = kfree(a);
            int free_b = kfree(b);
            int free_c = kfree(c);

            if (free_a == 1 && free_b == 1 && free_c == 1) {
                print("PASS: All stress allocations freed\n");
            } else {
                print("FAIL: Could not free all allocations\n");
            }

            /*
             * The three adjacent 64-byte blocks should
             * coalesce into a sufficiently large block.
             */
            unsigned int free_before_large = memory_pages_free();
            unsigned int allocated_before_large =
                memory_pages_allocated();

            void *large = kmalloc(256);

            if (large != 0 &&
                memory_pages_free() == free_before_large &&
                memory_pages_allocated() == allocated_before_large) {
                print("PASS: Coalesced space reused\n");
            } else {
                print("FAIL: Coalesced space reuse failed\n");
            }

            if (large != 0)
                kfree(large);
        }

        if (memory_pages_free() +
                memory_pages_allocated() ==
            free_before + allocated_before) {
            print("PASS: Physical page accounting preserved\n");
        } else {
            print("FAIL: Physical page accounting changed\n");
        }
    } else if (streq(cmd, "pagetest")) {
        unsigned int free_before = memory_pages_free();
        unsigned int allocated_before = memory_pages_allocated();

        print("Page Allocator Test\n");
        print("Free pages before: ");
        print_number(free_before);
        print("\nAllocated pages before: ");
        print_number(allocated_before);
        putchar('\n');

        unsigned int page = memory_page_alloc();

        if (page == 0) {
            print("FAIL: Page allocation failed\n");
        } else {
            print("Allocated page at: 0x");
            print_hex32(page);
            putchar('\n');

            int freed = memory_page_free(page);
            int double_free = memory_page_free(page);

            if (freed == 1 && double_free == 0 &&
                memory_pages_free() == free_before &&
                memory_pages_allocated() == allocated_before) {
                print("PASS: Page allocator test succeeded!\n");
            } else {
                print("FAIL: Page allocator test detected a problem\n");
            }
        }

    } else if (streq(cmd, "exhausttest")) {
        static unsigned int page_addresses[32768];

        unsigned int free_before = memory_pages_free();
        unsigned int allocated_before = memory_pages_allocated();
        unsigned int count = 0;
        unsigned int page;

        print("Memory Exhaustion Test\n");
        print("Allocating pages until failure...\n");

        while (count < 32768 &&
               (page = memory_page_alloc()) != 0) {
            page_addresses[count] = page;
            count++;
        }

        print("Pages allocated during test: ");
        print_number(count);
        putchar('\n');

        print("Allocation after exhaustion: ");
        print_number(memory_page_alloc());
        putchar('\n');

        if (memory_pages_free() != 0 ||
            memory_pages_allocated() != allocated_before + count) {
            print("FAIL: Counters incorrect at exhaustion\n");
        } else {
            print("PASS: Exhaustion detected correctly\n");
        }

        /* Free only pages actually allocated by this test. */
        for (unsigned int i = 0; i < count; i++) {
            if (!memory_page_free(page_addresses[i])) {
                print("FAIL: Could not free a test page\n");
            }
        }

        if (memory_pages_free() == free_before &&
            memory_pages_allocated() == allocated_before) {
            print("PASS: Memory state restored\n");
        } else {
            print("FAIL: Memory counters did not restore\n");
        }
    } else if (streq(cmd, "clear")) {
        clear_screen();
    } else if (streq(cmd, "reboot")) {
        print("Rebooting...\n");

        while (inb(0x64) & 0x02)
            ;

        outb(0x64, 0xFE);

        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    } else if (cmd[0] != '\0') {
        print("Unknown command: ");
        print(cmd);
        putchar('\n');
        print("Type help to see commands.\n");
    }
}

void kernel_main(void)
{
    char command[80];
    char last_command[80] = "";
    char draft[80];

    int length;
    int draft_length = 0;
    int browsing_history = 0;

    /* Read the BIOS memory map prepared by Stage 2. */
    unsigned short map_count =
        *(volatile unsigned short *)0x7000;

    struct memory_map_entry *memory_map =
        (struct memory_map_entry *)0x7004;

    memory_init(memory_map, map_count);
    heap_init();

    /* Initialize the interrupt system in a safe order. */
    idt_init();
    pic_remap();
    pit_init();

    /* Enable hardware interrupts only after setup is complete. */
    __asm__ volatile ("sti");

    clear_screen();

    print("========================================\n");
    print("             Welcome to OurOS           \n");
    print("========================================\n");
    print("OurOS terminal is ready.\n");
    print("Type help to list commands.\n\n");

    for (;;) {
        prompt();

        length = 0;
        command[0] = '\0';
        browsing_history = 0;
        draft_length = 0;

        for (;;) {
            char c = get_key();

            /* Process Up/Down history navigation. */
            if (history_key != 0) {
                int requested = history_key;
                history_key = 0;

                if (requested == 1 &&
                    last_command[0] != '\0' &&
                    !browsing_history) {

                    /* Save unfinished input. */
                    draft_length = length;

                    for (int i = 0; i < length; i++)
                        draft[i] = command[i];

                    draft[draft_length] = '\0';
                    browsing_history = 1;

                    erase_input(command, &length);

                    /* Display previous command. */
                    for (int i = 0; last_command[i]; i++) {
                        command[length++] = last_command[i];
                        putchar(last_command[i]);
                    }

                    command[length] = '\0';

                } else if (requested == 2 && browsing_history) {

                    erase_input(command, &length);

                    /* Restore unfinished input. */
                    for (int i = 0; i < draft_length; i++) {
                        command[length++] = draft[i];
                        putchar(draft[i]);
                    }

                    command[length] = '\0';
                    browsing_history = 0;
                }

                continue;
            }

            if (!c)
                continue;

            if (c == '\n') {
                putchar('\n');
                command[length] = '\0';
                break;
            }

            if (c == '\b') {
                if (length > 0) {
                    length--;
                    cursor--;
                    VGA[cursor] = 0x0F20;
                    command[length] = '\0';
                }

                browsing_history = 0;
                continue;
            }

            if (c >= 32 && c <= 126 && length < INPUT_MAX) {
                command[length++] = c;
                command[length] = '\0';
                putchar(c);

                /* Editing a recalled command starts a new input. */
                browsing_history = 0;
            }
        }

        /* Save the most recently executed non-empty command. */
        if (length > 0) {
            int i = 0;

            while (command[i] && i < INPUT_MAX) {
                last_command[i] = command[i];
                i++;
            }

            last_command[i] = '\0';
        }

        execute_command(command);
    }
}
