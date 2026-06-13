// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/4/Fill.asm

// Runs an infinite loop that listens to the keyboard input. 
// When a key is pressed (any key), the program blackens the screen,
// i.e. writes "black" in every pixel. When no key is pressed, 
// the screen should be cleared.

(LOOP)
@KBD
D=M

@BLACK
D;JNE

@WHITE
0;JMP

(BLACK)
@SCREEN
D=A

@addr
M=D

(LOOP1)
@addr
A=M
M=-1

@addr
M=M+1

@addr
D=M
@24576
D=D-A

@LOOP
D;JEQ

@LOOP1
0;JMP


(WHITE)
@SCREEN
D=A

@addr
M=D

(LOOP2)
@addr
A=M
M=0
@addr
M=M+1

@addr
D=M
@24576
D=D-A

@LOOP
D;JEQ

@LOOP2
0;JMP
