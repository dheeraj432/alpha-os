# OurOS

An experimental operating system built from the ground up using C and x86 Assembly.

OurOS is an educational operating-system project focused on developing a kernel foundation and eventually exploring an AI-centered operating-system experience.

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

OurOS is an experimental work in progress. Features are developed and tested incrementally; this is not yet a production-ready operating system.

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
