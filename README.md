c4 - C in four functions
========================

An exercise in minimalism.

Try the following:

    gcc -o c4 c4.c
    ./c4 hello.c
    ./c4 -s hello.c
    
    ./c4 c4.c hello.c
    ./c4 c4.c c4.c hello.c


## 汇编指令

IMM <num>:  将 <num> 放入寄存器 ax 中。
LC <addr>:  将对应地址中的字符载入 ax 中，要求 ax 中存放地址。
LI <addr>:  将对应地址中的整数载入 ax 中，要求 ax 中存放地址。
SC <addr>:  将 ax 中的数据作为字符存放入地址中，要求栈顶存放地址。
SI <addr>:  将 ax 中的数据作为整数存放入地址中，要求栈顶存放地址。
PUSH :      将 ax 的值放入栈中;
JMP <addr>: 将当前的 PC 寄存器设置为指定的 <addr>
JZ <addr>:  jump if ax == 0;
JNZ <addr>: jump if ax != 0;

CALL <addr>: 跳转到地址为 <addr> 的子函数
RET : 从子函数中返回

ENT <size> : 保存当前的栈指针，同时在栈上保留一定的空间，用以存放局部变量;
ADJ <size>: 在将调用子函数时压入栈中的数据清除  
LEV: 离开函数调用, 弹出栈, 并返回调用者
LEA <offset>: bp偏移offset后的内容赋值给ax
