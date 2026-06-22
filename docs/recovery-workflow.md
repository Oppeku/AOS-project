<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2026 Oppeko -->

# AOS Recovery Workflow

AOS recovery should be built around separate partitions, not one giant system
partition.

## Partition Roles

Root:
- filesystem: AOSFS
- mount: `/`
- contains commands, vash, bootloader files, kernel, drivers, and the network stack
- can be restored if broken

Main:
- filesystem: ext4
- mount: `/main`
- contains user files, apps, and MUI/user desktop data
- should not be touched by root recovery unless the user explicitly asks

Swap:
- filesystem: swap
- used for virtual memory later

Emergency Repair:
- filesystem: AOSFS
- separate minimal repair environment
- contains emergency repair shell, e1000 networking support, HTTP restore support,
  and local USB restore support

## Boot Decision

At boot, AOS should eventually check whether normal root is available.

If root is healthy:
- boot normal AOS root

If root is missing or corrupted:
- boot Emergency Repair
- show repair options
- restore root from network or recovery USB

## Online Restore

Online restore comes first through Ethernet because AOS already has e1000 work.

Flow:
- bring up e1000
- use DHCP
- use DNS if needed
- download a signed AOSFS root image over HTTP first
- verify the image before writing it
- write only the root AOSFS partition
- reboot into restored root

HTTPS and Wi-Fi can come later.

## Offline Restore

Offline restore should work from an AOS Recovery USB.

Flow:
- detect recovery USB later
- find local AOSFS root image
- verify it
- write only the root AOSFS partition
- reboot into restored root

## Safety Rules

Recovery must be boring and strict:
- do not wipe `/main` by default
- do not touch swap unless the user chooses it
- do not trust downloaded images without checksums/signatures
- keep Emergency Repair small
- keep Emergency Repair mostly read-only
- show clearly what partition will be written before writing

This makes AOS recoverable even if normal root gets destroyed, while still
protecting user files.
