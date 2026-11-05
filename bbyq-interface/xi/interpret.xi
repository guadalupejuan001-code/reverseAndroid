#!reverse-pc-machine-v1
# Bounded interface grammar over the same RVPC2 as BBY Q. The host transports
# observations; only this XI program interprets words and chooses an operation.
# A=8200000000000000000. +16..47 observation, +64..68 result, +512..526 tokens.
store8 8200000000000000064 0
store8 8200000000000000065 0
store8 8200000000000000067 0
store8 8200000000000000068 0
load8 r0 8200000000000000000
cmp r0 73
jne done
load8 r0 7400000000000000040
cmp r0 0
jne done
load8 r0 7400000000000000047
cmp r0 0
jne done
load8 r12 7400000000000000032
set r0 0
label token
cmp r0 r12
jge patterns
set r2 7400000000000000512
mov r9 r0
mul r9 34
add r2 r9
load8 r3 $r2
set r1 0
label word
load8 r9 8200000000000000004
cmp r1 r9
jge unknown
set r4 8200000000000005000
mov r9 r1
mul r9 40
add r4 r9
load8 r5 $r4
mov r9 r4
add r9 1
load8 r9 $r9
cmp r9 r3
jne next_word
set r6 0
label letter
cmp r6 r3
jge known
mov r7 r2
add r7 1
add r7 r6
load8 r7 $r7
mov r8 r4
add r8 2
add r8 r6
load8 r8 $r8
cmp r7 r8
jne next_word
add r6 1
jmp letter
label next_word
add r1 1
jmp word
label unknown
set r5 0
label known
set r9 8200000000000000512
add r9 r0
store8 $r9 r5
add r0 1
jmp token
label patterns
set r0 0
label pattern
load8 r9 8200000000000000006
cmp r0 r9
jge selected
set r2 8200000000000020000
mov r9 r0
mul r9 32
add r2 r9
load8 r8 $r2
mov r9 r2
add r9 1
load8 r3 $r9
# Reject a mismatching leading literal before scanning target aliases. This
# prunes work only; variable-leading frames still use the complete matcher.
mov r9 r2
add r9 8
load8 r13 $r9
cmp r13 255
je prefix_ready
load8 r14 8200000000000000512
cmp r13 r14
jne next_pattern
label prefix_ready
set r1 0
label alias
mov r9 r2
add r9 3
load8 r9 $r9
cmp r9 0
je literal_pattern
load8 r9 8200000000000000005
cmp r1 r9
jge next_pattern
set r5 8200000000000015000
mov r9 r1
mul r9 16
add r5 r9
load8 r11 $r5
mov r9 r5
add r9 1
load8 r4 $r9
add r9 1
load8 r13 $r9
mov r9 r2
add r9 2
load8 r14 $r9
cmp r14 2
jne exact_class
cmp r13 1
je class_ready
cmp r13 4
je class_ready
cmp r13 2
jne next_alias
jmp class_ready
label exact_class
cmp r13 r14
jne next_alias
label class_ready
mov r9 r8
add r9 r11
sub r9 1
cmp r9 r12
jne next_alias
jmp compare_start
label literal_pattern
set r4 0
set r11 0
cmp r8 r12
jne next_pattern
label compare_start
set r6 0
set r10 0
label slot
cmp r10 r8
jge candidate
mov r9 r2
add r9 8
add r9 r10
load8 r7 $r9
cmp r7 255
je target
set r9 8200000000000000512
add r9 r6
load8 r9 $r9
cmp r7 r9
jne next_alias
add r6 1
add r10 1
jmp slot
label target
set r13 0
label target_word
cmp r13 r11
jge target_done
mov r9 r5
add r9 4
add r9 r13
load8 r7 $r9
set r9 8200000000000000512
add r9 r6
load8 r9 $r9
cmp r7 r9
jne next_alias
add r13 1
add r6 1
jmp target_word
label target_done
add r10 1
jmp slot
label candidate
load8 r9 8200000000000000064
cmp r9 0
je remember
cmp r9 r3
jne ambiguous
load8 r9 8200000000000000065
cmp r9 r4
je next_alias
label ambiguous
store8 8200000000000000068 1
jmp next_alias
label remember
store8 8200000000000000064 r3
store8 8200000000000000065 r4
label next_alias
mov r9 r2
add r9 3
load8 r9 $r9
cmp r9 0
je next_pattern
add r1 1
jmp alias
label next_pattern
add r0 1
jmp pattern
label selected
load8 r0 8200000000000000068
cmp r0 1
je clarify
load8 r0 8200000000000000064
cmp r0 0
je done
load8 r1 8200000000000000016
cmp r1 1
jne disconnected
cmp r0 2
jlt accepted
cmp r0 6
jgt accepted
load8 r1 8200000000000000065
cmp r1 15
je current
set r7 8200000000000000032
add r7 r1
load8 r1 $r7
cmp r1 0
jne window_exists
cmp r0 2
je accepted
cmp r0 4
je accepted
cmp r0 5
je accepted
jmp missing
label window_exists
cmp r1 1
jgt clarify
jmp accepted
label current
load8 r1 8200000000000000017
cmp r1 0
je missing
label accepted
store8 8200000000000000067 1
jmp done
label disconnected
set r0 4
jmp reject
label missing
set r0 5
jmp reject
label clarify
set r0 6
label reject
store8 8200000000000000064 0
store8 8200000000000000065 0
store8 8200000000000000067 r0
label done
set r0 512
label clean
set r7 8200000000000000000
add r7 r0
store8 $r7 0
add r0 1
cmp r0 527
jlt clean
halt
