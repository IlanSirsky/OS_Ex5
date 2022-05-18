#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>





// Push() operation on a  stack
void push(Stack * stack, char *value)
{
    strcpy(stack[stack->index].data, value);
    stack->index++;
}

void *pop(Stack * stack)
{
    char * pop = stack[stack->index - 1].data;
    strcpy(stack[stack->index].data, "");
    stack->index--;
    return pop;
}

char *top(Stack * stack)
{
    char * top = stack[stack->index - 1].data;
    return top;
}