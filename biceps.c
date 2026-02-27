#define _GNU_SOURCE
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

char *buildPrompt(void);
void intHandler(int s);
int analyseCom(char *b);
char *copyString(char *s);
void freeWords(void);

static char **words;
static int nWords;

int main(int argc, char *argv[]){
    char *prompt;
    char *line;
    int i;

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

            if(analyseCom(line) > 0){
                printf("Command: %s\n", words[0]);
                if(nWords > 1){
                    printf("Arguments:\n");
                    for(i=1; i<nWords; i++){
                        printf("  %s\n", words[i]);
                    }
                }
                freeWords();
            }            
        }
        free(line);
    }

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
    exit(0);
}

int analyseCom(char *b){
    char *copy, *token, *aux, *sep = " \t\n";

    nWords = 0;
    words = NULL;
    copy = copyString(b);
    aux = copy;

    while((token = strsep(&aux, sep)) != NULL){
        if(*token == '\0') continue;
        
        words = realloc(words, (nWords + 1)*sizeof(char*));

        if(words == NULL){
            perror("Error reallocating memory for words array");
            free(copy);
            exit(EXIT_FAILURE);
        }
        words[nWords++] = copyString(token);
    }

    free(copy);
    return nWords;          
}

char *copyString(char *s){
    char *copy;
    int len = strlen(s) + 1;

    copy = malloc(len);
    if(copy == NULL){
        perror("Error allocating memory for string copying");
        exit(EXIT_FAILURE);
    }

    int i;
    for(i=0; i<len; i++){
        copy[i] = s[i];
    }  

    return copy;
}

void freeWords(void){
    int i;
    for(i=0; i<nWords; i++){
        free(words[i]);
    }
    free(words);
    nWords = 0;
    words = NULL;
}