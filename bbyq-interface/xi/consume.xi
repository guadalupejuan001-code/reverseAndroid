#!reverse-pc-machine-v1
# The host takes a typed action once, then clears all observation/result bytes.
set r0 16
label clear
set r7 8200000000000000000
add r7 r0
store8 $r7 0
add r0 1
cmp r0 80
jlt clear
halt
