#define _GNU_SOURCE
#include<stdio.h>
#include<readline/readline.h>
#include<readline/history.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include"gescom.h"

#define BICEPS_VERSION "2.0"

static char *buildPrompt(void);
static void intHandler(int s);
static void initShell(void);
static void maybeAddHistory(const char *line);
static void runShellLoop(const char *prompt);
static const char *getUserName(void);
static void getMachineName(char *machine, size_t machineSize);
static char *createPrompt(const char *user, const char *machine, const char *suffix);

int main(int argc, char *argv[]){    
    char *prompt, *historyPath;

    (void)argc;
    (void)argv;
    initShell();

    historyPath = getHistoryPath();
    stifle_history(HISTORY_SIZE);
    read_history(historyPath);
    prompt = buildPrompt();
    runShellLoop(prompt);
    write_history(historyPath);
    free(historyPath);
    free(prompt);
    return 0;
}

static void initShell(void){
    signal(SIGINT, intHandler);
    updateComInt(BICEPS_VERSION);
    listComInt();
}

static void maybeAddHistory(const char *line){
    HIST_ENTRY *last;

    last = history_length > 0 ? history_get(history_base + history_length - 1) : NULL;
    if(last == NULL || strcmp(last->line, line) != 0){
        add_history(line);
    }
}

static void runShellLoop(const char *prompt){
    char *line;

    while(1){
        line = readline(prompt);
        if(line == NULL){
            printf("Exiting biceps shell. Goodbye!\n");
            break;
        }
        if(*line != '\0'){
            maybeAddHistory(line);
            execLine(line);
        }
        free(line);
    }
}

static const char *getUserName(void){
    const char *user;

    user = getenv("USER");
    if(user == NULL){
        fprintf(stderr, "Error: USER environment variable not set.\n");
        exit(EXIT_FAILURE);
    }
    return user;
}

static void getMachineName(char *machine, size_t machineSize){
    if(gethostname(machine, machineSize) != 0){
        perror("Error getting hostname");
        exit(EXIT_FAILURE);
    }
}

static char *createPrompt(const char *user, const char *machine, const char *suffix){
    char *prompt;
    int len;

    len = strlen(user) + 1 + strlen(machine) + strlen(suffix) + 2;
    prompt = malloc(len);
    if(prompt == NULL){
        perror("Error allocating memory for prompt");
        exit(EXIT_FAILURE);
    }
    snprintf(prompt, len, "%s@%s%s ", user, machine, suffix);
    return prompt;
}

static char *buildPrompt(void){
    char machine[256];
    const char *user;
    const char *suffix;

    user = getUserName();
    getMachineName(machine, sizeof(machine));
    suffix = (getuid() == 0) ? "#" : "$";
    return createPrompt(user, machine, suffix);
}

static void intHandler(int s){
    (void)s;
    printf("\n");
    rl_on_new_line();
    rl_redisplay();
}