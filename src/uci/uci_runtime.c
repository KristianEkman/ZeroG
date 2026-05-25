#include "uci/uci_internal.h"

#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    Position board;
    SearchLimits limits;
    FILE *output;
    SearchResult result;
    int search_status;
    atomic_bool finished;
} UciSearchTask;

typedef struct
{
    pthread_t thread;
    int active;
    UciSearchTask task;
} UciSearchRuntime;

static int uci_read_command(FILE *input, char **buffer, size_t *capacity)
{
    size_t used = 0;

    if (input == NULL || buffer == NULL || capacity == NULL || *buffer == NULL || *capacity == 0)
    {
        return 0;
    }

    (*buffer)[0] = '\0';

    while (fgets(*buffer + used, (int)(*capacity - used), input) != NULL)
    {
        used += strlen(*buffer + used);

        if (used > 0 && (*buffer)[used - 1] == '\n')
        {
            return 1;
        }

        if (used + 1 < *capacity)
        {
            continue;
        }

        {
            size_t new_capacity = *capacity * 2;
            char *new_buffer = realloc(*buffer, new_capacity);

            if (new_buffer == NULL)
            {
                return 0;
            }

            *buffer = new_buffer;
            *capacity = new_capacity;
        }
    }

    return used > 0;
}

static void uci_search_runtime_init(UciSearchRuntime *runtime)
{
    if (runtime == NULL)
    {
        return;
    }

    runtime->active = 0;
}

static void *uci_search_thread_main(void *context)
{
    UciSearchTask *task = context;
    Move best_move = 0;

    if (task == NULL)
    {
        return NULL;
    }

    task->search_status = uci_search_with_limits(&task->board,
                                                 &task->limits,
                                                 task->output,
                                                 &task->result);

    if (task->search_status == 0 && task->result.has_legal_move)
    {
        best_move = task->result.best_move;
    }

    (void)uci_write_bestmove(&task->board, task->output, best_move);
    atomic_store_explicit(&task->finished, 1, memory_order_release);
    return NULL;
}

static int uci_start_async_search(UciSearchRuntime *runtime,
                                  const Position *board,
                                  const SearchLimits *limits,
                                  FILE *output)
{
    if (runtime == NULL || board == NULL || limits == NULL || output == NULL || runtime->active)
    {
        return -1;
    }

    runtime->task.board = *board;
    runtime->task.limits = *limits;
    runtime->task.output = output;
    runtime->task.result.best_move = 0;
    runtime->task.result.score = 0;
    runtime->task.result.has_legal_move = 0;
    runtime->task.result.node_count = 0;
    runtime->task.search_status = -1;
    atomic_init(&runtime->task.finished, 0);

    search_reset_stop_request();

    if (pthread_create(&runtime->thread, NULL, uci_search_thread_main, &runtime->task) != 0)
    {
        search_reset_stop_request();
        return -1;
    }

    runtime->active = 1;
    return 0;
}

static void uci_join_active_search(UciSearchRuntime *runtime, int request_stop)
{
    if (runtime == NULL || !runtime->active)
    {
        return;
    }

    if (request_stop)
    {
        search_request_stop();
    }

    (void)pthread_join(runtime->thread, NULL);
    runtime->active = 0;
    search_reset_stop_request();
}

static void uci_join_finished_search(UciSearchRuntime *runtime)
{
    if (runtime == NULL || !runtime->active)
    {
        return;
    }

    if (!atomic_load_explicit(&runtime->task.finished, memory_order_acquire))
    {
        return;
    }

    uci_join_active_search(runtime, 0);
}

void uci_run(FILE *input, FILE *output)
{
    UciState state;
    UciSearchRuntime search_runtime;
    char *line;
    size_t line_capacity = UCI_LINE_CAPACITY;

    if (input == NULL || output == NULL)
    {
        return;
    }

    line = malloc(line_capacity);
    if (line == NULL)
    {
        return;
    }

    if (uci_state_init(&state) != 0)
    {
        free(line);
        return;
    }

    uci_search_runtime_init(&search_runtime);

    while (uci_read_command(input, &line, &line_capacity))
    {
        int should_quit;
        const char *args;
        const char *trimmed_line;
        size_t line_length = strlen(line);

        while (line_length > 0 && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
        {
            line[--line_length] = '\0';
        }

        trimmed_line = uci_skip_spaces(line);
        uci_join_finished_search(&search_runtime);

        if (*trimmed_line == '\0')
        {
            continue;
        }

        if (search_runtime.active && uci_line_starts_with(trimmed_line, "stop", &args))
        {
            (void)args;
            uci_join_active_search(&search_runtime, 1);
            continue;
        }

        if (search_runtime.active && uci_line_starts_with(trimmed_line, "quit", &args))
        {
            (void)args;
            uci_join_active_search(&search_runtime, 1);
            return;
        }

        if (search_runtime.active &&
            (uci_line_starts_with(trimmed_line, "position", &args) ||
             uci_line_starts_with(trimmed_line, "ucinewgame", &args) ||
             uci_line_starts_with(trimmed_line, "setoption", &args) ||
             uci_line_starts_with(trimmed_line, "go", &args)))
        {
            uci_join_active_search(&search_runtime, 1);
        }

        if (uci_line_starts_with(trimmed_line, "go", &args))
        {
            SearchLimits limits;

            if (uci_parse_go_search_limits(&state, args, &limits) != 0 ||
                uci_start_async_search(&search_runtime, &state.board, &limits, output) != 0)
            {
                if (fprintf(output, "info string invalid command\n") < 0)
                {
                    break;
                }

                fflush(output);
            }

            continue;
        }

        if (uci_handle_line(&state, trimmed_line, output, &should_quit) != 0)
        {
            if (fprintf(output, "info string invalid command\n") < 0)
            {
                break;
            }

            fflush(output);
            continue;
        }

        if (should_quit)
        {
            break;
        }
    }

    if (search_runtime.active)
    {
        uci_join_active_search(&search_runtime, 0);
    }

    free(line);
}