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
        if (tasks[i].active) {
            total_processes += node->processes[i];
            if (node->processes[i] > max_processes) {
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
        // Kill a process from the task with the most
        node->processes[max_task]--;
        sprintf(buffer, "Reduced load of task %d in node %d.", tasks[max_task].id, node_index);
        print_log(buffer);

        // Set delta of processes to -1
        delta = -1;
        task = max_task;
    }
        // Check if load could be increased
    else if (total_processes < node->cpus) {
        float process_load;
        if (total_processes)
            process_load = node->cpu_load / total_processes;
        else
            process_load = 0.5;
        float available_load = MAX_LOAD - node->cpu_load;
        int new_processes = (int) (available_load / process_load); // Estimation of new processes

        // Increase if new_processes > 0
        if (new_processes > 0) {
            node->processes[min_task] += new_processes;

            // Set delta of processes to new_processes
            delta = (int) fmin(new_processes, node->cpus - total_processes);
            task = min_task;

            sprintf(buffer, "Increased load in node %d for task %d by %d processes.", node_index, delta,
                    tasks[task].id);
            print_log(buffer);
        }
    }

    // Send signal to modify the number of processes
    if (delta) {
        char command[1024];
        sprintf(command,
                "nping --udp -p 8900 -c 1 localhost --data-string \"%d 0 6:%s:%d\" %s",
                tasks[task].flexmpi_id,
                node->hostname,
                delta,
                "> /dev/null 2> /dev/null"
        );
        printf("\t-> %s\n", command);
        node->processes[task] += delta;
        system(command);
    }
    pthread_mutex_lock(&tasks_lock);
}

void *monitor_func(void *args) {
    int sockfd, len;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int broadcastEnable = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(MONITOR_PORT);

    bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        // Receive statistics from a node
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cli_addr, &len);
        buffer[n] = '\0';

        if (!cli_addr.sin_addr.s_addr) continue; //skip if node address is 0

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
        pthread_mutex_lock(&monitor_lock);
        for (int i = 0; i < NODES_MAX; ++i) {
            if (nodes[index].active) index = i; // index will be the first empty position
            if (nodes[i].active && strcmp(nodes[i].hostname, hostname) == 0) {
                index = i; // found!
                break;
            }
        }
        // Update or create entry
        if (index != NODES_MAX) { // There is still space for this node
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
                if (difftime(now, nodes[i].last_seen) > TIMEOUT)
                    nodes[i].active = 0;
            }
            pthread_mutex_unlock(&monitor_lock);
        }
#pragma clang diagnostic pop
    }
}

void start_monitor() {
    pthread_mutex_init(&monitor_lock, NULL);
    pthread_t m;
    pthread_create(&m, NULL, monitor_func, NULL);
}
