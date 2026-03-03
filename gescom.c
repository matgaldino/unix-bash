    #define _GNU_SOURCE
    #include<stdio.h>
    #include<stdlib.h>
    #include<unistd.h>
    #include<string.h>
    #include<sys/wait.h>
    #include<readline/history.h>
    #include<fcntl.h>
    #include "gescom.h"

    #define GESCOM_VERSION "1.2"
    static char *shell_version = "unknown";

    static char **words;
    static int nWords;

    static comInt tabCom[NBMAXC];
    static int nCom = 0;

    static int analyseCom(char *b);
    static void freeWords(void);
    static void addCom(char *name, int (*f)(int, char **));
    static int execComInt(int argc, char **argv);
    static int execComExt(char **argv);
    static void execPipe(char *line);
    static void applyRedirections(void);

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
            applyRedirections();
            execvp(words[0], words);
            fprintf(stderr, "%s: command not found\n", words[0]);
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
        if(copy == NULL){
            perror("Error duplicating line for command execution");
            exit(EXIT_FAILURE);
        }
        aux = copy;

        while((cmd = strsep(&aux, ";")) != NULL){
            #ifdef TRACE
                printf("[TRACE] execLine: sub-command: '%s'\n", cmd);
            #endif

            if(strchr(cmd, '|') != NULL){
                execPipe(cmd);
            }else{
                if(analyseCom(cmd) > 0){
                    if(!execComInt(nWords, words)){
                        execComExt(words);
                    }
                    freeWords();
                }
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

    static void execPipe(char *line){
        char *commands[NBMAXC];
        int nCommands = 0, i;
        char *copy, *cmd, *aux;
        int pipes[NBMAXC][2];
        pid_t pids[NBMAXC];
        
        copy = strdup(line); //4.4 remplace copyString par strdup()
        if(copy == NULL){
            perror("Error duplicating line for pipe execution");
            exit(EXIT_FAILURE);
        }
        aux = copy;

        while((cmd = strsep(&aux, "|")) != NULL){
            if(*cmd == '\0') continue;
            if(nCommands >= NBMAXC){
                fprintf(stderr, "Error: Too many commands in pipeline (max %d).\n", NBMAXC);
                free(copy);
                exit(EXIT_FAILURE);
            }
            commands[nCommands++] = cmd;
        }

        if(nCommands == 0){
            free(copy);
            return;
        }

        for(i=0; i<nCommands-1; i++){
            if(pipe(pipes[i]) < 0){
                perror("Error creating pipe");
                int j;
                for(j=0; j<i; j++){
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                free(copy);
                return;
            }
        }

        for(i=0; i<nCommands; i++){
            pids[i] = -1;

            pids[i] = fork();
            if(pids[i] < 0){
                perror("Error forking process");
                int j;
                for(j=0; j<nCommands-1; j++){
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                free(copy);
                return;
            }

            if(pids[i] == 0){
                if(i > 0){
                    if(dup2(pipes[i-1][0], STDIN_FILENO) < 0){
                        perror("dup2 stdin");
                        exit(EXIT_FAILURE);
                    }
                }

                if(i < nCommands - 1){
                    if(dup2(pipes[i][1], STDOUT_FILENO) < 0){
                        perror("dup2 stdout");
                        exit(EXIT_FAILURE);
                    }
                }

                int j;
                for(j=0; j<nCommands-1; j++){
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                if(analyseCom(commands[i]) == 0){
                    exit(0);
                }
                applyRedirections();

                #ifdef TRACE
                    printf("[TRACE] pipe: filho %d executando: %s\n", i, words[0]);
                #endif

                if(!execComInt(nWords, words)){
                    execvp(words[0], words);
                    fprintf(stderr, "%s: command not found\n", words[0]);
                    exit(EXIT_FAILURE);
                }
                exit(0);
            }
        }

        
        for(i=0; i<nCommands-1; i++){
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

        for(i=0; i<nCommands; i++){
            if(pids[i] == -1) continue;
            int status;
            waitpid(pids[i], &status, 0);
            #ifdef TRACE
                printf("[TRACE] pipe: filho %d terminou com status %d\n", i, WEXITSTATUS(status));
            #endif
        }

        free(copy);
    }

    static void applyRedirections(void){
        char **filtered;
        int newWords = 0, fd, i;

        filtered = malloc((nWords + 1) * sizeof(char *));
        if(filtered == NULL){
            perror("Error allocating filtered words");
            exit(EXIT_FAILURE);
        }

        i = 0;
        while(i < nWords){

            if(strcmp(words[i], "<") == 0){
                if(i + 1 >= nWords){ fprintf(stderr, "missing file after '<'\n");
                    exit(EXIT_FAILURE);
                }
                fd = open(words[i+1], O_RDONLY);
                if(fd < 0){
                    perror(words[i+1]);
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
                free(words[i]);
                free(words[i+1]);
                i += 2;

            }else if(strcmp(words[i], "<<") == 0){ 
                if(i + 1 >= nWords){
                    fprintf(stderr, "missing delimiter after '<<'\n");
                    exit(EXIT_FAILURE);
                }
                int hpipe[2];
                char *delim = words[i+1];
                char *hline = NULL;
                size_t hlen = 0;
                if(pipe(hpipe) < 0){
                    perror("pipe heredoc");
                    exit(EXIT_FAILURE);
                }
                while(1){
                    printf("> ");
                    fflush(stdout);
                    if(getline(&hline, &hlen, stdin) < 0) break;
                    hline[strcspn(hline, "\n")] = '\0';
                    if(strcmp(hline, delim) == 0) break;
                    write(hpipe[1], hline, strlen(hline));
                    write(hpipe[1], "\n", 1);
                }
                free(hline);
                close(hpipe[1]);
                dup2(hpipe[0], STDIN_FILENO);
                close(hpipe[0]);
                free(words[i]);
                free(words[i+1]);
                i += 2;

            }else if(strcmp(words[i], ">") == 0){
                if(i + 1 >= nWords){
                    fprintf(stderr, "missing file after '>'\n");
                    exit(EXIT_FAILURE);
                }
                fd = open(words[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(fd < 0){ perror(words[i+1]); exit(EXIT_FAILURE); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                free(words[i]); 
                free(words[i+1]);
                i += 2;
            }else if(strcmp(words[i], ">>") == 0){
                if(i + 1 >= nWords){
                    fprintf(stderr, "missing file after '>>'\n");
                    exit(EXIT_FAILURE);
                }
                fd = open(words[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if(fd < 0){ perror(words[i+1]); exit(EXIT_FAILURE); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                free(words[i]);
                free(words[i+1]);
                i += 2;
            }else if(strcmp(words[i], "2>") == 0){
                if(i + 1 >= nWords){
                    fprintf(stderr, "missing file after '2>'\n");
                    exit(EXIT_FAILURE);
                }
                fd = open(words[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(fd < 0){ perror(words[i+1]); exit(EXIT_FAILURE); }
                dup2(fd, STDERR_FILENO);
                close(fd);
                free(words[i]);
                free(words[i+1]);
                i += 2;
            }else if(strcmp(words[i], "2>>") == 0){
                if(i + 1 >= nWords){
                    fprintf(stderr, "missing file after '2>>'\n");
                    exit(EXIT_FAILURE);
                }
                fd = open(words[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if(fd < 0){ perror(words[i+1]); exit(EXIT_FAILURE); }
                dup2(fd, STDERR_FILENO);
                close(fd);
                free(words[i]);
                free(words[i+1]);
                i += 2;
            }else{
                filtered[newWords++] = words[i];
                i++;
            }
        }

        filtered[newWords] = NULL;
        free(words);
        words = filtered;
        nWords = newWords;
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