#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "lf_stack.h"

#define BUFFER_SIZE 1024

static int execute_workload(char *buffer)
{
    LFStack stack;

    stack_init(&stack);

    int pushes = 0;
    int pops = 0;
    int failures = 0;

    char *save = NULL;

    char *line = strtok_r(buffer, "\n", &save);

    while (line != NULL) {

        if (strncmp(line, "PUSH ", 5) == 0) {

            char *end = NULL;

            long value = strtol(
                line + 5,
                &end,
                10
            );

            if (end == line + 5 || *end != '\0') {

                failures++;

            } else {

                stack_push(&stack, (int)value);
                pushes++;
            }

        } else if (strcmp(line, "POP") == 0) {

            int value;

            if (stack_pop(&stack, &value)) {

                printf(
                    "  executed POP -> %d\n",
                    value
                );

            } else {

                printf(
                    "  executed POP -> EMPTY\n"
                );
            }

            pops++;

        } else if (*line != '\0') {

            failures++;
        }

        line = strtok_r(NULL, "\n", &save);
    }

    stack_thread_cleanup();
    stack_destroy(&stack);

    printf(
        "Commands executed : %d\n",
        pushes + pops
    );

    printf(
        "PUSH commands     : %d\n",
        pushes
    );

    printf(
        "POP commands      : %d\n",
        pops
    );

    return failures == 0 ? 0 : -1;
}

int main(void)
{
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {

        close(pipefd[0]);

        const char *workload =
            "PUSH 100\n"
            "PUSH 200\n"
            "PUSH 300\n"
            "POP\n"
            "POP\n"
            "PUSH 400\n"
            "POP\n"
            "POP\n";

        size_t length = strlen(workload);
        size_t written = 0;

        while (written < length) {

            ssize_t n = write(
                pipefd[1],
                workload + written,
                length - written
            );

            if (n < 0) {
                perror("write");
                close(pipefd[1]);
                _exit(1);
            }

            written += (size_t)n;
        }

        close(pipefd[1]);
        _exit(0);
    }

    close(pipefd[1]);

    char buffer[BUFFER_SIZE];
    size_t used = 0;

    printf("\n");
    printf("============================================================\n");
    printf("            PIPE-FED LOCKLAB WORKLOAD TEST\n");
    printf("============================================================\n");

    while (used + 1 < sizeof(buffer)) {

        ssize_t n = read(
            pipefd[0],
            buffer + used,
            sizeof(buffer) - used - 1
        );

        if (n < 0) {
            perror("read");
            close(pipefd[0]);
            return 1;
        }

        if (n == 0)
            break;

        used += (size_t)n;
    }

    buffer[used] = '\0';

    close(pipefd[0]);

    int status;

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    printf(
        "Workload received : %zu bytes\n",
        used
    );

    printf("------------------------------------------------------------\n");
    printf("Executing pipe-fed commands:\n");

    int execution_ok = execute_workload(buffer);

    printf("------------------------------------------------------------\n");

    if (WIFEXITED(status) &&
        WEXITSTATUS(status) == 0 &&
        execution_ok == 0) {

        printf("Pipe workload     : PASS\n");

    } else {

        printf("Pipe workload     : FAIL\n");
        return 1;
    }

    printf("============================================================\n\n");

    return 0;
}