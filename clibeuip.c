/*****
* Client BEUIP (tests)
* Envoie un datagramme BEUIP au serveur local 127.0.0.1:9998
* et attend la reponse (si le code demande un ACK).
*****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "creme.h"

#define SERVER_IP "127.0.0.1"

static int printUsage(const char *prog){
    fprintf(stderr, "[CLIENT] Usage:\n");
    fprintf(stderr, "  %s <pseudo> [0|1|2|9]\n", prog);
    fprintf(stderr, "  %s 3\n", prog);
    fprintf(stderr, "  %s 4 <pseudo> <message>\n", prog);
    fprintf(stderr, "  %s 5 <message>\n", prog);
    return 1;
}

static int invalidPseudoError(const char *label){
    fprintf(stderr, "[CLIENT] ERROR: invalid %s (1..%d chars).\n", label, BEUIP_MAX_PSEUDO_LEN - 1);
    return 1;
}

static int emptyMessageError(void){
    fprintf(stderr, "[CLIENT] ERROR: empty message is not allowed.\n");
    return 1;
}

static int invalidCodeError(const char *prog){
    fprintf(stderr, "[CLIENT] ERROR: invalid code. Use 0,1,2,9. List: %s 3 | Private: %s 4 <pseudo> <msg> | All: %s 5 <msg>\n", prog, prog, prog);
    return 1;
}

static int isValidPseudo(const char *pseudo){
    return strlen(pseudo) > 0 && strlen(pseudo) < BEUIP_MAX_PSEUDO_LEN;
}

static int parseBroadcastArgs(int argc, char **argv, char *code){
    *code = BEUIP_CODE_BROADCAST;
    if(!isValidPseudo(argv[1])) return invalidPseudoError("pseudo");
    if(argc == 2) return 0;
    if(strlen(argv[2]) != 1 || !creme_is_valid_code(argv[2][0]) ||
       argv[2][0] == BEUIP_CODE_LIST || argv[2][0] == BEUIP_CODE_TO_PSEUDO){
        return invalidCodeError(argv[0]);
    }
    *code = argv[2][0];
    return 0;
}

static int parseSpecialClientArgs(int argc, char **argv, char *code){
    if(argc == 2 && strcmp(argv[1], "3") == 0){ *code = BEUIP_CODE_LIST; return 0; }
    if(argc == 4 && strcmp(argv[1], "4") == 0){
        if(!isValidPseudo(argv[2])) return invalidPseudoError("destination pseudo");
        if(strlen(argv[3]) == 0) return emptyMessageError();
        *code = BEUIP_CODE_TO_PSEUDO;
        return 0;
    }
    return -1;
}

static int parseClientArgs(int argc, char **argv, char *code){
    int rc;

    rc = parseSpecialClientArgs(argc, argv, code);
    if(rc != -1) return rc;
    if(argc == 3 && strcmp(argv[1], "5") == 0){
        if(strlen(argv[2]) == 0) return emptyMessageError();
        *code = BEUIP_CODE_TO_ALL;
        return 0;
    }
    if(argc == 3 && strcmp(argv[1], "0") == 0){
        if(!isValidPseudo(argv[2])) return invalidPseudoError("pseudo");
        *code = BEUIP_CODE_LEAVE;
        return 0;
    }
    if(argc == 2 || argc == 3) return parseBroadcastArgs(argc, argv, code);
    return printUsage(argv[0]);
}

static int createClientSocket(void){
    int sid;

    sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sid < 0){
        perror("socket");
        return -1;
    }
    return sid;
}

static int initServerAddr(struct sockaddr_in *sockServer){
    if(!creme_prepare_ipv4_addr(sockServer, SERVER_IP, BEUIP_PORT)){
        fprintf(stderr, "[CLIENT] ERROR: invalid server address: %s\n", SERVER_IP);
        return 3;
    }
    return 0;
}

static int sendClientMessage(int sid, char code, char **argv, const struct sockaddr_in *sockServer){
    int rc;

    if(code == BEUIP_CODE_TO_PSEUDO) rc = creme_send_private_message(sid, sockServer, argv[2], argv[3]);
    else if(code == BEUIP_CODE_TO_ALL) rc = creme_send_broadcast_text(sid, sockServer, argv[2]);
    else if(code == BEUIP_CODE_LIST) rc = creme_send_list_request(sid, sockServer);
    else if(code == BEUIP_CODE_LEAVE) rc = creme_send_leave(sid, sockServer, argv[2]);
    else rc = creme_send_presence(sid, sockServer, argv[1]);
    if(rc == -1) fprintf(stderr, "[CLIENT] ERROR: message too long\n");
    return rc == -1 ? 4 : 0;
}

static void printSendSummary(char code, char **argv){
    if(code == BEUIP_CODE_TO_PSEUDO) printf("[CODE 4] TO: %s:%d | DEST: %s | MSG: %s\n", SERVER_IP, BEUIP_PORT, argv[2], argv[3]);
    else if(code == BEUIP_CODE_TO_ALL) printf("[CODE 5] TO: %s:%d | DEST: ALL | MSG: %s\n", SERVER_IP, BEUIP_PORT, argv[2]);
    else if(code == BEUIP_CODE_LIST) printf("[CODE 3] TO: %s:%d | EVENT: LIST REQUEST\n", SERVER_IP, BEUIP_PORT);
    else if(code == BEUIP_CODE_LEAVE) printf("[CODE 0] TO: %s:%d | EVENT: LEAVE | PSEUDO: %s\n", SERVER_IP, BEUIP_PORT, argv[2]);
    else printf("[CODE %c] TO: %s:%d | PSEUDO: %s\n", code, SERVER_IP, BEUIP_PORT, argv[1]);
}

static int printImmediateStatus(char code){
    if(code == BEUIP_CODE_LIST) printf("[CODE 3] STATUS: SENT | Server prints list locally\n");
    else if(code == BEUIP_CODE_TO_PSEUDO) printf("[CODE 4] STATUS: SENT\n");
    else if(code == BEUIP_CODE_TO_ALL) printf("[CODE 5] STATUS: SENT\n");
    else if(code == BEUIP_CODE_LEAVE) printf("[CODE 0] STATUS: SENT\n");
    else return 0;
    return 1;
}

static int recvReply(int sid, char *inMsg, struct sockaddr_in *sockFrom, socklen_t *lFrom){
    *lFrom = sizeof(*sockFrom);
    return recvfrom(sid, (void *)inMsg, BEUIP_LBUF, 0, (struct sockaddr *)sockFrom, lFrom);
}

static int printReply(int sid){
    int n;
    char inMsg[BEUIP_LBUF + 1];
    char recvCode;
    char recvPseudo[BEUIP_MAX_PSEUDO_LEN];
    struct sockaddr_in sockFrom;
    socklen_t lFrom;

    n = recvReply(sid, inMsg, &sockFrom, &lFrom);
    if(n == -1){ perror("recvfrom"); return 6; }
    inMsg[n] = '\0';
    if(!creme_parse_reply(inMsg, n, &recvCode, recvPseudo, sizeof(recvPseudo))){
        printf("[CLIENT] RX: non-BEUIP payload: <%s>\n", inMsg);
        return 0;
    }
    printf("[CODE %c] FROM: %s:%d | PSEUDO: %s\n", recvCode, inet_ntoa(sockFrom.sin_addr), ntohs(sockFrom.sin_port), recvPseudo);
    return 0;
}

static int receiveAndClose(int sid){
    int rc;

    rc = printReply(sid);
    close(sid);
    return rc;
}

int main(int N, char *P[]){
    int sid;
    char code;
    struct sockaddr_in sockServer;

    if(parseClientArgs(N, P, &code) != 0) return 1;
    sid = createClientSocket();
    if(sid < 0) return 2;
    if(initServerAddr(&sockServer) != 0){ close(sid); return 3; }
    if(sendClientMessage(sid, code, P, &sockServer) != 0){ close(sid); return 4; }
    printSendSummary(code, P);
    if(printImmediateStatus(code)){ close(sid); return 0; }
    return receiveAndClose(sid);
}
