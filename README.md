![Screenshot of Blink running GCC 9.4.0](blink/blink-gcc.png)

# Blinkenlights

This project contains two programs:

`blink` is a virtual machine that runs statically-compiled x86-64-linux
programs on different operating systems and hardware architectures. It's
designed to do the same thing as the `qemu-x86_64` command, except (a)
rather than being a 4mb binary, Blink only has a ~156kb footprint; and
(b) Blink goes 2x faster than Qemu on some benchmarks such as emulating
GCC. The tradeoff is Blink doesn't have as many features as Qemu. Blink
is a great fit when you want a virtual machine that's extremely small
and runs ephemeral programs much faster. For further details on the
motivations for this tool, please read <https://justine.lol/ape.html>.

[`blinkenlights`](https://justine.lol/blinkenlights) is a TUI interface
that may be used for debugging x86_64-linux programs across platforms.
Unlike GDB, Blinkenlights focuses on visualizing program execution. It
uses UNICODE IBM Code Page 437 characters to display binary memory
panels, which change as you step through your program's assembly code.
These memory panels may be scrolled and zoomed using your mouse wheel.
Blinkenlights also permits reverse debugging, where scroll wheeling over
the assembly display allows the rewinding of execution history.

## Getting Started

We regularly test that Blink is able run x86-64-linux binaries on the
following platforms:

- Linux (x86, ARM, RISC-V, MIPS, PowerPC, s390x)
- MacOS (x86, ARM)
- FreeBSD
- OpenBSD
- NetBSD

Blink depends on the following libraries:

- libc (POSIX.1-2017)

Blink can be compiled on UNIX systems that have:

- A C11 compiler (e.g. GCC 4.9.4+)
- Modern GNU Make (i.e. not the one that comes with XCode)

The instructions for compiling Blink are as follows:

```sh
$ make -j4
$ o//blink/blink -h
Usage: o//blink/blink [-hjms] PROG [ARGS...]
  -h        help
  -j        disable jit
  -m        enable memory safety
  -s        print statistics on exit
```

Here's how you can run a simple hello world program with Blink:

```sh
o//blink/blink third_party/cosmo/tinyhello.elf
```

Blink has a debugger TUI, which works with UTF-8 ANSI terminals. The
most important keystrokes in this interface are `?` for help, `s` for
step, `c` for continue, and scroll wheel for reverse debugging.

```sh
o//blink/blinkenlights third_party/cosmo/tinyhello.elf
```

## Testing

Blink is tested primarily using precompiled x86 binaries, which are
downloaded automatically. You can check how well Blink works on your
local platform by running:

```sh
make check
```

To check that Blink works on 11 different hardware `$(ARCHITECTURES)`
(see [Makefile](Makefile)), you can run the following command, which
will download statically-compiled builds of GCC and Qemu. Since our
toolchain binaries are intended for x86-64 Linux, Blink will bootstrap
itself locally first, so that it's possible to run these tests on other
operating systems and architectures.

```sh
make check2
make emulates
```

## Alternative Builds

For maximum performance, use `MODE=rel` or `MODE=opt`. Please note the
release mode builds will remove all the logging and assertion statements
and Blink isn't mature enough for that yet. So extra caution is advised.

```sh
make MODE=rel
o/rel/blink/blink -h
```

For maximum tinyness, use `MODE=tiny`. This build mode will not only
remove logging and assertion statements, but also reduce performance in
favor of smaller binary size whenever possible.

```sh
make MODE=tiny
strip o/tiny/blink/blink
ls -hal o/tiny/blink/blink
```

You can hunt down bugs in Blink using the following build modes:

- `MODE=asan` helps find memory safety bugs
- `MODE=tsan` helps find threading related bugs
- `MODE=ubsan` to find violations of the C standard
- `MODE=msan` helps find uninitialized memory errors

## Technical Details

blink is an x86-64 interpreter for POSIX platforms that's written in
ANSI C11 that's compatible with C++ compilers. Instruction decoding is
done using our trimmed-down version of Intel's disassembler Xed.

The prime directive of this project is to act as a virtual machine for
userspace binaries compiled by Cosmopolitan Libc. However we've also had
success virtualizing programs compiled with Glibc and Musl Libc, such as
GCC and Qemu. Blink supports 130+ Linux system call ABIs, including
fork() and clone(). Linux system calls may only be used by long mode
programs via the `SYSCALL` instruction, as it is written in the System V
ABI.

### Instruction Sets

The following hardware ISAs are supported by Blink.

- i8086
- i386
- X87
- SSE2
- x86_64
- SSE3
- SSSE3
- CLMUL
- POPCNT
- ADX
- BMI2
- RDRND
- RDSEED
- RDTSCP

Programs may use `CPUID` to confirm the presence or absence of optional
instruction sets. Please note that Blink does not follow the same
monotonic progress as Intel's hardware. For example, BMI2 is supported;
this is an AVX2-encoded (VEX) instruction set, which Blink is able to
decode, even though the AVX2 ISA isn't supported. Therefore it's
important to not glob ISAs into "levels" (as Windows software tends to
do) where it's assumed that BMI2 support implies AVX2 support; because
with Blink that currently isn't the case.

On the other hand, Blink does share Windows' x87 behavior w.r.t. double
(rather than long double) precision. It's not possible to use 80-bit
floating point precision with Blink, because Blink simply passes along
floating point operations to the host architecture, and very few
architectures support `long double` precision. You can still use x87
with 80-bit words. Blink will just store 64-bit floating point values
inside them, and that's a legal configuration according to the x87 FPU
control word. If possible, it's recommended that `long double` simply be
avoided. If 64-bit floating point [is good enough for the rocket
scientists at
NASA](https://www.jpl.nasa.gov/edu/news/2016/3/16/how-many-decimals-of-pi-do-we-really-need/)
then it should be good enough for everybody. There are some peculiar
differences in behavior with `double` across architectures (which Blink
currently does nothing to address) but they tend to be comparatively
minor, e.g. an op returning `NAN` instead of `-NAN`.

Blink has reasonably comprehensive coverage of the baseline ISAs,
including even support for BCD operations (even in long mode!) But there
are some truly fringe instructions Blink hasn't implemented, such as
`BOUND` and `ENTER`. Most of the unsupported instructions, are usually
ring-0 system instructions, since Blink is primarily a user-mode VM, and
therefore only has limited support for bare metal operating system
software (which we'll discuss more in-depth in a later section).

Blink itself may be detected via `CPUID` by checking for the vendor
string `GenuineBlink` (rather than the `GenuineIntel` / `AuthenticAMD`
brands that get reported normally). Please note that old versions of
Blinkenlights use the brand `GenuineCosmo`. Blink also identifies itself
as `blink 4.0` via the `uname()` system call. We report a false version
because otherwise, every program that links Glibc will refuse to run.

### JIT

Blink uses just-in-time compilation, which is supported on x86_64 and
aarch64. Blink takes the appropriate steps to work around restrictions
relating to JIT, on platforms like Apple and OpenBSD. We generate JIT
code using a printf-style domain-specific language. The JIT works by
generating functions at runtime which call the micro-op functions the
compiler created. To make micro-operations go faster, Blink determines
the byte length of the compiled function at runtime by scanning for a
RET instruction. Blink will then copy the compiled function into the
function that the JIT is generating. This works in most cases, however
some tools can cause problems. For example, OpenBSD RetGuard inserts
static memory relocations into every compiled function, which Blink's
JIT currently doesn't understand; so we need to use compiler flags to
disable that type of magic. In the event other such magic slips through,
Blink has a runtime check which will catch obvious problems, and then
gracefully fall back to using a CALL instruction. Since no JIT can be
fully perfect on all platforms, the `o//blink/blink -j` flag may be
passed to disable Blink's JIT. Please note that disabling JIT makes
Blink go 10x slower. With the `o//blink/blinkenlights` command, the `-j`
flag takes on the opposite meaning, where it instead *enables* JIT. This
can be useful for troubleshooting the JIT, because the TUI display has a
feature that lets JIT path formation be visualized. Blink currently only
enables the JIT for programs running in long mode (64-bit) but we may
support JITing 16-bit programs in the future.

### Virtualization

Blink virtualizes memory using the same PML4T approach as the hardware
itself, where memory lookups are indirected through a four-level radix
tree. Since performing four separate page table lookups on every memory
access can be slow, Blink checks a translation lookaside buffer, which
contains the sixteen most recently used page table entries. The PML4T
allows all memory lookups in Blink to be "safe" but it still doesn't
offer the best possible performance. Therefore, on systems with a huge
address space (i.e. petabytes of virtual memory) Blink relies on itself
being loaded to a random location, and then identity maps guest memory
using a simple linear translation. For example, if the guest virtual
address is `0x400000` then the host address might be
`0x400000+0x088800000000`. This means that each time a memory operation
is executed, only a simple addition needs to be performed. This goes
extremely fast, however it may present issues for programs that use
`MAP_FIXED`. Some systems, such as modern Raspberry Pi, actually have a
larger address space than x86-64, which lets Blink offer the guest the
complete address space. However on some platforms, like 32-bit ones,
only a limited number of identity mappings are possible. There's also
compiler tools like TSAN which lay claim to much of the fixed address
space. Blink's solution is designed to meet the needs of Cosmopolitan
Libc, while working around Apple's restriction on 32-bit addresses, and
still remain fully compatible with ASAN's restrictions. In the event
that this translation scheme doesn't work on your system, the `blink -m`
flag may be passed to disable the linear translation optimization, and
instead use only the memory safe full virtualization approach of the
PML4T and TLB.

Blink has an xterm-compatible ANSI teletypewriter display implementation
which allows Blink's TUI interface to host other TUI programs, within an
embedded terminal display. For example, it's possible to use Antirez's
Kilo text editor inside Blink's TUI.

Blink supports 16-bit BIOS programs, such as SectorLISP. To boot real
mode programs in Blink, the `o//blink/blinkenlights -r` flag may be
passed, which puts the virtual machine in i8086 mode. Currently only a
limited set of BIOS APIs are available. For example, Blink supports IBM
PC Serial UART, CGA display, and the MDA display APIs which are rendered
using block characters in the TUI interface. We hope to expand our real
mode support in the near future, in order to run operating systems like
ELKS.

Blink supports troubleshooting operating system bootloaders. Blink was
designed for Cosmopolitan Libc, which embeds an operating system in each
binary it compiles. Blink has helped us debug our bare metal support,
since Blink is capable of running in the 16-bit, 32-bit, and 64-bit
modes a bootloader requires at various stages. In order to do that, we
needed to implement some ring0 hardware instructions. Blink has enough
to support Cosmopolitan, but it'll take much more time to get Blink to a
point where it can boot something like Windows.

Blink supports several different executable formats, all of which are
static. You can run:

- Actually Portable Executables, which have either the `MZqFpD` or
  `jartsr` magic.

- Statically-compiled x86-64-linux ELF executables, so long as they
  don't use PIC/PIE or require a interpreter.

- Flat executables, which must end with the file extension `.bin`. In
  this case, you can make executables as small as 10 bytes in size,
  since they're treated as raw x86-64 code. Blink always loads flat
  executables to the address `0x400000` and automatically appends 16mb
  of BSS memory.

- Real mode executables, which are loaded to the address `0x7c00`. These
  programs must be run using the `blinkenlights` command with the `-r`
  flag.
