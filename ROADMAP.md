# ALPHA OS Roadmap

**Vision:** Build an operating system from the ground up, with AI as a central part of the user experience.

ALPHA OS is an experimental, community-driven project. Developers worldwide are welcome to contribute ideas, code, testing, and documentation.

## Phase 1 — Kernel Foundation

- [x] BIOS bootloader and kernel startup
- [x] 32-bit x86 freestanding C kernel
- [x] VGA text-mode terminal and basic commands
- [x] Physical page memory allocator
- [x] Kernel heap allocator
- [x] Interrupt Descriptor Table (IDT)
- [x] Timer interrupt
- [x] CPU exception diagnostics
- [ ] Standardized build system and documentation
- [ ] Automated boot tests in QEMU

## Phase 2 — Core Operating-System Services

- [ ] Improve interrupt and exception handling
- [ ] Virtual memory and memory protection
- [ ] Process and thread abstractions
- [ ] CPU scheduler and multitasking
- [ ] System calls and user-mode execution

## Phase 3 — Hardware and Applications

- [ ] Keyboard and input handling
- [ ] Storage drivers and filesystem
- [ ] Executable loading
- [ ] Basic user-space applications
- [ ] Networking foundations

## Phase 4 — AI-Centered Experience

- [ ] Command-driven assistant interface
- [ ] Speech input and spoken responses
- [ ] AI task execution through permission-controlled system services
- [ ] Explore local inference and offline capabilities

## Help Wanted

Contributors can help with:
- C and x86 Assembly
- Kernel architecture and debugging
- QEMU testing and automation
- Build tools and documentation
- Hardware research and device drivers
- AI systems and human-computer interaction

## Development Principles

- Build and test features incrementally.
- Keep the system understandable and maintainable.
- Document architectural decisions.
- Never claim a feature is complete until it has been tested.
- Treat security and user control as core design requirements.

This roadmap is a plan, not a guarantee of delivery dates. Priorities may change as the project develops.
