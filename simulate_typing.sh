#!/bin/bash
FIFO=/tmp/aos_input
rm -f $FIFO
mkfifo $FIFO
# Start QEMU with serial input from the FIFO
qemu-system-x86_64 -cdrom build/aos.iso -display none -serial stdio < $FIFO &
QEMU_PID=$!
# Wait for boot
sleep 5
# Type "HELLO"
echo -n "H" > $FIFO
sleep 0.5
echo -n "E" > $FIFO
sleep 0.5
echo -n "L" > $FIFO
sleep 0.5
echo -n "L" > $FIFO
sleep 0.5
echo -n "O" > $FIFO
sleep 0.5
echo "" > $FIFO # Newline
sleep 1
# Cleanup
kill $QEMU_PID
rm $FIFO
