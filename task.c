#include "task.h"

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

    pthread_mutex_lock(&monitor_lock);
    for (int i = 0; i < NODES_MAX; ++i) {
        if (nodes[i].active) {
            // Cores to be used will be the CPUs * free_fraction_of_load
            cores[i] = (int) roundf(nodes[i].cpus * fmax(0, 1 - nodes[i].cpu_load + LOAD_EPSILON));
            nodes[i].processes = cores[i];
        }
    }
    pthread_mutex_unlock(&monitor_lock);

    // TODO: Send signal to start execution
}

int process_task(int id) {
    char buffer[BUFFER_SIZE];

    // Request task
    struct task task = get_task_info(id);
    sprintf(buffer, "Received task %d.", task.id);
    print_log(buffer);

    // Save task
    int i;
    pthread_mutex_lock(&tasks_lock);
    for (i = 0; i < TASKS_MAX; ++i) {
        if (!tasks[i].active) {
            tasks[i].id = task.id;
            tasks[i].active = 1;
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
            request_execution(&task);
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
