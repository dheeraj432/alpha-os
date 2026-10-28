# ALPHA OS

Assembling Leading Programmers & Hackers All-around

An experimental operating system built from scratch using C and x86 Assembly, with a long-term vision of AI-centered interaction.

ALPHA OS welcomes developers, programmers, systems engineers, and hackers worldwide to help build its foundation.

## Current Features

- BIOS bootloader and kernel startup
- 32-bit x86 freestanding C kernel
- VGA text-mode terminal
- Basic terminal commands
- Physical page memory allocator
- Kernel heap allocator
- Interrupt Descriptor Table (IDT)
- Programmable Interval Timer (PIT) interrupt
- CPU exception diagnostics
- QEMU-based testing

## Development Status

ALPHA OS is an experimental work in progress. Features are developed and tested incrementally; this is not yet a production-ready operating system.

Planned areas include:
- Multitasking and CPU scheduling
- Virtual memory and memory protection
- Filesystem support
- Device drivers and networking
- User-space applications and system calls
- AI-assisted operating-system interaction

## Build Environment

The current development environment uses:
- Ubuntu on WSL2
- GCC with 32-bit freestanding compilation support
- NASM
- GNU binutils
- QEMU

## Build and Run

Build commands and boot-image generation steps will be documented as the build process is standardized.

The current kernel and bootloader are under active development.

## Contributing

Contributions, bug reports, design discussions, and documentation improvements are welcome.

Please open an issue to discuss substantial changes before submitting a pull request.

## License

A license has not yet been selected. Until one is added, do not assume that the source is available for unrestricted reuse.
