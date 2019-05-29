#include "include/monitor.h"
#include "include/report.h"

int controller_sockfd;

void initialize_socket(int *sockfd, int port) {
    struct sockaddr_in serv_addr;

    *sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    bind(*sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
}

int calculate_adjustment(struct node *node, int *task) {
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
        return 0;
    }

    int delta = 0;

    // Check if load needs to be reduced
    if (node->cpu_load > MAX_LOAD && total_processes) {
        float process_load = node->cpu_load / total_processes;

        // Set delta to the number of processes above the maximum allowed load
        delta = -(int) (node->processes[max_task] - MAX_LOAD / process_load);
        *task = max_task;
    }
        // Check if load could be increased
    else if (total_processes < node->cpus) {
        float process_load;
        if (total_processes)
            process_load = node->cpu_load / total_processes;
        else
            process_load = 0.25;
        float available_load = MAX_LOAD - LOAD_EPSILON - node->cpu_load;
        int new_processes = (int) (available_load / process_load); // Estimation of new processes

        // Increase if new_processes > 0
        if (new_processes > 0) {
            // Set delta of processes to new_processes
            delta = (int) fmin(new_processes, node->cpus - total_processes);
            delta = (int) fmin(delta, node->cpus / 2.0); // only half of total CPUs at once
            *task = min_task;
        }
    }

    return delta;
}

void request_full_info(int node_index) {
    int sockfd;
    struct sockaddr_in servaddr;
    struct node *node = &nodes[node_index];
    struct hostent *he;

    // socket create
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));

    // Get node ip from its hostname
    he = gethostbyname(node->hostname);

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(MONITOR_FULL_PORT);
    servaddr.sin_addr = *((struct in_addr *) he->h_addr);

    // connect the client socket to server socket
    connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    // Receive information
    char buffer[BUFFER_SIZE];
    int total = 0;
    while (total < BUFFER_SIZE) {
        int n = read(sockfd, buffer + total, FIELD_SIZE);
        total += n;
    }

    // Extract number of processors from received data
    int cpus = *(int *) buffer;
    char *stats = buffer + sizeof(int);

    node->cpus = cpus;
    strcpy(node->stats, stats);

    // close the socket
    close(sockfd);
}

void *monitor_func(void *args) {
    int monitor_sockfd;
    socklen_t len;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in cli_addr;

    initialize_socket(&monitor_sockfd, MONITOR_LITE_PORT);
    initialize_socket(&controller_sockfd, CONTROLLER_PORT);

    print_log("Monitor thread initialized.");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        // Receive statistics from a node
        int n = recvfrom(monitor_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cli_addr, &len);
        buffer[n] = '\0';

        // Get current time to calculate last time the node was seen
        time_t now;
        time(&now);

        // Extract hostname from received data
        char hostname[NAME_LENGTH_MAX];
        strcpy(hostname, buffer);
        int slen = strlen(hostname);
        hostname[slen] = 0;
        char *stats = buffer + slen + 1;

        // Extract loadavg from received data
        float cpu_load = *(float *) stats;

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
                char info[FIELD_SIZE];
                sprintf(info, "Detected node '%s'.", hostname);
                print_log(info);

                // Reset information regarding tasks
                memset(nodes[index].processes, 0, sizeof(nodes[index].processes));
                memset(nodes[index].root_task, 0, sizeof(nodes[index].root_task));

                // Request node full information
                strcpy(nodes[index].hostname, hostname);
                request_full_info(index);
            }

            nodes[index].active = 1;
            nodes[index].last_seen = now;
            nodes[index].cpu_load = cpu_load;

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

void *updater_func(void *args) {
    char buffer[BUFFER_SIZE];
    print_log("Updater thread initialized.");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        for (int i = 0; i < NODES_MAX; ++i) {
            pthread_mutex_lock(&nodes_lock);

            if (!nodes[i].active) {
                pthread_mutex_unlock(&nodes_lock);
                continue; // Skip inactive nodes
            }

            pthread_mutex_lock(&tasks_lock);

            struct node *node = &nodes[i];
            int delta, task;

            // Get a task to adjust and its processes delta for this node
            delta = calculate_adjustment(node, &task);

            // Send signal to modify the number of processes
            if (delta) {
                // Activate monitoring in FlexMPI controller
                sprintf(buffer, "%d 0 4:on", tasks[task].flexmpi_id);
                send_controller_instruction(buffer, 0);

                // Send instruction to change processes
                sprintf(buffer, "%d 0 6:%s:%d", tasks[task].flexmpi_id, node->hostname, delta);
                send_controller_instruction(buffer, 1);
                node->processes[task] += delta;

                // Check until changes are done
                int diff = 1, task_processes = 0;
                for (int j = 0; j < NODES_MAX; ++j) {
                    if (nodes[j].active)
                        task_processes += nodes[j].processes[task]; // Store in task_processes its total number of processes
                }
                int times = 0;
                while (diff) {
                    // Keep trying until the number of processes is synchronized with FlexMPI
                    sprintf(buffer, "%d 2", tasks[task].flexmpi_id);
                    send_controller_instruction(buffer, 0);

                    socklen_t len;
                    struct sockaddr_in cli_addr;
                    recvfrom(controller_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cli_addr, &len);
                    char *saveptr, *procs;
                    strtok_r(buffer, " ", &saveptr);
                    procs = strtok_r(NULL, " ", &saveptr);

                    diff = task_processes - atoi(procs);

                    if (++times > 200) {
                        // Try again switching monitoring off and on
                        sprintf(buffer, "%d 0 4:off", tasks[task].flexmpi_id);
                        send_controller_instruction(buffer, 0);

                        sprintf(buffer, "%d 0 4:on", tasks[task].flexmpi_id);
                        send_controller_instruction(buffer, 0);
                    }
                }
                // Deactivate monitoring in FlexMPI controller
                sprintf(buffer, "%d 0 4:off", tasks[task].flexmpi_id);
                send_controller_instruction(buffer, 0);

                if (delta < 0)
                    sprintf(buffer, "Reduced load of task %d in node '%s'.", tasks[task].id, node->hostname);
                else
                    sprintf(buffer, "Increased load in node '%s' for task %d by %d processes.", node->hostname,
                            tasks[task].id,
                            delta);
                print_log(buffer);
            }
            pthread_mutex_unlock(&tasks_lock);
            pthread_mutex_unlock(&nodes_lock);
        }
    }
#pragma clang diagnostic pop
}

void start_monitor() {
    pthread_mutex_init(&nodes_lock, NULL);
    pthread_t monitor, updater;
    pthread_create(&monitor, NULL, monitor_func, NULL);
    pthread_create(&updater, NULL, updater_func, NULL);
}
