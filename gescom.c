#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include<signal.h>
#include<readline/history.h>
#include<fcntl.h>
#include<errno.h>
#include<pthread.h>
#include "gescom.h"
#include "creme.h"

#define GESCOM_VERSION "1.3"
static char *shell_version = "unknown";

static char **words;
static int nWords;
static pthread_t g_beuipThread;
static volatile int g_beuipRunning = 0;
static volatile int g_stopUdp = 0;
static char g_pseudo[BEUIP_MAX_PSEUDO_LEN];
static creme_peer_table g_peers;
static pthread_mutex_t g_peers_mutex = PTHREAD_MUTEX_INITIALIZER;

static comInt tabCom[NBMAXC];
static int nCom = 0;

typedef struct {
    const char *token;
    void (*handler)(int);
} redirection_rule;

static int analyseCom(char *b);
static char *dupLineOrExit(const char *src, const char *context);
static void addTokenWord(const char *token);
static void freeWords(void);
static void addCom(char *name, int (*f)(int, char **));
static int execComInt(int argc, char **argv);
static int execComExt(char **argv);
static void runExternalChild(char **argv);
static int waitExternalChild(pid_t pid);
static void execSimpleCommand(char *cmd);
static void execPipe(char *line);
static int splitPipeCommands(char *copy, char **commands);
static int createPipeArray(int pipes[][2], int nCommands);
static void closePipeArray(int pipes[][2], int nPipes);
static void runPipeChild(int idx, int nCommands, int pipes[][2], char *cmd);
static int forkPipeChildren(int nCommands, int pipes[][2], pid_t pids[], char **commands);
static void waitPipeChildren(pid_t pids[], int nCommands);
static void applyRedirections(void);
static void freeRedirectionTokens(int i);
static void ensureRedirectionOperand(int i, const char *op, const char *what);
static void redirectInputFromFile(int i);
static void redirectInputFromHeredoc(int i);
static void readHeredocIntoPipe(int writeFd, const char *delim);
static void connectPipeToStdin(int readFd);
static void redirectStdoutToFile(int i);
static void redirectStdoutAppendToFile(int i);
static void redirectStderrToFile(int i);
static void redirectStderrAppendToFile(int i);
static int findRedirectionHandler(const char *token);
static int applyOneRedirection(int i);
static void keepFilteredWord(char **filtered, int *newWords, int idx);

static int Exit(int argc, char **argv);
static int Help(int argc, char **argv);
static int Cd(int argc, char **argv);
static int Pwd(int argc, char **argv);
static int Vers(int argc, char **argv);
static int Beuip(int argc, char **argv);
static int udpSetupSocket(struct sockaddr_in *sockBroadcast);
static int isServerCode(char code);
static void udpHandleDatagram(int sid, const char *pseudo, char *buf, int n, struct sockaddr_in *sockRemote, socklen_t ls);
static void udpRunLoop(int sid, const char *pseudo);
static void *serveur_udp(void *p);
static int sendTextToIp(unsigned int ipHost, const char *text);
static void commandeList(void);
static void commandeToPseudo(const char *pseudo, const char *message);
static void commandeAll(const char *message);
static void commande(char octet1, char *message, char *pseudo);
static int handleBeuipStop(int argc);
static int validateBeuipStartArgs(int argc, char **argv);
static int spawnBeuipServer(const char *pseudo);
static int stopBeuipServer(void);
static int Mess(int argc, char **argv);
static int messUsage(void);
static int messListCommand(int argc);
static int messToCommand(int argc, char **argv);
static int messAllCommand(int argc, char **argv);
static char *joinArgs(int start, int argc, char **argv);
static size_t joinedArgsLength(int start, int argc, char **argv);
static void appendJoinedArgs(char *msg, int start, int argc, char **argv);

static char *dupLineOrExit(const char *src, const char *context){
    char *copy;

    copy = strdup(src);
    if(copy == NULL){
        perror(context);
        exit(EXIT_FAILURE);
    }
    return copy;
}

