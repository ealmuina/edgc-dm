#include "include/task.h"

int FLEXMPI_ID = 0;

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

struct task get_task_info(int id, long *code) {
    char url[BUFFER_SIZE];
    sprintf(url, "%s?domainId=%d", TASK_ADDR, id);

    char *text = get(url, code);
    struct task task;

    if (*code != 200)
        return task;

    json_t *root = json_loads(text, 0, NULL);

    task.id = json_integer_value(json_object_get(root, "id"));
    task.cpu_intensity = json_integer_value(json_object_get(root, "cpu_intensity"));
    task.com_intensity = json_integer_value(json_object_get(root, "com_intensity"));
    task.io_intensity = json_integer_value(json_object_get(root, "io_intensity"));

    strcpy(task.kernel, json_string_value(json_object_get(root, "kernel")));
    strcpy(task.input, json_string_value(json_object_get(root, "input")));
    strcpy(task.output, json_string_value(json_object_get(root, "output")));
    strcpy(task.unpack, json_string_value(json_object_get(root, "unpack")));
    strcpy(task.pack, json_string_value(json_object_get(root, "pack")));

    strcpy(task.kernel_md5, json_string_value(json_object_get(root, "kernel_md5")));
    strcpy(task.input_md5, json_string_value(json_object_get(root, "input_md5")));
    strcpy(task.unpack_md5, json_string_value(json_object_get(root, "unpack_md5")));
    strcpy(task.pack_md5, json_string_value(json_object_get(root, "pack_md5")));

    task.flexmpi_id = FLEXMPI_ID++;

    json_decref(root);
    free(text);
    return task;
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

void request_execution(struct task *task, int task_index) {
    int root_node = 0, max_cores = -1;

    pthread_mutex_lock(&monitor_lock);
    for (int i = 0; i < NODES_MAX; ++i) {
        if (nodes[i].active) {
            // Cores to be used will be the CPUs * free_fraction_of_load
            int cores = (int) roundf(nodes[i].cpus * fmax(0, 1 - nodes[i].cpu_load + LOAD_EPSILON));

            // Root node will be the one with the most cores
            if (cores > max_cores) {
                root_node = i;
                max_cores = cores;
            }
        }
    }

    // Set the number of processes used in the root node
    nodes[root_node].processes[task_index] = max_cores;

    char command[1024];
    sprintf(command,
            "nping --udp -p 8900 -c 1 localhost --data-string \"-1 dynamic:5000:%d:%d:%d:2.500000:100:%s:%d\" %s",
            task->cpu_intensity,
            task->com_intensity,
            task->io_intensity,
            nodes[root_node].hostname,
            max_cores,
            "> /dev/null 2> /dev/null"
    );
    pthread_mutex_unlock(&monitor_lock);

    printf("\t-> %s\n", command);
    system(command);
}

int process_task(int id) {
    char buffer[BUFFER_SIZE];

    // Request task
    long code;
    struct task task = get_task_info(id, &code);

    if (code != 200) {
        sprintf(buffer, "Error requesting task.");
        print_log(buffer);
    } else {
        sprintf(buffer, "Received task %d.", task.id);
        print_log(buffer);

        // Save task
        int i;
        pthread_mutex_lock(&tasks_lock);
        for (i = 0; i < TASKS_MAX; ++i) {
            if (!tasks[i].active) {
                tasks[i].id = task.id;
                tasks[i].active = 1;
                tasks[i].cpu_intensity = task.cpu_intensity;
                tasks[i].com_intensity = task.com_intensity;
                tasks[i].io_intensity = task.io_intensity;
                strcpy(tasks[i].kernel, task.kernel);
                strcpy(tasks[i].input, task.input);
                strcpy(tasks[i].output, task.output);
                strcpy(tasks[i].unpack, task.pack);
                strcpy(tasks[i].kernel_md5, task.kernel_md5);
                strcpy(tasks[i].input_md5, task.input_md5);
                strcpy(tasks[i].unpack_md5, task.unpack_md5);
                strcpy(tasks[i].pack_md5, task.pack_md5);
                break;
            }
        }
        pthread_mutex_unlock(&tasks_lock);

        if (i != TASKS_MAX) {
            // Download task files
            download_task(&task);
            sprintf(buffer, "Downloaded task %d content.", task.id);
            print_log(buffer);

            // Validate files and put in execution
            if (validate_task(&task)) {
                sprintf(buffer, "Task %d downloaded correctly.", task.id);
                print_log(buffer);
                request_execution(&task, i);
                sprintf(buffer, "Requested execution of task %d.", task.id);
                print_log(buffer);
                return 0;
            } else {
                sprintf(buffer, "Task %d corrupted. It will be cancelled.", task.id);
                print_log(buffer);
                // Set task space status to inactive
                pthread_mutex_lock(&tasks_lock);
                tasks[i].active = 0;
                pthread_mutex_unlock(&tasks_lock);
                return -2;
            }
        } else {
            print_log("Error: Limit of tasks in execution has been reached.");
            return -1;
        }
    }
}
