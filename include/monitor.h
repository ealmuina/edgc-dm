#ifndef EDGC_DM_MONITOR_H
#define EDGC_DM_MONITOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#include "utils.h"

#define TIMEOUT 180
#define NODES_MAX 1024
#define NAME_LENGTH_MAX 512
#define MONITOR_PORT 9910
#define LOAD_EPSILON 0.1
#define MAX_LOAD 0.9
#define MAX_TASKS 1024

struct node {
    char stats[BUFFER_SIZE], hostname[NAME_LENGTH_MAX];
    int active, cpus, processes[MAX_TASKS];
    float cpu_load;
    time_t last_seen;
};

struct node nodes[NODES_MAX];
pthread_mutex_t monitor_lock;

void update_processes(int node_index);

void *monitor_func(void *args);

void start_monitor();

#endif //EDGC_DM_MONITOR_H
