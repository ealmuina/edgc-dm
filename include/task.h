#ifndef EDGC_DM_TASK_H
#define EDGC_DM_TASK_H

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include <jansson.h>

#include "http.h"
#include "monitor.h"
#include "utils.h"
#include "report.h"

#define TASKS_MAX 1024
#define FIELD_SIZE 1024
#define SERVER_ADDR "http://localhost:5000"
#define TASK_ADDR "http://localhost:5000/api/task"

struct task {
    char kernel[FIELD_SIZE], input[FIELD_SIZE], output[FIELD_SIZE], unpack[FIELD_SIZE], pack[FIELD_SIZE],
            kernel_md5[FIELD_SIZE], input_md5[FIELD_SIZE], unpack_md5[FIELD_SIZE], pack_md5[FIELD_SIZE];
    int id, active, flexmpi_id;
};

struct task tasks[TASKS_MAX];

void download_task(struct task *task);

struct task get_task_info(int id);

int validate_file(char *file, char *received_hash);

int validate_task(struct task *task);

void request_execution(struct task *task, int task_index);

int process_task(int id);

#endif //EDGC_DM_TASK_H
