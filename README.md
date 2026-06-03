################################################
#                      AOS                     #
################################################

1. What is AOS?

AOS is a kernel developed from scratch by Oppeko.

The main motive is simple:
if you bought your hardware, why should your OS waste it?

I know this is hard, but I will do it.

AOS is meant to be:
- light
- fast
- open source
- low bloat
- hardware friendly
- Linux inspired, but with its own AOS identity

AOS is open sourced and licensed with GPL 3.0.


2. What AOS can do right now

AOS is not just a mockup anymore.

Right now AOS can:

- boot into its own kernel
- start an interactive shell
- run custom userspace programs
- run BusyBox
- run some GNU coreutils
- load ELF64 userspace binaries
- use a Linux-style syscall path
- use fork, execve, wait4, pipes, dup, open, read, write, mkdir, rm, touch, cd, ls, cat and more
- mount initrd
- mount AOSFS as root
- create and write files in AOSFS
- mount FAT32 and EXT4 test filesystems
- show memory usage with mem and mem -v
- show uptime
- show uname and uname -a
- show user identity with whoami and id
- use a small sudo command for now
- use shutdown, restart and reboot commands
- show PCI devices with lspci
- show drivers with driver and drivers
- detect the QEMU e1000 Ethernet card
- use DHCP
- use ARP
- use IPv4
- use DNS
- use ICMP ping
- use basic TCP
- fetch HTTP pages
- download files with wget/download
- list Wi-Fi/firmware work in progress
- load firmware blobs from initrd
- use a basic TTY instead of only raw VGA text

This means AOS already has a real booting system, real files, real commands and real networking.


3. Downloadable items

In future AOS will have a list of downloadable items.

The idea:
- packages.txt will contain package/download entries
- command line can download them
- later the MUI download app can show them nicely

We already started this.

Current download commands:

```sh
wget oppeku.org / /tmp/oppeku.txt
download oppeku.org / /tmp/oppeku.txt
```

This is HTTP only for now.
HTTPS/TLS will come later because that is a much bigger system.


4. What is MUI?

MUI stands for Mastered User Interface.

It is not different from a graphical user interface in meaning.
I just wanted a cooler AOS name for it.

The name is inspired from Dragon Ball.
Thank you Akira Toriyama.

MUI will be the AOS-native interface system.

The plan:
- desktop
- taskbar
- launcher
- settings
- files app
- terminal app
- download/package app
- themes
- widgets
- smooth graphics

Right now MUI is not complete.
We have started graphics/input foundations, but the full interface comes later.


5. Packages lists

The future package list should live in:

```txt
pakages/pakages.txt
```

Yeah the spelling is funny right now, but we can keep it AOS style or fix it later.

Package system goal:
- read package list
- show available downloads
- download selected items
- install apps only with proper permission later
- protect root/system files with sudo once full users exist


6. Efficiency

AOS should use memory in the least wasteful way possible.

The goal is:
- instant boot
- low RAM usage
- no useless background junk
- fast shell
- fast filesystem
- fast networking

Current measured boot environment:
- QEMU test RAM: 128 MiB
- memory before shell: about 63 MiB used, 66 MiB free

This can still be optimized a lot.
The target is to make AOS feel instant.


7. Storage and filesystem plan

Current working storage:
- initrd
- AOSFS root
- tmpfs
- FAT32 test mount
- EXT4 test mount

The AOS plan:

```txt
/        = AOSFS root
/boot    = boot/kernel files inside root
/kernel  = kernel files
/drivers = driver files
/commands = commands
/mui     = Mastered User Interface files
/tmp     = temporary files
/main    = user area later
```

Partition manager work has started, but full installer partitioning comes later.


8. Network and Wi-Fi plan

Networking is one of the main goals right now.

Done:
- PCI discovery
- driver listing
- e1000 Ethernet detection
- packet send/receive
- DHCP
- ARP
- IPv4
- DNS
- ping
- TCP connect/read/write
- HTTP GET
- wget/download

Next:
- better network stats
- packet counters
- better TCP handling
- more IPv6 work
- Wi-Fi scan/auth/association layers

Wi-Fi is still planned.
But real Wi-Fi is hard because it needs:
- PCI/USB device support
- firmware loading
- MAC layer
- scanning
- authentication
- association
- WPA later

So we are building it the correct way, step by step.


9. AOS philosophy

AOS should not be Windows.
AOS should not eat RAM like crazy.
AOS should not hide everything from the user.

AOS should be:
- understandable
- fast
- light
- powerful
- owned by the user

This is early.
But it is real.
My real goal is to reduce ewaste and make computers free to use like normal and no ads and will ask your permission to where to add AI,
Oppeko Signing off !. 
