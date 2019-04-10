#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>

#include <jansson.h>

#include "utils.c"

#define REGISTER_ADDR "http://localhost:5000/api/register"
#define SERVER_ADDR "http://localhost:5000"
#define TASK_ADDR "http://localhost:5000/api/task"
#define REPORT_ADDR "http://localhost:5000/api/report"

struct task {
    const char *kernel, *input, *kernel_md5, *input_md5;
    int id;
};

void download_task(struct task *task) {
    char buffer[1024];

    sprintf(buffer, "%s/%s", SERVER_ADDR, task->kernel);
    download(buffer, task->kernel);

    sprintf(buffer, "%s/%s", SERVER_ADDR, task->input);
    download(buffer, task->input);
}

struct task get_task_info(int id) {
    char url[1024];
    sprintf(url, "%s?domainId=%d", TASK_ADDR, id);

    char *text = get(url);
    struct task task_info;

    json_t *root = json_loads(text, 0, NULL);
    task_info.id = json_integer_value(json_object_get(root, "id"));
    task_info.kernel = json_string_value(json_object_get(root, "kernel"));
    task_info.kernel_md5 = json_string_value(json_object_get(root, "kernel_md5"));
    task_info.input = json_string_value(json_object_get(root, "input"));
    task_info.input_md5 = json_string_value(json_object_get(root, "input_md5"));

    free(text);
    return task_info;
}

int register_domain() {
    char *text = post(REGISTER_ADDR, "{\"nodes\":1}");
    json_t *root = json_loads(text, 0, NULL);
    free(text);
    return json_integer_value(json_object_get(root, "id"));
}

int validate_task(struct task *task) {
    char *kernel_hash = md5(task->kernel);
    int kernel_comp = strcmp(kernel_hash, task->kernel_md5);
    free(kernel_hash);

    char *input_hash = md5(task->input);
    int input_comp = strcmp(input_hash, task->input_md5);
    free(input_hash);

    if (kernel_comp != 0) return 0;
    if (input_comp != 0) return 0;
    return 1;
}

void report_task(struct task *task, int domain_id) {
    char buffer[1024];
    sprintf(buffer, "%s?taskId=%d&domainId=%d", REPORT_ADDR, task->id, domain_id);
    upload(buffer, "output.txt");
}

void print_log(char *msg) {
    time_t rawtime;
    time(&rawtime);
    char *time = strtok(ctime(&rawtime), "\n");
    printf("[%s] %s", time, msg);
}

int main() {
    char buffer[1024];

    int id = register_domain();
    sprintf(buffer, "Registered with id: %d\n", id);
    print_log(buffer);

    struct task task = get_task_info(id);
    sprintf(buffer, "Received task %d\n", task.id);
    print_log(buffer);

    download_task(&task);
    sprintf(buffer, "Downloaded task %d content\n", task.id);
    print_log(buffer);

    if (validate_task(&task)) {
        sprintf(buffer, "Task %d downloaded correctly\n", task.id);
        print_log(buffer);

        report_task(&task, id);
        sprintf(buffer, "Result of task %d reported\n", task.id);
        print_log(buffer);
    } else {
        sprintf(buffer, "Task %d corrupted\n", task.id);
        print_log(buffer);
    }

    return 0;
}