static void addTokenWord(const char *token){
    char **newWords;

    newWords = realloc(words, (nWords + 2) * sizeof(char *));
    if(newWords == NULL){
        perror("Error reallocating memory for words array");
        exit(EXIT_FAILURE);
    }
    words = newWords;
    words[nWords++] = strdup(token);
    if(words[nWords - 1] == NULL){
        perror("Error duplicating command token");
        exit(EXIT_FAILURE);
    }
}

static int analyseCom(char *b){
    char *copy, *token, *aux;

    nWords = 0;
    words = NULL;
    copy = dupLineOrExit(b, "Error duplicating command line");
    aux = copy;
    while((token = strsep(&aux, " \t\n")) != NULL){
        if(*token != '\0'){
            addTokenWord(token);
        }
    }
    if(words != NULL){
        words[nWords] = NULL;
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
    addCom("beuip", Beuip);
    addCom("mess", Mess);
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

static void runExternalChild(char **argv){
    (void)argv;
    #ifdef TRACE
        printf("[TRACE] child pid=%d executing: %s\n", getpid(), words[0]);
    #endif
    applyRedirections();
    execvp(words[0], words);
    fprintf(stderr, "%s: command not found\n", words[0]);
    exit(EXIT_FAILURE);
}

static int waitExternalChild(pid_t pid){
    int status;

    #ifdef TRACE
        printf("[TRACE] parent pid=%d waiting for child pid=%d\n", getpid(), pid);
    #endif
    waitpid(pid, &status, 0);
    #ifdef TRACE
        printf("[TRACE] child exited with status %d\n", WEXITSTATUS(status));
    #endif
    return WEXITSTATUS(status);
}

static int execComExt(char **argv){
    pid_t pid;

    (void)argv;
    pid = fork();
    if (pid < 0){
        perror("Error forking process");
        return -1;
    }
    if(pid == 0){
        runExternalChild(argv);
    }
    return waitExternalChild(pid);
}

static void execSimpleCommand(char *cmd){
    if(analyseCom(cmd) > 0){
        if(!execComInt(nWords, words)){
            execComExt(words);
        }
        freeWords();
    }
}

void execLine(char *line){
    char *copy, *cmd, *aux;

    copy = dupLineOrExit(line, "Error duplicating line for command execution");
    aux = copy;
    while((cmd = strsep(&aux, ";")) != NULL){
        #ifdef TRACE
            printf("[TRACE] execLine: sub-command: '%s'\n", cmd);
        #endif
        if(strchr(cmd, '|') != NULL){
            execPipe(cmd);
        }else{
            execSimpleCommand(cmd);
        }
    }
    free(copy);
}

char *getHistoryPath(void){
    const char *home;
    char *path;
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

static int splitPipeCommands(char *copy, char **commands){
    char *aux, *cmd;
    int nCommands;

    aux = copy;
    nCommands = 0;
    while((cmd = strsep(&aux, "|")) != NULL){
        if(*cmd == '\0') continue;
        if(nCommands >= NBMAXC){
            fprintf(stderr, "Error: Too many commands in pipeline (max %d).\n", NBMAXC);
            exit(EXIT_FAILURE);
        }
        commands[nCommands++] = cmd;
    }
    return nCommands;
}

static void closePipeArray(int pipes[][2], int nPipes){
    int i;

    for(i = 0; i < nPipes; i++){
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static int createPipeArray(int pipes[][2], int nCommands){
    int i;

    for(i = 0; i < nCommands - 1; i++){
        if(pipe(pipes[i]) < 0){
            perror("Error creating pipe");
            closePipeArray(pipes, i);
            return -1;
        }
    }
    return 0;
}

static void runPipeChild(int idx, int nCommands, int pipes[][2], char *cmd){
    if(idx > 0 && dup2(pipes[idx - 1][0], STDIN_FILENO) < 0){ perror("dup2 stdin"); exit(EXIT_FAILURE); }
    if(idx < nCommands - 1 && dup2(pipes[idx][1], STDOUT_FILENO) < 0){ perror("dup2 stdout"); exit(EXIT_FAILURE); }
    closePipeArray(pipes, nCommands - 1);
    if(analyseCom(cmd) == 0) exit(0);
    applyRedirections();
    #ifdef TRACE
        printf("[TRACE] pipe: filho %d executando: %s\n", idx, words[0]);
    #endif
    if(!execComInt(nWords, words)) execvp(words[0], words);
    fprintf(stderr, "%s: command not found\n", words[0]);
    exit(EXIT_FAILURE);
}

static int forkPipeChildren(int nCommands, int pipes[][2], pid_t pids[], char **commands){
    int i;

    for(i = 0; i < nCommands; i++){
        pids[i] = fork();
        if(pids[i] < 0){
            perror("Error forking process");
            return -1;
        }
        if(pids[i] == 0) runPipeChild(i, nCommands, pipes, commands[i]);
    }
    return 0;
}

static void waitPipeChildren(pid_t pids[], int nCommands){
    int i;

    for(i = 0; i < nCommands; i++){
        int status;

        if(pids[i] == -1) continue;
        waitpid(pids[i], &status, 0);
        #ifdef TRACE
            printf("[TRACE] pipe: filho %d terminou com status %d\n", i, WEXITSTATUS(status));
        #endif
    }
}

static void execPipe(char *line){
    char *commands[NBMAXC];
    int pipes[NBMAXC][2];
    pid_t pids[NBMAXC];
    int nCommands;
    char *copy;

    copy = dupLineOrExit(line, "Error duplicating line for pipe execution");
    nCommands = splitPipeCommands(copy, commands);
    if(nCommands == 0){ free(copy); return; }
    if(createPipeArray(pipes, nCommands) == -1){ free(copy); return; }
    if(forkPipeChildren(nCommands, pipes, pids, commands) == -1){ closePipeArray(pipes, nCommands - 1); free(copy); return; }
    closePipeArray(pipes, nCommands - 1);
    waitPipeChildren(pids, nCommands);
    free(copy);
}

static void freeRedirectionTokens(int i){
    free(words[i]);
    free(words[i+1]);
}

static void ensureRedirectionOperand(int i, const char *op, const char *what){
    if(i + 1 >= nWords){
        fprintf(stderr, "missing %s after '%s'\n", what, op);
        exit(EXIT_FAILURE);
    }
}

static void redirectInputFromFile(int i){
    int fd;

    ensureRedirectionOperand(i, "<", "file");
    fd = open(words[i+1], O_RDONLY);
    if(fd < 0){
        perror(words[i+1]);
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, STDIN_FILENO) < 0){
        perror("dup2 stdin");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    freeRedirectionTokens(i);
}

static void readHeredocIntoPipe(int writeFd, const char *delim){
    char *hline;
    size_t hlen;

    hline = NULL;
    hlen = 0;
    while(1){
        printf("> ");
        fflush(stdout);
        if(getline(&hline, &hlen, stdin) < 0) break;
        hline[strcspn(hline, "\n")] = '\0';
        if(strcmp(hline, delim) == 0) break;
        write(writeFd, hline, strlen(hline));
        write(writeFd, "\n", 1);
    }
    free(hline);
}

static void connectPipeToStdin(int readFd){
    if(dup2(readFd, STDIN_FILENO) < 0){
        perror("dup2 stdin");
        close(readFd);
        exit(EXIT_FAILURE);
    }
    close(readFd);
}

static void redirectInputFromHeredoc(int i){
    int hpipe[2];

    ensureRedirectionOperand(i, "<<", "delimiter");
    if(pipe(hpipe) < 0){
        perror("pipe heredoc");
        exit(EXIT_FAILURE);
    }
    readHeredocIntoPipe(hpipe[1], words[i + 1]);
    close(hpipe[1]);
    connectPipeToStdin(hpipe[0]);
    freeRedirectionTokens(i);
}

static void redirectStdoutToFile(int i){
    int fd;

    ensureRedirectionOperand(i, ">", "file");
    fd = open(words[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0){
        perror(words[i+1]);
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, STDOUT_FILENO) < 0){
        perror("dup2 stdout");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    freeRedirectionTokens(i);
}

static void redirectStdoutAppendToFile(int i){
    int fd;

    ensureRedirectionOperand(i, ">>", "file");
    fd = open(words[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd < 0){
        perror(words[i+1]);
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, STDOUT_FILENO) < 0){
        perror("dup2 stdout");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    freeRedirectionTokens(i);
}

static void redirectStderrToFile(int i){
    int fd;

    ensureRedirectionOperand(i, "2>", "file");
    fd = open(words[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0){
        perror(words[i+1]);
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, STDERR_FILENO) < 0){
        perror("dup2 stderr");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    freeRedirectionTokens(i);
}

static void redirectStderrAppendToFile(int i){
    int fd;

    ensureRedirectionOperand(i, "2>>", "file");
    fd = open(words[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd < 0){
        perror(words[i+1]);
        exit(EXIT_FAILURE);
    }
    if(dup2(fd, STDERR_FILENO) < 0){
        perror("dup2 stderr");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    freeRedirectionTokens(i);
}

static int findRedirectionHandler(const char *token){
    static const redirection_rule rules[] = {
        {"<", redirectInputFromFile}, {"<<", redirectInputFromHeredoc},
        {">", redirectStdoutToFile}, {">>", redirectStdoutAppendToFile},
        {"2>", redirectStderrToFile}, {"2>>", redirectStderrAppendToFile}
    };
    int idx;

    for(idx = 0; idx < (int)(sizeof(rules) / sizeof(rules[0])); idx++){
        if(strcmp(token, rules[idx].token) == 0) return idx;
    }
    return -1;
}

static int applyOneRedirection(int i){
    static const redirection_rule rules[] = {
        {"<", redirectInputFromFile}, {"<<", redirectInputFromHeredoc},
        {">", redirectStdoutToFile}, {">>", redirectStdoutAppendToFile},
        {"2>", redirectStderrToFile}, {"2>>", redirectStderrAppendToFile}
    };
    int idx;

    idx = findRedirectionHandler(words[i]);
    if(idx < 0) return 0;
    rules[idx].handler(i);
    return 1;
}

static void keepFilteredWord(char **filtered, int *newWords, int idx){
    filtered[*newWords] = words[idx];
    (*newWords)++;
}

static void applyRedirections(void){
    char **filtered;
    int i;
    int newWords;

    filtered = malloc((nWords + 1) * sizeof(char *));
    if(filtered == NULL){ perror("Error allocating filtered words"); exit(EXIT_FAILURE); }
    i = 0;
    newWords = 0;
    while(i < nWords){
        if(applyOneRedirection(i)) i += 2;
        else { keepFilteredWord(filtered, &newWords, i); i++; }
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

    if(g_beuipRunning){
        stopBeuipServer();
    }

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
    printf("creme library version v%s\n", creme_version());
    printf("biceps shell version v%s\n", shell_version);
    return 0;
}

static int udpSetupSocket(struct sockaddr_in *sockBroadcast){
    int sid;

    sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sid < 0){ perror("serveur_udp: socket"); return -1; }
    if(creme_enable_broadcast(sid) == -1){ perror("serveur_udp: broadcast"); close(sid); return -1; }
    if(creme_enable_recv_timeout(sid, 1) == -1){ perror("serveur_udp: timeout"); close(sid); return -1; }
    if(creme_bind_any(sid, BEUIP_PORT) == -1){ perror("serveur_udp: bind"); close(sid); return -1; }
    if(!creme_prepare_ipv4_addr(sockBroadcast, BEUIP_BROADCAST_IP, BEUIP_PORT)){ close(sid); return -1; }
    return sid;
}

static int isServerCode(char code){
    return code == BEUIP_CODE_LEAVE || code == BEUIP_CODE_BROADCAST ||
           code == BEUIP_CODE_ACK  || code == BEUIP_CODE_TEXT;
}

static void udpHandleDatagram(int sid, const char *pseudo, char *buf, int n,
                               struct sockaddr_in *sockRemote, socklen_t ls){
    if(!isServerCode(buf[0])){
        fprintf(stderr, "[SECURITY] Rejected code '%c'\n", buf[0]);
        return;
    }
    pthread_mutex_lock(&g_peers_mutex);
    creme_handle_server_datagram(sid, pseudo, &g_peers, sockRemote, ls, buf, n);
    pthread_mutex_unlock(&g_peers_mutex);
}

static void udpRunLoop(int sid, const char *pseudo){
    char buf[BEUIP_LBUF + 1];
    struct sockaddr_in sockRemote;
    socklen_t ls;
    int n;

    while(!g_stopUdp){
        ls = sizeof(sockRemote);
        n = recvfrom(sid, buf, BEUIP_LBUF, 0, (struct sockaddr *)&sockRemote, &ls);
        if(n < 0) continue;
        buf[n] = '\0';
        udpHandleDatagram(sid, pseudo, buf, n, &sockRemote, ls);
    }
}

static void *serveur_udp(void *p){
    const char *pseudo = (const char *)p;
    struct sockaddr_in sockBroadcast;
    int sid;

    creme_init_peer_table(&g_peers);
    sid = udpSetupSocket(&sockBroadcast);
    if(sid < 0){ g_beuipRunning = 0; return NULL; }
    printf("[SERVER] BEUIP thread listening on UDP %d | pseudo=%s\n", BEUIP_PORT, pseudo);
    creme_send_presence(sid, &sockBroadcast, pseudo);
    udpRunLoop(sid, pseudo);
    creme_send_leave(sid, &sockBroadcast, pseudo);
    close(sid);
    printf("[SERVER] BEUIP thread stopped\n");
    g_beuipRunning = 0;
    return NULL;
}

static int handleBeuipStop(int argc){
    if(argc != 2){
        fprintf(stderr, "Usage: beuip stop\n");
        return 1;
    }
    if(!g_beuipRunning){
        fprintf(stderr, "beuip: no server running.\n");
        return 1;
    }
    return stopBeuipServer();
}

static int validateBeuipStartArgs(int argc, char **argv){
    if(strcmp(argv[1], "start") != 0 || argc != 3) return 1;
    if(g_beuipRunning){
        fprintf(stderr, "beuip: server already running\n");
        return 1;
    }
    if(strlen(argv[2]) == 0 || strlen(argv[2]) >= BEUIP_MAX_PSEUDO_LEN){
        fprintf(stderr, "beuip: invalid pseudo (1..%d chars).\n", BEUIP_MAX_PSEUDO_LEN - 1);
        return 1;
    }
    return 0;
}

static int spawnBeuipServer(const char *pseudo){
    strncpy(g_pseudo, pseudo, BEUIP_MAX_PSEUDO_LEN - 1);
    g_pseudo[BEUIP_MAX_PSEUDO_LEN - 1] = '\0';
    g_stopUdp = 0;
    g_beuipRunning = 1;
    if(pthread_create(&g_beuipThread, NULL, serveur_udp, g_pseudo) != 0){
        perror("beuip: pthread_create");
        g_beuipRunning = 0;
        return 1;
    }
    printf("beuip: server thread started | pseudo=%s\n", pseudo);
    return 0;
}

static int Beuip(int argc, char **argv){
    if(argc < 2){
        fprintf(stderr, "Usage: beuip start <pseudo> | beuip stop\n");
        return 1;
    }
    if(strcmp(argv[1], "stop") == 0){
        return handleBeuipStop(argc);
    }
    if(validateBeuipStartArgs(argc, argv) != 0){
        fprintf(stderr, "Usage: beuip start <pseudo> | beuip stop\n");
        return 1;
    }
    return spawnBeuipServer(argv[2]);
}

static int stopBeuipServer(void){
    if(!g_beuipRunning) return 0;
    g_stopUdp = 1;
    pthread_join(g_beuipThread, NULL);
    printf("beuip: server thread stopped\n");
    return 0;
}

static int sendTextToIp(unsigned int ipHost, const char *text){
    struct sockaddr_in dest;
    char msg[BEUIP_LBUF + 1];
    int sid, len;

    sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sid < 0){ perror("commande: socket"); return -1; }
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(BEUIP_PORT);
    dest.sin_addr.s_addr = htonl(ipHost);
    len = creme_build_message(BEUIP_CODE_TEXT, text, msg, sizeof(msg));
    if(len < 0){ close(sid); return -1; }
    sendto(sid, msg, len, 0, (struct sockaddr *)&dest, sizeof(dest));
    close(sid);
    return 0;
}

static void commandeList(void){
    pthread_mutex_lock(&g_peers_mutex);
    creme_print_peer_list(&g_peers);
    pthread_mutex_unlock(&g_peers_mutex);
}

static void commandeToPseudo(const char *pseudo, const char *message){
    int idx;
    unsigned int ip;

    pthread_mutex_lock(&g_peers_mutex);
    idx = creme_find_peer_by_pseudo(&g_peers, pseudo);
    if(idx >= 0) ip = g_peers.entries[idx].ip;
    pthread_mutex_unlock(&g_peers_mutex);
    if(idx < 0){ fprintf(stderr, "mess: pseudo '%s' not found\n", pseudo); return; }
    sendTextToIp(ip, message);
}

static void commandeAll(const char *message){
    unsigned int ips[BEUIP_MAX_PEERS];
    int i, count;

    pthread_mutex_lock(&g_peers_mutex);
    count = g_peers.count;
    for(i = 0; i < count; i++) ips[i] = g_peers.entries[i].ip;
    pthread_mutex_unlock(&g_peers_mutex);
    for(i = 0; i < count; i++) sendTextToIp(ips[i], message);
}

static void commande(char octet1, char *message, char *pseudo){
    if(octet1 == BEUIP_CODE_LIST){ commandeList(); return; }
    if(octet1 == BEUIP_CODE_TO_PSEUDO){ commandeToPseudo(pseudo, message); return; }
    if(octet1 == BEUIP_CODE_TO_ALL){ commandeAll(message); return; }
    fprintf(stderr, "commande: unknown code '%c'\n", octet1);
}

static size_t joinedArgsLength(int start, int argc, char **argv){
    int i;
    size_t total;

    total = 1;
    for(i = start; i < argc; i++){
        total += strlen(argv[i]);
        if(i + 1 < argc) total += 1;
    }
    return total;
}

static void appendJoinedArgs(char *msg, int start, int argc, char **argv){
    int i;

    msg[0] = '\0';
    for(i = start; i < argc; i++){
        strcat(msg, argv[i]);
        if(i + 1 < argc) strcat(msg, " ");
    }
}

static char *joinArgs(int start, int argc, char **argv){
    char *msg;
    size_t total;

    total = joinedArgsLength(start, argc, argv);
    msg = malloc(total);
    if(msg == NULL){
        perror("mess: malloc");
        return NULL;
    }
    appendJoinedArgs(msg, start, argc, argv);
    return msg;
}

static int messUsage(void){
    fprintf(stderr, "Usage: mess list | mess to <pseudo> <message> | mess all <message>\n");
    return 1;
}

static int messListCommand(int argc){
    if(argc != 2){ fprintf(stderr, "Usage: mess list\n"); return 1; }
    commande(BEUIP_CODE_LIST, NULL, NULL);
    return 0;
}

static int messToCommand(int argc, char **argv){
    char *msg;

    if(argc < 4){ fprintf(stderr, "Usage: mess to <pseudo> <message>\n"); return 1; }
    if(strlen(argv[2]) == 0 || strlen(argv[2]) >= BEUIP_MAX_PSEUDO_LEN){
        fprintf(stderr, "mess: invalid pseudo (1..%d chars)\n", BEUIP_MAX_PSEUDO_LEN - 1);
        return 1;
    }
    msg = joinArgs(3, argc, argv);
    if(msg == NULL) return 1;
    commande(BEUIP_CODE_TO_PSEUDO, msg, argv[2]);
    free(msg);
    return 0;
}

static int messAllCommand(int argc, char **argv){
    char *msg;

    if(argc < 3){ fprintf(stderr, "Usage: mess all <message>\n"); return 1; }
    msg = joinArgs(2, argc, argv);
    if(msg == NULL) return 1;
    commande(BEUIP_CODE_TO_ALL, msg, NULL);
    free(msg);
    return 0;
}

static int Mess(int argc, char **argv){
    if(argc < 2) return messUsage();
    if(strcmp(argv[1], "list") == 0) return messListCommand(argc);
    if(strcmp(argv[1], "to") == 0) return messToCommand(argc, argv);
    if(strcmp(argv[1], "all") == 0) return messAllCommand(argc, argv);
    return messUsage();
}