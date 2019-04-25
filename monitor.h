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

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */
#define TIMEOUT 180
#define NODES_MAX 1024
#define MONITOR_PORT 9910

struct node {
    char stats[BUFFER_SIZE];
    int active, cores;
    double loadavg;
    time_t last_seen;
    in_addr_t addr;
};

struct node nodes[NODES_MAX];

pthread_mutex_t monitor_lock;

void *monitor_func(void *args);

void start_monitor();

#endif //EDGC_DM_MONITOR_H
