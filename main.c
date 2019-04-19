#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>
#include <unistd.h>

#include <jansson.h>

#include "http.c"
#include "monitor.h"
#include "utils.c"

#define FIELD_SIZE 1024
#define REGISTER_ADDR "http://localhost:5000/api/register"
#define SERVER_ADDR "http://localhost:5000"
#define TASK_ADDR "http://localhost:5000/api/task"
#define REPORT_ADDR "http://localhost:5000/api/report"

struct task {
    char kernel[FIELD_SIZE], input[FIELD_SIZE], kernel_md5[FIELD_SIZE], input_md5[FIELD_SIZE];
    int id;
};

char big_buffer[FIELD_SIZE * BUFFER_SIZE];

void download_task(struct task *task) {
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "%s/%s", SERVER_ADDR, task->kernel);
    download(buffer, task->kernel);

    sprintf(buffer, "%s/%s", SERVER_ADDR, task->input);
    download(buffer, task->input);
}

struct task get_task_info(int id) {
    char url[BUFFER_SIZE];
    sprintf(url, "%s?domainId=%d", TASK_ADDR, id);

    char *text = get(url);
    struct task task_info;

    json_t *root = json_loads(text, 0, NULL);
    task_info.id = json_integer_value(json_object_get(root, "id"));
    strcpy(task_info.kernel, json_string_value(json_object_get(root, "kernel")));
    strcpy(task_info.kernel_md5, json_string_value(json_object_get(root, "kernel_md5")));
    strcpy(task_info.input, json_string_value(json_object_get(root, "input")));
    strcpy(task_info.input_md5, json_string_value(json_object_get(root, "input_md5")));

    json_decref(root);
    free(text);
    return task_info;
}

int register_domain() {
    big_buffer[0] = '[';

    pthread_mutex_lock(&monitor_lock);
    for (int i = 0; i < NODES_MAX; ++i) {
        if (nodes[i].active) {
            strcat(big_buffer, nodes[i].stats);
            strcat(big_buffer, ",");
        }
    }
    pthread_mutex_unlock(&monitor_lock);
    int len = strlen(big_buffer);
    if (big_buffer[len - 1] == ',')
        big_buffer[len - 1] = ']';
    else
        strcat(big_buffer, "]");

    char *text = post(REGISTER_ADDR, big_buffer);
    json_t *root = json_loads(text, 0, NULL);
    int id = json_integer_value(json_object_get(root, "id"));

    free(text);
    json_decref(root);
    return id;
}

int validate_task(struct task *task) {
    char *kernel_hash = md5(task->kernel);
    int kernel_comp = strcmp(kernel_hash, task->kernel_md5);
    free(kernel_hash);

    char *input_hash = md5(task->input);
    int input_comp = strcmp(input_hash, task->input_md5);
    free(input_hash);

    if (kernel_comp != 0) return 0;
    if (input_comp != 0) return 0;
    return 1;
}

void report_task(struct task *task, int domain_id) {
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "%s?taskId=%d&domainId=%d", REPORT_ADDR, task->id, domain_id);
    upload(buffer, "output.txt");
}

void print_log(char *msg) {
    time_t rawtime;
    time(&rawtime);
    char *time = strtok(ctime(&rawtime), "\n");
    printf("[%s] %s\n", time, msg);
}

int main() {
    char buffer[BUFFER_SIZE];

    start_monitor();
    print_log("Program will wait 5 seconds for recovering nodes data");
    fflush(stdout);
    sleep(5); // wait to receive nodes data first

    int id = register_domain();
    sprintf(buffer, "Registered with id: %d", id);
    print_log(buffer);

    struct task task = get_task_info(id);
    sprintf(buffer, "Received task %d", task.id);
    print_log(buffer);

    download_task(&task);
    sprintf(buffer, "Downloaded task %d content", task.id);
    print_log(buffer);

    if (validate_task(&task)) {
        sprintf(buffer, "Task %d downloaded correctly", task.id);
        print_log(buffer);

        report_task(&task, id);
        sprintf(buffer, "Result of task %d reported", task.id);
        print_log(buffer);
    } else {
        sprintf(buffer, "Task %d corrupted", task.id);
        print_log(buffer);
    }

    return 0;
}