/**
 * ISA - projekt
 * 
 * Dominik Nejedly (xnejed09), 2020
 * 
 * Jednosmerne vazany seznam
 */

//navratove hodnoty
#define SUCCESS 0
#define FAIL -1


//struktura prvku seznamu
typedef struct charPNode {
    char *str;
    struct charPNode *next;
} charPNodeT;

//struktura seznamu
typedef struct charPList {
    charPNodeT *head;
} charPListT;

//deklarace obsluznych funkci
void init(charPListT *list);
bool isEmpty(charPListT *list);
int push(charPListT *list, char *str);
void clear(charPListT *list);