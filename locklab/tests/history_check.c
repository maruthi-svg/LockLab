#include <stdio.h>
#include <string.h>

#define MAX_HISTORY 10000

typedef struct {
    int start;
    int end;
    int thread;
    char operation[8];
    int value;
    char result[16];
} Record;

static Record history[MAX_HISTORY];
static int count = 0;

static int parse_history(const char *filename)
{
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror("history");
        return -1;
    }

    char line[256];

    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return -1;
    }

    while (count < MAX_HISTORY &&
           fgets(line, sizeof(line), file)) {

        Record *r = &history[count];

        if (sscanf(
                line,
                "%d,%d,%d,%7[^,],%d,%15[^\n]",
                &r->start,
                &r->end,
                &r->thread,
                r->operation,
                &r->value,
                r->result) == 6) {

            if (r->start <= 0 ||
                r->end < r->start)
                continue;

            count++;
        }
    }

    fclose(file);

    return 0;
}

static int stack[MAX_HISTORY];
static int top = 0;

static unsigned char used[MAX_HISTORY];

static int search_linearization(void)
{
    int complete = 1;

    for (int i = 0; i < count; i++) {
        if (!used[i]) {
            complete = 0;
            break;
        }
    }

    if (complete)
        return 1;

    for (int i = 0; i < count; i++) {

        Record *candidate = &history[i];

        if (used[i])
            continue;

        int predecessor_missing = 0;

        for (int j = 0; j < count; j++) {

            if (i == j || used[j])
                continue;

            if (history[j].end < candidate->start) {
                predecessor_missing = 1;
                break;
            }
        }

        if (predecessor_missing)
            continue;

        if (strcmp(candidate->operation, "PUSH") == 0) {

            if (top >= MAX_HISTORY)
                continue;

            used[i] = 1;
            stack[top++] = candidate->value;

            if (search_linearization())
                return 1;

            top--;
            used[i] = 0;

        } else if (strcmp(candidate->operation, "POP") == 0) {

            int old_top = top;
            int valid = 0;

            if (strcmp(candidate->result, "EMPTY") == 0) {

                valid = (top == 0);

            } else if (top > 0) {

                valid =
                    (stack[top - 1] ==
                     candidate->value);
            }

            if (!valid)
                continue;

            used[i] = 1;

            if (top > 0)
                top--;

            if (search_linearization())
                return 1;

            top = old_top;
            used[i] = 0;
        }
    }

    return 0;
}

static int check_linearizability(void)
{
    memset(used, 0, sizeof(used));

    top = 0;

    return search_linearization();
}

int main(void)
{
    printf("\n");
    printf("============================================================\n");
    printf("          LOCKLAB LINEARIZABILITY SPOT CHECK\n");
    printf("============================================================\n");

    if (parse_history("history.log") != 0) {

        printf("History parsing    : FAIL\n");
        return 1;
    }

    printf(
        "Records loaded     : %d\n",
        count);

    if (count == 0) {

        printf("Linearizability    : FAIL\n");
        return 1;
    }

    if (check_linearizability()) {

        printf("Model check        : PASS\n");
        printf("Linearizability    : PASS\n");

    } else {

        printf("Model check        : FAIL\n");
        printf("Linearizability    : FAIL\n");
        return 1;
    }

    printf("============================================================\n\n");

    return 0;
}