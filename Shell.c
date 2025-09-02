#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 80
#define HISTORY_COUNT 10

char history[HISTORY_COUNT][MAX_LINE];
int history_count = 0;
int history_index = 0;

/* Add command to history */
void add_history(const char *cmd) {
    strncpy(history[history_index], cmd, MAX_LINE);
    history[history_index][MAX_LINE - 1] = '\0';
    history_index = (history_index + 1) % HISTORY_COUNT;
    history_count++;
}

/* Print last 10 commands */
void print_history() {
    write(STDOUT_FILENO, "\n--- Command History ---\n", 25);
    char line[200];
    int start = (history_count > HISTORY_COUNT) ? history_index : 0;
    int num = (history_count > HISTORY_COUNT) ? HISTORY_COUNT : history_count;

    for (int i = 0; i < num; i++) {
        int idx = (start + i) % HISTORY_COUNT;
        int n = snprintf(line, sizeof(line), "%d %s\n",
                         history_count - num + i + 1, history[idx]);
        write(STDOUT_FILENO, line, n);
    }
    write(STDOUT_FILENO, "------------------------\nCOMMAND-> ", 38);
    fflush(stdout);
}

/* Signal handler for Ctrl+C */
void handle_SIGINT(int sig) {
    (void)sig; // unused
    print_history();
}

/* Tokenize input */
int setup(char inputBuffer[], char *args[], int *background) {
    *background = 0;
    if (fgets(inputBuffer, MAX_LINE, stdin) == NULL) {
        printf("\n");
        exit(0);
    }

    size_t length = strlen(inputBuffer);
    if (length > 0 && inputBuffer[length - 1] == '\n')
        inputBuffer[length - 1] = '\0';

    if (strlen(inputBuffer) == 0) {
        args[0] = NULL;
        return 0;
    }

    char temp[MAX_LINE];
    strcpy(temp, inputBuffer);

    char *token = strtok(inputBuffer, " ");
    int i = 0;
    while (token != NULL && i < MAX_LINE/2) {
        if (strcmp(token, "&") == 0) {
            *background = 1;
        } else {
            args[i++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    return strlen(temp) > 0;
}

/* Resolve "r" and "r x" commands */
int handle_repeat(char inputBuffer[], char *args[], int *background) {
    // No commands in history yet
    if (history_count == 0) {
        printf("No commands in history.\n");
        args[0] = NULL;
        return 0;
    }

    // "r" only
    if (strcmp(args[0], "r") == 0 && args[1] == NULL) {
        strcpy(inputBuffer, history[(history_index - 1 + HISTORY_COUNT) % HISTORY_COUNT]);
    }
    // "r x"
    else if (strcmp(args[0], "r") == 0 && args[1] != NULL) {
        char ch = args[1][0];
        int found = 0;
        for (int i = 1; i <= HISTORY_COUNT && i <= history_count; i++) {
            int idx = (history_index - i + HISTORY_COUNT) % HISTORY_COUNT;
            if (history[idx][0] == ch) {
                strcpy(inputBuffer, history[idx]);
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("No matching command found for '%c'.\n", args[1][0]);
            args[0] = NULL;
            return 0;
        }
    } else {
        return 1; // not an "r" command
    }

    printf("Re-executing: %s\n", inputBuffer);
    add_history(inputBuffer); // store resolved command in history

    // Re-tokenize
    char *token = strtok(inputBuffer, " ");
    int i = 0;
    while (token != NULL && i < MAX_LINE/2) {
        if (strcmp(token, "&") == 0) {
            *background = 1;
        } else {
            args[i++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    return 0; // handled successfully
}

int main(void) {
    char inputBuffer[MAX_LINE];
    int background;
    char *args[MAX_LINE/2 + 1];

    /* Set up SIGINT handler */
    struct sigaction handler;
    handler.sa_handler = handle_SIGINT;
    sigemptyset(&handler.sa_mask);
    handler.sa_flags = SA_RESTART;
    sigaction(SIGINT, &handler, NULL);

    while (1) {
        printf("COMMAND-> ");
        fflush(stdout);

        if (!setup(inputBuffer, args, &background)) continue;

        // Exit command
        if (strcmp(args[0], "exit") == 0) {
            printf("Exiting shell...\n");
            break;
        }

        // History command
        if (strcmp(args[0], "history") == 0) {
            print_history();
            continue;
        }

        // Repeat command (r / r x)
        if (strcmp(args[0], "r") == 0) {
            if (!handle_repeat(inputBuffer, args, &background)) {
                continue; // if failed, skip
            }
        } else {
            add_history(inputBuffer); // normal command stored in history
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        } else if (pid == 0) {
            if (execvp(args[0], args) == -1) {
                perror("execvp failed");
            }
            exit(1);
        } else {
            if (!background) {
                waitpid(pid, NULL, 0);
            }
        }
    }
    return 0;
}
