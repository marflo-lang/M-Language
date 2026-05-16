#pragma once

#include <stdint.h>

/* Limits the number of nested calls */
#define M_MAXCALLS  20000

/* Limits the number of M stack slots */
#define M_MAXSTACK  8000

/* Limits the nested C calls */
#define M_MAXCCALLS 200

/* Limits the number of variables per function */
#define M_MAXVARS   200

#define MAX_FRAMES  64

//typedef struct
//{
//    int line;
//    int column;
//}Position;
//
//typedef struct
//{
//    Position* begin;
//    Position* end;
//}Location;

typedef uint32_t Instruction;

#define SIZE_OP 6
#define SIZE_A  8
#define SIZE_B  9
#define SIZE_C  9

#define POS_OP  0
#define POS_A   (POS_OP + SIZE_OP)
#define POS_C   (POS_A + SIZE_A)
#define POS_B   (POS_C + SIZE_C)

#define MASK1(n)    ((1u << (n)) - 1)

#define MASK_OP MASK1(SIZE_OP)
#define MASK_A  MASK1(SIZE_A)
#define MASK_B  MASK1(SIZE_B)
#define MASK_C  MASK1(SIZE_C)

#define GET_OPCODE(i) ((i) & MASK_OP)
#define GET_A(i)    (((i) >> POS_A) & MASK_A)
#define GET_C(i)    (((i) >> POS_C) & MASK_C)
#define GET_B(i)    (((i) >> POS_B) & MASK_B)
#define GET_Bx(i)   ((i) >> POS_C)

#define MAX_Bx  ((1 << (SIZE_B + SIZE_C)) -1)
#define MAX_sBx (MAX_Bx >> 1)
#define GET_sBx(i)  (GET_Bx(i) - MAX_sBx)

#define CREATE_ABC(op, a, b, c) \
    ((Instruction) (\
        ((op) & MASK_OP) << POS_OP |\
        ((a) & MASK_A) << POS_A |\
        ((c) & MASK_C) << POS_C |\
        ((b) & MASK_B) << POS_B ))

#define CREATE_ABx(op, a, bx) \
    ((Instruction) (\
        ((op) & MASK_OP) << POS_OP |\
        ((a) & MASK_A) << POS_A |\
        ((bx) & MASK1(SIZE_B + SIZE_C)) <<POS_C ))

#define CREATE_AsBx(op, a, sbx) \
    CREATE_ABx(op, a, (sbx) + MAX_sBx)



