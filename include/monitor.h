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
#include <netdb.h>

#include "utils.h"
#include "task.h"

#define TIMEOUT 600
#define NODES_MAX 1024
#define NAME_LENGTH_MAX 512
#define MONITOR_LITE_PORT 9910
#define MONITOR_FULL_PORT 9913
#define CONTROLLER_PORT 9912
#define MAX_DELTA_PROCESSES 100
#define UPDATER_INTERVAL 5

double MAX_LOAD, LOAD_EPSILON;

struct node {
    char stats[BUFFER_SIZE], hostname[NAME_LENGTH_MAX];
    int active, cpus;
    int processes[TASKS_MAX]; // stores the number of processes each task has currently running in the node
    int root_task[TASKS_MAX]; // indicates for each task whether the node is its root or not
    float cpu_load;
    time_t last_seen;
};

struct adjustment {
    int task_index, delta, active;
};

int total_cpus;

struct node nodes[NODES_MAX];

pthread_mutex_t nodes_lock, controller_lock;

void initialize_socket(int *sockfd, int port);

int calculate_adjustment(struct node *node, int *task);

void build_adjustments(struct adjustment *adjustments);

void request_full_info(int node_index);

void *monitor_func(void *args);

void *updater_func(void *args);

void start_monitor(double max_load, double load_epsilon);

#endif //EDGC_DM_MONITOR_H
