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
    int local_processes = 0, min_processes = INT32_MAX, max_processes = INT32_MIN;
    int min_task = -1, max_task = -1;
    for (int i = 0; i < TASKS_MAX; ++i) {
        // Consider only active tasks
        if (tasks[i].active) {
            local_processes += node->processes[i];
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
    if (min_task == -1)
        return 0;

    int delta = 0;

    // Check if load needs to be reduced
    if (node->cpu_load > MAX_LOAD && local_processes) {
        float process_load = node->cpu_load / local_processes;

        // Set delta to get the processes to the middle of the interval [MAX_LOAD - LOAD_EPSILON, MAX_LOAD]
        float goal_load = (MAX_LOAD + MAX_LOAD - LOAD_EPSILON) / 2; // Value between max and min loads allowed
        delta = -(int) (node->processes[max_task] - goal_load / process_load);
        *task = max_task;

        // Adjust delta if the node is root for task
        if (node->root_task[max_task])
            delta = (int) fmax(delta, ROOT_PROCESSES - node->processes[max_task]);
    }
        // Check if load could be increased
    else if (node->cpu_load < MAX_LOAD - LOAD_EPSILON && local_processes < node->cpus - 1) {
        float process_load;
        if (local_processes)
            process_load = node->cpu_load / local_processes;
        else
            process_load = 0.25;

        // Set delta to get the processes to the middle of the interval [MAX_LOAD - LOAD_EPSILON, MAX_LOAD]
        float goal_load = (MAX_LOAD + MAX_LOAD - LOAD_EPSILON) / 2; // Value between max and min loads allowed
        float available_load = goal_load - node->cpu_load;
        int new_processes = (int) (available_load / process_load); // Estimation of new processes

        // Increase if new_processes > 0
        if (new_processes > 0) {
            // Set delta of processes to new_processes
            delta = (int) fmin(new_processes, node->cpus - 1 - local_processes);
            delta = (int) fmin(delta, MAX_DELTA_PROCESSES);
            delta = (int) fmin(delta, (float) total_cpus / max_tasks);
            *task = min_task;
        }
    }

    return delta;
}

void build_adjustments(struct adjustment *adjustments) {
    char buffer[FIELD_SIZE];

    pthread_mutex_lock(&nodes_lock);
    printf("lock monitor:91\n");
    for (int i = 0; i < NODES_MAX; ++i) {

        if (!nodes[i].active)
            continue; // Skip inactive nodes

        struct node *node = &nodes[i];
        int delta, task_index;

        // Get a task to adjust and its processes delta for this node
        pthread_mutex_lock(&tasks_lock);
        delta = calculate_adjustment(node, &task_index);
        struct task task = tasks[task_index];

        // Send signal to modify the number of processes
        if (delta) {
            adjustments[i].active = 1;
            adjustments[i].delta = delta;
            adjustments[i].task_index = task_index;

            if (delta < 0) {
                sprintf(buffer, "Reducing load of task %d in node '%s' by %d processes.", task.id, node->hostname,
                        -delta);
                print_log(buffer, 3);
            } else {
                sprintf(buffer, "Increasing load of task %d in node '%s' by %d processes.", task.id,
                        node->hostname,
                        delta);
                print_log(buffer, 4);
            }
        }
        pthread_mutex_unlock(&tasks_lock);
    }
    pthread_mutex_unlock(&nodes_lock);
    printf("unlock monitor:125\n");
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

    print_log("Monitor thread initialized.", 0);

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

        // Extract cpu_load from received data
        float cpu_load = *(float *) stats;

        // Search node in the list
        int index = 0;
        pthread_mutex_lock(&nodes_lock);
        printf("lock monitor:202\n");
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
                print_log(info, 6);

                // Reset information regarding tasks
                memset(nodes[index].processes, 0, sizeof(nodes[index].processes));
                memset(nodes[index].root_task, 0, sizeof(nodes[index].root_task));

                // Request node full information
                strcpy(nodes[index].hostname, hostname);
                request_full_info(index);

                // Update total CPUs
                total_cpus += nodes[index].cpus;
            }

            nodes[index].active = 1;
            nodes[index].last_seen = now;
            nodes[index].cpu_load = cpu_load;

            // Remove nodes that have not been seen in a while
            for (int i = 0; i < NODES_MAX; ++i) {
                if (nodes[i].active && difftime(now, nodes[i].last_seen) > TIMEOUT) {
                    nodes[i].active = 0;
                    total_cpus -= nodes[i].cpus; // Update total CPUs
                    sprintf(buffer, "Removed node '%s' after being inactive for %d seconds.", nodes[i].hostname,
                            TIMEOUT);
                    print_log(buffer, 0);
                }
            }
        }
        pthread_mutex_unlock(&nodes_lock);
        printf("unlock monitor:246\n");
#pragma clang diagnostic pop
    }
}

