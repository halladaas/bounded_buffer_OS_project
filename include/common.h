#ifndef COMMON_H
#define COMMON_H

#define POISON_PILL -1

typedef struct {
    int value;
    int priority;
    long timestamp;
} item_t;

#endif
