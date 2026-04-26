/* utils.h — drop this into include/ */
#ifndef UTILS_H
#define UTILS_H

#include <time.h>

/* Returns current wall-clock time in milliseconds */
long current_time_ms(void);

/* Write metrics to results/metrics.txt */
void save_metrics(const char *path, long produced, long consumed, double avg_wait_ms);

#endif /* UTILS_H */
