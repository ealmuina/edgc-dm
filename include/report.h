#ifndef EDGC_DM_REPORT_H
#define EDGC_DM_REPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include "utils.h"
#include "task.h"

#define REPORT_PORT 9911
#define REPORT_URL "api/report"

pthread_mutex_t tasks_lock;

void *report_func(void *args);

void start_reporter(int id);

#endif //EDGC_DM_REPORT_H