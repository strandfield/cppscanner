
#ifndef MY_GUARD
#define MY_GUARD

#define MY_CONSTANT 3

#define MY_MIN(a,b) ( (a) < (b) ? (a) : (b) )

#define GREATER_THAN_MY_CONSTANT(a) (MY_MIN(MY_CONSTANT, (a)) == MY_CONSTANT)

#endif MY_GUARD