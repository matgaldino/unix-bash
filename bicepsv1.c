#define _GNU_SOURCE
#include<stdio.h>
#include<readline/readline.h>
#include<readline/history.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include<sys/wait.h>

#define VERSION "1.00"
#define NBMAXC 10

char *buildPrompt(void);
void intHandler(int s);
int analyseCom(char *b);
char *copyString(char *s);
void freeWords(void);
void addCom(char *name, int (*f)(int, char **));
void updateComInt(void);
void listComInt(void);
int execComInt(int argc, char **argv);
int execComExt(char **argv);
void execLine(char *line);

/* Internal Commands */
int Help(int argc, char **argv);
int Exit(int argc, char **argv);
int Cd(int argc, char **argv);
int Pwd(int argc, char **argv);
int Vers(int argc, char **argv);

static char **words;
static int nWords;

typedef struct{
    char *name;
    int (*f)(int, char **);
} comInt;

static comInt tabCom[NBMAXC];
static int nCom = 0;

int main(int argc, char *argv[]){    
    char *prompt;
    char *line;

    signal(SIGINT, intHandler);

    updateComInt();
    listComInt();

    prompt = buildPrompt();

    while(1){
        line = readline(prompt);

        if(line == NULL){
            printf("\n");
            break;
        }

        if(*line != '\0'){
            add_history(line);
            execLine(line);
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
        
        words = realloc(words, (nWords + 2)*sizeof(char*));

        if(words == NULL){
            perror("Error reallocating memory for words array");
            free(copy);
            exit(EXIT_FAILURE);
        }
        words[nWords++] = copyString(token);
    }

    if(words != NULL){
        words[nWords] = NULL; //NULL for execvp
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

void addCom(char *name, int (*f)(int, char **)){
    if(nCom >= NBMAXC){
        fprintf(stderr, "Error: Maximum number of commands reached (NBMAXC = %d).\n", NBMAXC);
        exit(EXIT_FAILURE);
    }
    tabCom[nCom].name = name;
    tabCom[nCom].f = f;
    nCom++;
}


void updateComInt(void){
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

int execComInt(int argc, char **argv){
    int i;
    for(i=0; i<nCom; i++){
        if(strcmp(argv[0], tabCom[i].name) == 0){
            tabCom[i].f(argc, argv);
            return 1;
        }
    }
    return 0;
}

int execComExt(char **argv){
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

    copy = copyString(line);
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

int Exit(int argc, char **argv){
    (void)argc;
    (void)argv;
    exit(0);
}

int Help(int argc, char **argv){
    (void)argc;
    (void)argv;
    listComInt();
    return 0;
}

int Cd(int argc, char **argv){
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

int Pwd(int argc, char **argv){
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

int Vers(int argc, char **argv){
    (void)argc;
    (void)argv;
    printf("biceps shell version v%s\n", VERSION);
    return 0;
}