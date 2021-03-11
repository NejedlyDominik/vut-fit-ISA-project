/**
 * ISA - projekt
 * 
 * Dominik Nejedly (xnejed09), 2020
 * 
 * Jednosmerne vazany seznam
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "charPList.h"


/**
 * inicializace seznamu
 */
void init(charPListT *list)
{
    list->head = NULL;
}


/**
 * testovani prazdnosti seznamu
 */
bool isEmpty(charPListT *list)
{
    if(list->head == NULL)
    {
        return true;
    }

    return false;
}


/**
 * pridani noveho prvku na zacatek seznamu
 */
int push(charPListT *list, char *str)
{
    charPNodeT *node = malloc(sizeof(charPNodeT));

    if(node == NULL)
    {
        return FAIL;
    }

    node->str = str;
    node->next = list->head;
    list->head = node;

    return SUCCESS;
}


/**
 * uvolneni alokovane pameti celeho seznamu
 */
void clear(charPListT *list)
{
    charPNodeT *node;

    while(list->head != NULL)
    {
        node = list->head;
        list->head = node->next;
        free(node);
    }
}