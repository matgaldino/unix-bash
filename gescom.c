#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include<readline/history.h>
#include "gescom.h"

#define GESCOM_VERSION "1.00"
static char *shell_version = "unkown";

static char **words;
static int nWords;

static comInt tabCom[NBMAXC];
static int nCom = 0;

static int analyseCom(char *b);
static void freeWords(void);
static void addCom(char *name, int (*f)(int, char **));
static int execComInt(int argc, char **argv);
static int execComExt(char **argv);
static int Exit(int argc, char **argv);
static int Help(int argc, char **argv);
static int Cd(int argc, char **argv);
static int Pwd(int argc, char **argv);
static int Vers(int argc, char **argv);

static int analyseCom(char *b){
    char *copy, *token, *aux, *sep = " \t\n";

    nWords = 0;
    words = NULL;
    copy = strdup(b); //4.4 remplace copyString par strdup() 
    aux = copy;

    while((token = strsep(&aux, sep)) != NULL){
        if(*token == '\0') continue;
        
        words = realloc(words, (nWords + 2)*sizeof(char*));

        if(words == NULL){
            perror("Error reallocating memory for words array");
            free(copy);
            exit(EXIT_FAILURE);
        }
        words[nWords++] = strdup(token); //4.4 remplace copyString par strdup()
    }

    if(words != NULL){
        words[nWords] = NULL; //NULL for execvp
    }
    
    free(copy);
    return nWords;          
}

/*
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
*/

static void freeWords(void){
    int i;
    for(i=0; i<nWords; i++){
        free(words[i]);
    }
    free(words);
    nWords = 0;
    words = NULL;
}

static void addCom(char *name, int (*f)(int, char **)){
    if(nCom >= NBMAXC){
        fprintf(stderr, "Error: Maximum number of commands reached (NBMAXC = %d).\n", NBMAXC);
        exit(EXIT_FAILURE);
    }
    tabCom[nCom].name = name;
    tabCom[nCom].f = f;
    nCom++;
}

void updateComInt(char *bicepsVersion){
    shell_version = bicepsVersion;
    addCom("exit", Exit);
    addCom("help", Help);
    addCom("cd", Cd);
    addCom("pwd", Pwd);
    addCom("vers", Vers);
}

void listComInt(void){
    int i;
    printf("Internal Commands (%d):\n", nCom);
    for(i=0; i<nCom; i++){
        printf("  %s\n", tabCom[i].name);
    }
}

static int execComInt(int argc, char **argv){
    int i;
    for(i=0; i<nCom; i++){
        if(strcmp(argv[0], tabCom[i].name) == 0){
            tabCom[i].f(argc, argv);
            return 1;
        }
    }
    return 0;
}

static int execComExt(char **argv){
    pid_t pid;
    int status;

    pid = fork();

    if (pid < 0){
        perror("Error forking process");
        return -1;
    }else if(pid == 0){ //Child process
        #ifdef TRACE
            printf("[TRACE] child pid=%d executing: %s\n", getpid(), argv[0]);
        #endif
        execvp(argv[0], argv);
        fprintf(stderr, "%s: command not found\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //Parent process
    #ifdef TRACE
        printf("[TRACE] parent pid=%d waiting for child pid=%d\n", getpid(), pid);
    #endif
    waitpid(pid, &status, 0);
    #ifdef TRACE
        printf("[TRACE] child exited with status %d\n", WEXITSTATUS(status));
    #endif
    return WEXITSTATUS(status);
}

void execLine(char *line){
    char *copy, *cmd, *aux;

    copy = strdup(line); //4.4 remplace copyString par strdup()
    aux = copy;

    while((cmd = strsep(&aux, ";")) != NULL){
        #ifdef TRACE
            printf("[TRACE] execLine: sub-command: '%s'\n", cmd);
        #endif

        if(analyseCom(cmd) > 0){
            if(!execComInt(nWords, words)){
                execComExt(words);
            }
            freeWords();
        }
    }
    free(copy);
}

char *getHistoryPath(void){
    char *home, *path;
    int len;

    home = getenv("HOME");
    if(home == NULL){
        fprintf(stderr, "Error: HOME environment variable not set.\n");
        exit(EXIT_FAILURE);
    }

    //home + '/' + HISTORY_FILE + '\0'
    len = strlen(home) + 1 + strlen(HISTORY_FILE) + 1;
    path = malloc(len);
    if(path == NULL){
        perror("Error allocating memory for history path");
        exit(EXIT_FAILURE);
    }

    snprintf(path, len, "%s/%s", home, HISTORY_FILE);

    return path;
}

static int Exit(int argc, char **argv){
    char *historyPath;
    
    (void)argc;
    (void)argv;

    historyPath = getHistoryPath();
    write_history(historyPath);
    free(historyPath);
    printf("Exiting biceps shell. Goodbye!\n");
    exit(0);
}

static int Help(int argc, char **argv){
    (void)argc;
    (void)argv;
    listComInt();
    return 0;
}

static int Cd(int argc, char **argv){
    char *dir;
    
    if(argc < 2){
        dir = getenv("HOME");
        if(dir == NULL){
            fprintf(stderr, "cd: HOME environment variable not set.\n");
            return 1;
        }
    }else{
        dir = argv[1];
    }

    if(chdir(dir) != 0){
        perror("cd");
        return 1;
    }
    return 0;
}

static int Pwd(int argc, char **argv){
    char *cwd;
    (void)argc;
    (void)argv;

    cwd = getcwd(NULL, 0);
    if(cwd == NULL){
        perror("pwd");
        return 1;
    }

    printf("%s\n", cwd);
    free(cwd);
    return 0;
}

static int Vers(int argc, char **argv){
    (void)argc;
    (void)argv;
    printf("gescom library version v%s\n", GESCOM_VERSION);
    printf("biceps shell version v%s\n", shell_version);
    return 0;
}