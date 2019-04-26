#include "include/monitor.h"

void *monitor_func(void *args) {
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in serv_addr, cli_addr;
    int n, len;

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
        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cli_addr, &len);
        buffer[n] = '\0';

        if (!cli_addr.sin_addr.s_addr) continue; //skip if node address is 0

        // Get time for knowing last time the node was seen
        time_t now;
        time(&now);

        // Extract loadavg from received data
        float cpu_load = *(float *) buffer;
        char *stats = buffer + sizeof(float);

        // Extract number of processors from received data
        int cpus = *(int *) stats;
        stats += sizeof(int);

        // Search node in the list
        int index = 0;
        pthread_mutex_lock(&monitor_lock);
        for (int i = 0; i < NODES_MAX; ++i) {
            if (nodes[index].active) index = i; // index will be the first empty position
            if (nodes[i].active && nodes[i].addr == cli_addr.sin_addr.s_addr) {
                index = i; // found!
                break;
            }
        }
        // Update or create entry
        if (index != NODES_MAX) { // There is still space for this node
            nodes[index].active = 1;
            strcpy(nodes[index].stats, stats);
            nodes[index].last_seen = now;
            nodes[index].addr = cli_addr.sin_addr.s_addr;
            nodes[index].cpu_load = cpu_load;
            nodes[index].cpus = cpus;

            // Update processes if necessary
            if (nodes[index].processes) {
                if (cpu_load > MAX_LOAD) { // Load needs to be reduced
                    nodes[index].processes--;
                    sprintf(buffer, "Reduced load in node %d.", index);
                    print_log(buffer);

                    // TODO: Send signal to remove one process in node
                } else {
                    // Load could be increased
                    float process_load = cpu_load / nodes[index].processes;
                    float available_load = MAX_LOAD - cpu_load;
                    int new_processes = (int) (available_load / process_load);

                    // Increase if new_processes > 0
                    if (new_processes > 0) {
                        nodes[index].processes += new_processes;
                        sprintf(buffer, "Increased load in node %d by %d processes.", index, new_processes);
                        print_log(buffer);
                        // TODO: Send signal to create new_processes in node
                    }
                }
            }
        }
        // Remove nodes that have not been seen in a while
        for (int i = 0; i < NODES_MAX; ++i) {
            if (difftime(now, nodes[i].last_seen) > TIMEOUT)
                nodes[i].active = 0;
        }
        pthread_mutex_unlock(&monitor_lock);
    }
#pragma clang diagnostic pop
}

void start_monitor() {
    pthread_mutex_init(&monitor_lock, NULL);
    pthread_t m;
    pthread_create(&m, NULL, monitor_func, NULL);
}
