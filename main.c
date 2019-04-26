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

#include "http.h"
#include "monitor.h"
#include "utils.h"
#include "report.h"
#include "task.h"

#define FIELD_SIZE 1024
#define REGISTER_ADDR "http://localhost:5000/api/register"

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

    char *text = post(REGISTER_ADDR, big_buffer);
    json_t *root = json_loads(text, 0, NULL);
    int id = json_integer_value(json_object_get(root, "id"));

    free(text);
    json_decref(root);
    return id;
}

int main() {
    char buffer[BUFFER_SIZE];

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

    // Process a task
    process_task(id);

    sleep(100);

    return 0;
}