#include "include/task.h"

int FLEXMPI_ID = 0;

void download_task(struct task *task) {
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "%s/%s", repository_url, task->kernel);
    download(buffer, task->kernel);

    sprintf(buffer, "%s/%s", repository_url, task->input);
    download(buffer, task->input);

    sprintf(buffer, "%s/%s", repository_url, task->unpack);
    download(buffer, task->unpack);
    sprintf(buffer, "chmod +x %s", task->unpack);
    system(buffer);

    sprintf(buffer, "%s/%s", repository_url, task->pack);
    download(buffer, task->pack);
    sprintf(buffer, "chmod +x %s", task->pack);
    system(buffer);
}

struct task get_task_info(int id, long *code) {
    char url[FIELD_SIZE];
    sprintf(url, "%s/%s?domainId=%d", repository_url, TASK_URL, id);

    char *text = get(url, code);
    struct task task;

    if (*code != 200)
        return task;

    json_t *root = json_loads(text, 0, NULL);

    task.id = json_integer_value(json_object_get(root, "id"));

    strcpy(task.kernel, json_string_value(json_object_get(root, "kernel")));
    strcpy(task.input, json_string_value(json_object_get(root, "input")));
    strcpy(task.output, json_string_value(json_object_get(root, "output")));
    strcpy(task.unpack, json_string_value(json_object_get(root, "unpack")));
    strcpy(task.pack, json_string_value(json_object_get(root, "pack")));

    strcpy(task.kernel_md5, json_string_value(json_object_get(root, "kernel_md5")));
    strcpy(task.input_md5, json_string_value(json_object_get(root, "input_md5")));
    strcpy(task.unpack_md5, json_string_value(json_object_get(root, "unpack_md5")));
    strcpy(task.pack_md5, json_string_value(json_object_get(root, "pack_md5")));

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

    int input;
    if (strcmp("", task->input) == 0)
        input = 1;
    else
        input = validate_file(task->input, task->input_md5);

    int unpack = validate_file(task->unpack, task->unpack_md5);
    int pack = validate_file(task->pack, task->pack_md5);

    return kernel && input && unpack && pack;
}

void request_execution(struct task *task, int task_index) {
    int root_node = 0, max_cores = -1;

    // Execute unpacking script
    system(task->unpack);

    // Find the best node for starting execution in it
    pthread_mutex_lock(&nodes_lock);
    for (int i = 0; i < NODES_MAX; ++i) {
        if (nodes[i].active) {
            // Cores to be used will be the CPUs * free_fraction_of_load
            int cores = (int) roundf(nodes[i].cpus * fmax(0, 1 - nodes[i].cpu_load));

            // Root node will be the one with the most cores
            if (cores > max_cores) {
                root_node = i;
                max_cores = cores;
            }
        }
    }

    // Set the number of processes used in the root node
    nodes[root_node].processes[task_index] = ROOT_PROCESSES;
    nodes[root_node].root_task[task_index] = 1;

    // Activate task and set its flexmpi_id
    pthread_mutex_lock(&tasks_lock);
    tasks[task_index].active = task->active = 1;
    tasks[task_index].flexmpi_id = task->flexmpi_id = FLEXMPI_ID++;
    pthread_mutex_unlock(&tasks_lock);

    // Send command to start application
    pthread_mutex_lock(&controller_lock);
    char command[FIELD_SIZE];
    sprintf(command,
            "-1 dynamic:20000:2:1:0:2.500000:10000:%s:%d",
            nodes[root_node].hostname,
            nodes[root_node].processes[task_index]
    );
    send_controller_instruction(command, 1);

    // Send command to load kernel
    sprintf(command, "%d 1 iocmd:5", task->flexmpi_id);
    send_controller_instruction(command, 1);
    pthread_mutex_unlock(&controller_lock);

    pthread_mutex_unlock(&nodes_lock);
}

int process_task(int id) {
    char buffer[BUFFER_SIZE];

    // Request task
    long code;
    struct task task = get_task_info(id, &code);

    if (code != 200) {
        sprintf(buffer, "Error requesting task.");
        print_log(buffer, 0);
    } else {
        sprintf(buffer, "Received task %d.", task.id);
        print_log(buffer, 0);

        // Save task
        int i;
        pthread_mutex_lock(&tasks_lock);
        for (i = 0; i < TASKS_MAX; ++i) {
            if (!tasks[i].active) {
                tasks[i].id = task.id;
                strcpy(tasks[i].kernel, task.kernel);
                strcpy(tasks[i].input, task.input);
                strcpy(tasks[i].output, task.output);
                strcpy(tasks[i].pack, task.pack);
                strcpy(tasks[i].unpack, task.unpack);
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
            print_log(buffer, 0);

            // Validate files and put in execution
            if (validate_task(&task)) {
                sprintf(buffer, "Task %d downloaded correctly.", task.id);
                print_log(buffer, 0);
                request_execution(&task, i);
                sprintf(buffer, "Requested execution of task %d.", task.id);
                print_log(buffer, 2);
                return 0;
            } else {
                sprintf(buffer, "Task %d corrupted. It will be cancelled.", task.id);
                print_log(buffer, 0);
                // Set task space status to inactive
                pthread_mutex_lock(&tasks_lock);
                tasks[i].active = 0;
                pthread_mutex_unlock(&tasks_lock);
                return -2;
            }
        } else {
            print_log("Error: Limit of tasks in execution has been reached.", 0);
            return -1;
        }
    }
}
