#define _GNU_SOURCE
#include<stdio.h>
#include<readline/readline.h>
#include<readline/history.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include"gescom.h"

#define BICEPS_VERSION "1.00"

char *buildPrompt(void);
void intHandler(int s);

int main(int argc, char *argv[]){    
    char *prompt, *line, *historyPath;
    HIST_ENTRY *last;


    signal(SIGINT, intHandler);

    updateComInt(BICEPS_VERSION);
    listComInt();

    historyPath = getHistoryPath();
    stifle_history(HISTORY_SIZE);
    read_history(historyPath);

    prompt = buildPrompt();

    while(1){
        line = readline(prompt);

        if(line == NULL){
            printf("Exiting biceps shell. Goodbye!\n");
            break;
        }

        if(*line != '\0'){
            last = history_length > 0 ? history_get(history_base + history_length - 1) : NULL;
            if(last == NULL || strcmp(last->line, line) != 0){
                add_history(line);
            }
            execLine(line);
        }
        free(line);
    }

    write_history(historyPath);
    free(historyPath);

    free(prompt);
    return 0;
}

char *buildPrompt(void){
    char *user;
    char machine[256];
    char *prompt;
    int len;

    user = getenv("USER");
    if(user == NULL){
        fprintf(stderr, "Error: USER environment variable not set.\n");
        exit(EXIT_FAILURE);
    }

    if(gethostname(machine, sizeof(machine)) != 0){
        perror("Error getting hostname");
        exit(EXIT_FAILURE);
    }

    const char *sufix = (getuid() == 0) ? "#" : "$";

    // utilisateur@machine + sufix + ' ' + '\0'
    len = strlen(user) + 1 + strlen(machine) + strlen(sufix) + 2;
    prompt = malloc(len);
    if(prompt == NULL){
        perror("Error allocating memory for prompt");
        exit(EXIT_FAILURE);
    }

    snprintf(prompt, len, "%s@%s%s ", user, machine, sufix);

    return prompt;
}

void intHandler(int s){
    (void)s;
    printf("\n");
    rl_on_new_line();
    rl_redisplay();
}