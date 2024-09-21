#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "arraylist.h"
#include <ctype.h>
#ifndef DEBUG
#define DEBUG 0
#endif

void al_init(arraylist_t *L, unsigned size)
{
    //arraylist_t * dict = (arraylist_t*)malloc(sizeof(arraylist_t))
    L->data = (char**)malloc(size*sizeof(char*));
    L->length = 0;
    L->capacity = size;
}

void al_destroy(arraylist_t *L)
{
    for (int k = 0; k <L->length; k++)
    {
        free(L->data[k]); //free each character
    }
    free(L->data);
}

unsigned al_length(arraylist_t *L)
{
    return L->length;
}

void al_push(arraylist_t *L, char *word)
{
    if (L->length == L->capacity) 
    {
        L->capacity *= 2;
        char** temp = (char **)realloc(L->data, sizeof(char*)*L->capacity);
        if (!L->data) 
        {
            // for our own purposes, we can decide how to handle this error
            // for more general code, it would be better to indicate failure to our caller
            fprintf(stderr, "Out of memory!\n");
            exit(EXIT_FAILURE);
        }
        L->data = temp;
        if (DEBUG) printf("Resized array to %u\n", L->capacity);
    }
   
    L->data[L->length] = (char*)malloc(strlen(word) + 1);
    if (!L->data[L->length])
    {
        fprintf(stderr, "Out of memory!\n");
        exit(EXIT_FAILURE);

    }

    strcpy(L->data[L->length], word); //copy word into dictionary
    L->length++;
}

// returns 1 on success and writes popped item to dest
// returns 0 on failure (list empty)

int al_pop(arraylist_t *L, char *dest)
{
    if (L->length == 0) return 0;
    L->length--;
    dest = L->data[L->length];
    return 1;
}

void al_print(arraylist_t *L)
{
    for (int j = 0; j < L->length; j++)
    {
        printf("%s\n", L->data[j]);
    }
}

char* al_get(arraylist_t *L, int index)
{
    if ((index > L->length) || (index < 0))
    {
        return NULL;
    }
    return L->data[index];
}