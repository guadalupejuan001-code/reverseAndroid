#!reverse-pc-machine-v1
# Reply fragments are interface data in Ether, selected by native XI outcome.
load8 r0 8200000000000000067
cmp r0 1
jlt done
cmp r0 6
jgt done
sub r0 1
mul r0 2
load8 r1 7400000000000000003
sub r1 1
add r0 r1
mul r0 128
set r7 8200000000000030000
add r7 r0
load8 r2 $r7
set r0 0
label character
cmp r0 r2
jge done
mov r3 r7
add r3 1
add r3 r0
load8 r14 $r3
print r14
add r0 1
jmp character
label done
halt
