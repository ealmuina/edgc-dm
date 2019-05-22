#include "include/monitor.h"
#include "include/report.h"

void update_processes(int node_index) {
    char buffer[BUFFER_SIZE];
    struct node *node = &nodes[node_index];

    pthread_mutex_lock(&tasks_lock);

    // Get the total, max and min of processes
    int total_processes = 0, min_processes = INT32_MAX, max_processes = INT32_MIN;
    int min_task = -1, max_task = -1;
    for (int i = 0; i < MAX_TASKS; ++i) {
        // Consider only active tasks
        if (tasks[i].active) {
            total_processes += node->processes[i];
            /* Task with the most processes will only be used to decrease them if necessary
             * The number of processes on root hosts cannot be below its initial value
             * -> So tasks will only be taken into into account for this variable only if the node is non-root for them
             *    or they have more processes than when beginning */
            if (node->processes[i] > max_processes && (!node->root_task[i] || node->processes[i] > ROOT_PROCESSES)) {
                max_processes = node->processes[i];
                max_task = i;
            }
            if (node->processes[i] < min_processes) {
                min_task = i;
                min_processes = node->processes[i];
            }
        }
    }

    // Return if the min_task = -1 --> No active tasks in the system
    if (min_task == -1) {
        pthread_mutex_unlock(&tasks_lock);
        return;
    }

    int delta = 0, task = -1;
    // Check if load needs to be reduced
    if (node->cpu_load > MAX_LOAD && total_processes) {
        float process_load = node->cpu_load / total_processes;

        // Set delta to the number of processes above the maximum allowed load
        delta = -(int) (node->processes[max_task] - (MAX_LOAD - LOAD_EPSILON) / process_load);
        task = max_task;
    }
        // Check if load could be increased
    else if (total_processes < node->cpus) {
        float process_load;
        if (total_processes)
            process_load = node->cpu_load / total_processes;
        else
            process_load = 0.25;
        float available_load = MAX_LOAD - node->cpu_load;
        int new_processes = (int) (available_load / process_load); // Estimation of new processes

        // Increase if new_processes > 0
        if (new_processes > 0) {
            // Set delta of processes to new_processes
            delta = (int) fmin(new_processes, node->cpus - total_processes);
            task = min_task;
        }
    }

    // Send signal to modify the number of processes
    if (delta) {
        sleep(10); // wait 10 seconds to let FlexMPI controller process previous commands
        if (delta < 0)
            sprintf(buffer, "Reduced load of task %d in node '%s'.", tasks[max_task].id, node->hostname);
        else
            sprintf(buffer, "Increased load in node '%s' for task %d by %d processes.", node->hostname, tasks[task].id,
                    delta);
        print_log(buffer);

        sprintf(buffer,
                "nping --udp -p 8900 -c 1 localhost --data-string \"%d 0 6:%s:%d\" %s",
                tasks[task].flexmpi_id,
                node->hostname,
                delta,
                "> /dev/null 2> /dev/null"
        );
        printf("\t-> %s\n", buffer);
        node->processes[task] += delta;
        system(buffer);
    }
    pthread_mutex_unlock(&tasks_lock);
}

void *monitor_func(void *args) {
    int sockfd, len;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(MONITOR_PORT);

    bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    print_log("Monitor thread initialized.");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        // Receive statistics from a node
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cli_addr, &len);
        buffer[n] = '\0';

        // Get time for knowing last time the node was seen
        time_t now;
        time(&now);

        // Extract hostname from received data
        char hostname[NAME_LENGTH_MAX];
        strcpy(hostname, buffer);
        len = strlen(hostname);
        hostname[len] = 0;
        char *stats = buffer + len + 1;

        // Extract loadavg from received data
        float cpu_load = *(float *) stats;
        stats += sizeof(float);

        // Extract number of processors from received data
        int cpus = *(int *) stats;
        stats += sizeof(int);

        // Search node in the list
        int index = 0;
        pthread_mutex_lock(&nodes_lock);
        for (int i = 0; i < NODES_MAX; ++i) {
            if (nodes[index].active) index = i; // index will be the first empty position
            if (nodes[i].active && strcmp(nodes[i].hostname, hostname) == 0) {
                index = i; // found!
                break;
            }
        }
        // Update or create entry
        if (index != NODES_MAX) { // There is still space for this node
            if (nodes[index].active == 0) {
                sprintf(buffer, "Detected node '%s'.", hostname);
                print_log(buffer);
                // Reset information regarding tasks
                memset(nodes[index].processes, 0, sizeof(nodes[index].processes));
                memset(nodes[index].root_task, 0, sizeof(nodes[index].root_task));
            }

            nodes[index].active = 1;
            strcpy(nodes[index].stats, stats);
            strcpy(nodes[index].hostname, hostname);
            nodes[index].last_seen = now;
            nodes[index].cpu_load = cpu_load;
            nodes[index].cpus = cpus;

            // Update processes
            update_processes(index);
            // Remove nodes that have not been seen in a while
            for (int i = 0; i < NODES_MAX; ++i) {
                if (nodes[i].active && difftime(now, nodes[i].last_seen) > TIMEOUT) {
                    nodes[i].active = 0;
                    sprintf(buffer, "Removed node '%s' after being inactive for %d seconds.", nodes[i].hostname,
                            TIMEOUT);
                    print_log(buffer);
                }
            }
        }
        pthread_mutex_unlock(&nodes_lock);
#pragma clang diagnostic pop
    }
}

void start_monitor() {
    pthread_mutex_init(&nodes_lock, NULL);
    pthread_t m;
    pthread_create(&m, NULL, monitor_func, NULL);
}