void *updater_func(void *args) {
    char buffer[BUFFER_SIZE], commands[FIELD_SIZE][TASKS_MAX];
    struct adjustment adjustments[NODES_MAX];
    int adjust[TASKS_MAX];
    print_log("Updater thread initialized.", 0);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        sleep(UPDATER_INTERVAL);

        memset(adjustments, 0, sizeof(adjustments));
        memset(commands, 0, sizeof(commands));
        memset(adjust, 0, sizeof(adjust));
        build_adjustments(adjustments);

        pthread_mutex_lock(&nodes_lock);
        printf("lock monitor:266\n");
        pthread_mutex_lock(&tasks_lock);

        // Build commands
        for (int i = 0; i < NODES_MAX; ++i) {
            if (adjustments[i].active) {
                // Add adjustment to the corresponding command so it can be performed.

                int task_index = adjustments[i].task_index;
                int delta = adjustments[i].delta;
                nodes[i].processes[task_index] += delta; // Update node information

                if (!adjust[task_index]) {
                    //Add command header
                    sprintf(commands[task_index], "%d 0 6", tasks[task_index].flexmpi_id);
                    adjust[task_index] = 1;
                }
                sprintf(buffer, ":%s:%d", nodes[i].hostname, delta);
                strcat(commands[task_index], buffer);
            }
        }

        pthread_mutex_unlock(&nodes_lock);
        printf("unlock monitor:291\n");
        pthread_mutex_unlock(&tasks_lock);

        // Execute commands
        for (int i = 0; i < TASKS_MAX; ++i) {
            if (adjust[i]) {
                pthread_mutex_lock(&controller_lock);
                pthread_mutex_lock(&tasks_lock);

                struct task task = tasks[i];

                pthread_mutex_lock(&finished_lock);
                if (finished[task.flexmpi_id % TASKS_MAX]) {
                    pthread_mutex_unlock(&finished_lock);
                    adjust[i] = 0;
                    continue;
                }
                pthread_mutex_unlock(&finished_lock);

                // Activate monitoring in FlexMPI controller
                sprintf(buffer, "%d 0 4:on", task.flexmpi_id);
                send_controller_instruction(buffer, -1);

                // Send instruction to change processes
                send_controller_instruction(commands[i], 1);

                int diff = 1, task_processes = 0;
                for (int j = 0; j < NODES_MAX; ++j) {
                    if (nodes[j].active)
                        task_processes += nodes[j].processes[i]; // Store in task_processes its total processes
                }

                int times = 0, received_report = 0;
                while (diff) {
                    // Keep trying until the number of processes is synchronized with FlexMPI
                    sprintf(buffer, "%d 2", task.flexmpi_id);
                    send_controller_instruction(buffer, -1);

                    socklen_t len;
                    struct sockaddr_in cli_addr;
                    int n = recvfrom(controller_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cli_addr, &len);
                    buffer[n] = '\0';
                    char *saveptr, *procs;
                    strtok_r(buffer, " ", &saveptr);
                    procs = strtok_r(NULL, " ", &saveptr);

                    diff = task_processes - atoi(procs);

                    pthread_mutex_lock(&finished_lock);
                    if (finished[task.flexmpi_id % TASKS_MAX]) {
                        received_report = 1;
                        pthread_mutex_unlock(&finished_lock);
                        break;
                    }
                    pthread_mutex_unlock(&finished_lock);

                    if (++times > 180) {
                        // Kill task
                        sprintf(buffer, "%d 0 5", task.flexmpi_id);
                        send_controller_instruction(buffer, 1);

                        finish_task(i);
                        break;
                    }
                }
                // Deactivate monitoring in FlexMPI controller
                sprintf(buffer, "%d 0 4:off", task.flexmpi_id);
                send_controller_instruction(buffer, -1);

                pthread_mutex_unlock(&tasks_lock);
                pthread_mutex_unlock(&controller_lock);

                if (!received_report) {
                    if (times > 180) { // Task was killed
                        sprintf(buffer, "Killed task %d.", task.id);
                        print_log(buffer, 0);
                    } else {
                        sprintf(buffer, "Updated load for task %d.", task.id);
                        print_log(buffer, 0);
                    }
                }
            }
        }
    }
#pragma clang diagnostic pop
}

void start_monitor(double max_load, double load_epsilon) {
    if (max_load) MAX_LOAD = max_load;
    else MAX_LOAD = 0.9;

    if (load_epsilon) LOAD_EPSILON = load_epsilon;
    else LOAD_EPSILON = 0.025;

    pthread_mutex_init(&nodes_lock, NULL);
    pthread_mutex_init(&controller_lock, NULL);

    pthread_t monitor, updater;
    pthread_create(&monitor, NULL, monitor_func, NULL);
    pthread_create(&updater, NULL, updater_func, NULL);
}
