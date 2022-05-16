#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>


// Structure to create a node with data and next pointer
struct Node
{
    char *data;
    struct Node *next;
};
Node *head = NULL;


// Push() operation on a  stack
void push(char *value)
{
    struct Node *newNode;
    newNode = (struct Node *)malloc(sizeof(struct Node));
    newNode->data = value; // assign value to the node
    if (head == NULL)
    {
        newNode->next = NULL;
    }
    else
    {
        newNode->next = head; // Make the node as head
    }
    head = newNode; // head always points to the newly created node
}

char *pop()
{
    if (head == NULL)
    {
        return NULL;
    }

    struct Node *temp = head;
    char *temp_data = head->data;
    head = head->next;
    free(temp);
    return temp_data;
}

char *top()
{
    // Display the elements of the stack
    if (head == NULL)
    {
        return NULL;
    }
    return head->data;
}