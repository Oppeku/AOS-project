<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2026 Oppeko -->

# Hardware, Network, and Bluetooth Roadmap

AOS should not jump straight into network or Bluetooth drivers yet. Those
drivers need a hardware discovery layer and a clean driver model first.

## Goal

Bring AOS from basic local storage/userspace into real hardware support:

1. Discover hardware.
2. Register drivers cleanly.
3. Handle interrupts and DMA safely.
4. Bring up one simple network card.
5. Build a small network stack.
6. Add USB.
7. Add Bluetooth after USB exists.

## Phase 1: PCI Discovery

Purpose:
- Find real hardware devices.
- Detect vendor ID, device ID, class, subclass, BARs, and IRQ lines.
- Make AOS able to print hardware like Linux `lspci`.

Build:
- PCI config-space read/write helpers.
- PCI bus/device/function scanner.
- PCI device table in kernel memory.
- `lspci` userspace command.

Test target:
- QEMU PCI devices should show up.
- `lspci` should list at least VGA, IDE/storage, and an emulated NIC when enabled.

## Phase 2: Driver Model

Purpose:
- Stop drivers from being one-off code paths.
- Let devices and drivers register with the kernel.

Build:
- `struct device`
- `struct driver`
- driver probe/remove hooks
- driver name/status
- IRQ registration per device
- device nodes later, such as `/dev/net0`

Test target:
- PCI scanner finds a device.
- Matching driver claims it.
- A command can show claimed/unclaimed devices.

## Phase 3: Interrupt and DMA Readiness

Purpose:
- Real NICs and USB controllers need interrupts and DMA buffers.

Build:
- IRQ handler registration API.
- Basic PCI IRQ routing support.
- DMA-safe page allocation.
- Physical address helpers for DMA descriptors.
- Cache/ownership rules for buffers.

Test target:
- A test driver can receive an IRQ.
- A test DMA buffer can be allocated and its physical address printed.

## Phase 4: First Network Driver

Start with Intel e1000/e1000e because QEMU can emulate it.

QEMU target later:

```sh
qemu-system-x86_64 -netdev user,id=n0 -device e1000,netdev=n0
```

Build:
- PCI match for e1000.
- MMIO BAR mapping.
- TX descriptor ring.
- RX descriptor ring.
- MAC address read.
- Packet send/receive.

Test target:
- `netdev` command shows `net0`.
- AOS can receive Ethernet frames.
- AOS can transmit a raw Ethernet frame.

## Phase 5: Minimal Network Stack

Build in this order:

1. Ethernet frame parser.
2. ARP.
3. IPv4.
4. ICMP echo.
5. UDP.
6. TCP later.

First useful user command:

```txt
ping 10.0.2.2
```

Test target:
- `ping` works in QEMU user networking.
- Packet counters increase in `netstat` or `netdev`.

## Phase 6: USB Foundation

Bluetooth usually comes through USB, so USB comes before Bluetooth.

Build:
- PCI discovery for USB host controllers.
- Start with one controller family first.
- USB device enumeration.
- Control transfers.
- Basic HID keyboard/mouse later.
- USB storage later.

Preferred order:

1. UHCI/OHCI only if keeping it old/simple.
2. EHCI for USB 2.0.
3. xHCI later for modern hardware.

## Phase 7: Bluetooth

Bluetooth should wait until USB enumeration works.

Build:
- USB Bluetooth adapter detection.
- Bluetooth HCI command/event layer.
- Device inquiry.
- Pairing later.
- HID/audio/network profiles much later.

Test target:
- Detect a USB Bluetooth adapter.
- Print controller address.
- List nearby devices.

## Current Recommendation

Do this next:

1. Add PCI config-space access.
2. Add PCI scan table.
3. Add `lspci`.
4. Add e1000 only after PCI is solid.

Bluetooth is intentionally later because it depends on USB and has much more
protocol complexity than basic Ethernet.
