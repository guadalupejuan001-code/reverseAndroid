#!reverse-pc-machine-v1
# Registers are fresh on every run; durable state resides in InverseMemory/RVPC2.
load8 r0 500
add r0 1
store8 501 r0
print r0
mem_read 501
expect Z:0 Z:127
qstats
