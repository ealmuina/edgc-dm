#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <math.h>

#include <jansson.h>

#include "include/http.h"
#include "include/monitor.h"
#include "include/utils.h"
#include "include/report.h"
#include "include/task.h"

#define REGISTER_URL "api/register"

char big_buffer[FIELD_SIZE * BUFFER_SIZE];

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

    char register_addr[FIELD_SIZE];
    sprintf(register_addr, "%s/%s", repository_url, REGISTER_URL);

    char *text = post(register_addr, big_buffer);
    json_t *root = json_loads(text, 0, NULL);
    int id = json_integer_value(json_object_get(root, "id"));

    free(text);
    json_decref(root);
    return id;
}

int main(int argc, char *argv[]) {
    int max_tasks = 1;
    char buffer[BUFFER_SIZE];

    if (argc != 2) {
        printf("Usage edgc-dm <repository_url>");
        return -1;
    }
    if (strncmp("http://", argv[1], 7) == 0)
        sprintf(repository_url, "%s", argv[1]);
    else
        sprintf(repository_url, "http://%s", argv[1]);

    // Initialize monitor and wait for nodes to report their statistics
    start_monitor();
    print_log("Program will wait 10 seconds for recovering nodes data");
    fflush(stdout);
    sleep(10); // wait to receive nodes data first

    // Register domain in repository
    int id = register_domain();
    sprintf(buffer, "Registered with id: %d", id);
    print_log(buffer);

    // Initialize reporter
    start_reporter(id);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        int current_tasks = 0;
        pthread_mutex_lock(&tasks_lock);
        for (int i = 0; i < MAX_TASKS; ++i) {
            if (tasks[i].active) current_tasks++;
        }
        pthread_mutex_unlock(&tasks_lock);

        // Process a task
        if (current_tasks < max_tasks)
            process_task(id);

        sleep(60);
    }
#pragma clang diagnostic pop

    return 0;
}