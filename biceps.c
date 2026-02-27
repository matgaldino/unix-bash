#define _GNU_SOURCE
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

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
    exit(0);
}

int main(int argc, char *argv[]){
    char *prompt;
    char *line;

    signal(SIGINT, intHandler);

    prompt = buildPrompt();

    while(1){
        line = readline(prompt);

        if(line == NULL){
            printf("\n");
            break;
        }

        if(*line != '\0'){
            add_history(line);
        }
        printf("Command entered: %s\n", line);

        free(line);
    }

    free(prompt);
    return 0;
}