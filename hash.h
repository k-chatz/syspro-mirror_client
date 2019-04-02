#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>
#include <stdio.h>

typedef struct Hashtable *Hashtable;

bool HT_Init(
        Hashtable *ht,
        unsigned long capacity,
        unsigned long int bucketSize,
        void *(*createValue)(void *),
        int (*cmp)(void *, void *),
        unsigned long (*hash)(void *, unsigned long int),
        unsigned long (*destroy)(void *)
);

int HT_Insert(Hashtable ht, void *key, void *valueParams, void **value);

void *HT_Get(Hashtable ht, void *key);

int HT_Remove(Hashtable ht, void *key, void* valueParams, bool forceDestroyItem);

void HT_Destroy(Hashtable *ht, bool forceDestroyItems);

#endif
