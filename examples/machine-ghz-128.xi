#!reverse-pc-machine-v1
# Real classic control + 128-qubit integrated Ether in one XI transaction.
# The program is short: the loop builds the actual GHZ register dynamically.
seed 917
set r0 0
set r1 1
set r2 128
h $r0
label entangle
cx $r0 $r1
add r0 1
add r1 1
cmp r1 r2
jlt entangle
t 0
tdg 127
expect Z:0 Z:$r0
qstats
# Euclid's algorithm, using signed 64-bit CPU registers.
set r3 1071
set r4 462
label gcd
cmp r4 0
je found
mov r5 r3
mod r5 r4
mov r3 r4
mov r4 r5
jmp gcd
label found
print r3
store8 500 r3
mem_read 500
stats
