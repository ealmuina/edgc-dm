#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

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
    char kernel[FIELD_SIZE], input[FIELD_SIZE], output[FIELD_SIZE], unpack[FIELD_SIZE], pack[FIELD_SIZE],
            kernel_md5[FIELD_SIZE], input_md5[FIELD_SIZE], unpack_md5[FIELD_SIZE], pack_md5[FIELD_SIZE];
    int id;
};

char big_buffer[FIELD_SIZE * BUFFER_SIZE];

void download_task(struct task *task) {
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "%s/%s", SERVER_ADDR, task->kernel);
    download(buffer, task->kernel);

    sprintf(buffer, "%s/%s", SERVER_ADDR, task->input);
    download(buffer, task->input);

    sprintf(buffer, "%s/%s", SERVER_ADDR, task->unpack);
    download(buffer, task->unpack);

    sprintf(buffer, "%s/%s", SERVER_ADDR, task->pack);
    download(buffer, task->pack);
}

struct task get_task_info(int id) {
    char url[BUFFER_SIZE];
    sprintf(url, "%s?domainId=%d", TASK_ADDR, id);

    char *text = get(url);
    struct task task_info;

    json_t *root = json_loads(text, 0, NULL);
    task_info.id = json_integer_value(json_object_get(root, "id"));
    strcpy(task_info.kernel, json_string_value(json_object_get(root, "kernel")));
    strcpy(task_info.input, json_string_value(json_object_get(root, "input")));
    strcpy(task_info.output, json_string_value(json_object_get(root, "output")));
    strcpy(task_info.unpack, json_string_value(json_object_get(root, "unpack")));
    strcpy(task_info.pack, json_string_value(json_object_get(root, "pack")));

    strcpy(task_info.kernel_md5, json_string_value(json_object_get(root, "kernel_md5")));
    strcpy(task_info.input_md5, json_string_value(json_object_get(root, "input_md5")));
    strcpy(task_info.unpack_md5, json_string_value(json_object_get(root, "unpack_md5")));
    strcpy(task_info.pack_md5, json_string_value(json_object_get(root, "pack_md5")));

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

int validate_file(char *file, char *received_hash) {
    char *hash = md5(file);
    int comp = strcmp(hash, received_hash);
    free(hash);
    return comp == 0;
}

int validate_task(struct task *task) {
    int kernel = validate_file(task->kernel, task->kernel_md5);
    int input = validate_file(task->input, task->input_md5);
    int unpack = validate_file(task->unpack, task->unpack_md5);
    int pack = validate_file(task->pack, task->pack_md5);

    return kernel && input && unpack && pack;
}

void request_execution(struct task *task) {
    int cores[NODES_MAX];
    memset(cores, 0, sizeof(int));

    for (int i = 0; i < NODES_MAX; ++i) {
        if (nodes[i].active) {
            cores[i] = (int) fmax(0, nodes[i].cores - (int) nodes[i].loadavg);
            printf("=== %d\n", cores[i]); // TODO: Keep from here
        }
    }
}

void report_task(struct task *task, int domain_id) {
    system(task->pack);
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "%s?taskId=%d&domainId=%d", REPORT_ADDR, task->id, domain_id);
    upload(buffer, task->output);
}

void print_log(char *msg) {
    time_t rawtime;
    time(&rawtime);
    char *time = strtok(ctime(&rawtime), "\n");
    printf("[%s] %s\n", time, msg);
}

int main() {
    char buffer[BUFFER_SIZE];

    // Initialize monitor and wait for nodes to report their statistics
    start_monitor();
    print_log("Program will wait 5 seconds for recovering nodes data");
    fflush(stdout);
    sleep(5); // wait to receive nodes data first

    // Register domain in repository
    int id = register_domain();
    sprintf(buffer, "Registered with id: %d", id);
    print_log(buffer);

    // Request task
    struct task task = get_task_info(id);
    sprintf(buffer, "Received task %d", task.id);
    print_log(buffer);

    // Download task files
    download_task(&task);
    sprintf(buffer, "Downloaded task %d content", task.id);
    print_log(buffer);

    // Validate files, execute and report results
    if (validate_task(&task)) {
        sprintf(buffer, "Task %d downloaded correctly", task.id);
        print_log(buffer);

        request_execution(&task);

        report_task(&task, id);
        sprintf(buffer, "Result of task %d reported", task.id);
        print_log(buffer);
    } else {
        sprintf(buffer, "Task %d corrupted", task.id);
        print_log(buffer);
    }

    return 0;
}