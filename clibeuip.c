/*****
* Client BEUIP (tests)
* Envoie un datagramme BEUIP au serveur local 127.0.0.1:9998
* et attend la reponse (si le code demande un ACK).
*****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9998
#define LBUF 512
#define MAX_PSEUDO_LEN 128
#define BEUIP_TAG "BEUIP"
#define CODE_BROADCAST '1'
#define CODE_ACK '2'
#define CODE_LIST '3'
#define CODE_TO_PSEUDO '4'
#define CODE_TO_ALL '5'
#define CODE_LEAVE '0'
#define CODE_TEXT '9'

static int isValidCode(char code){
    return (code == CODE_LEAVE || code == CODE_BROADCAST || code == CODE_ACK ||
            code == CODE_LIST || code == CODE_TO_PSEUDO || code == CODE_TO_ALL ||
            code == CODE_TEXT);
}

static int buildBeuipMessage(char code, const char *pseudo, char *out, size_t outSize){
    int needed;

    needed = 1 + 5;
    if(code != CODE_LIST){
        needed += (int)strlen(pseudo);
    }
    if((size_t)needed >= outSize){
        return -1;
    }

    out[0] = code;
    memcpy(out + 1, BEUIP_TAG, 5);
    if(code != CODE_LIST){
        memcpy(out + 6, pseudo, strlen(pseudo));
    }
    out[needed] = '\0';

    return needed;
}

static int buildToPseudoMessage(const char *pseudoDest, const char *text, char *out, size_t outSize){
    int needed;

    needed = 1 + 5 + (int)strlen(pseudoDest) + 1 + (int)strlen(text);
    if((size_t)needed >= outSize){
        return -1;
    }

    out[0] = CODE_TO_PSEUDO;
    memcpy(out + 1, BEUIP_TAG, 5);
    memcpy(out + 6, pseudoDest, strlen(pseudoDest));
    out[6 + strlen(pseudoDest)] = '\0';
    memcpy(out + 7 + strlen(pseudoDest), text, strlen(text));
    out[needed] = '\0';
    return needed;
}

static int parseBeuipMessage(const char *msg, int n, char *code, char *pseudo, size_t pseudoSize){
    int pseudoLen;

    if(n < 6) return 0;
    if(!isValidCode(msg[0])) return 0;
    if(strncmp(msg + 1, BEUIP_TAG, 5) != 0) return 0;

    if(msg[0] == CODE_LIST){
        if(n != 6) return 0;
        pseudo[0] = '\0';
        *code = msg[0];
        return 1;
    }

    pseudoLen = n - 6;
    if(pseudoLen <= 0) return 0;
    if((size_t)pseudoLen >= pseudoSize){
        pseudoLen = (int)pseudoSize - 1;
    }

    memcpy(pseudo, msg + 6, pseudoLen);
    pseudo[pseudoLen] = '\0';
    *code = msg[0];

    return 1;
}

int main(int N, char *P[]){
    int sid, n;
    char outMsg[LBUF + 1];
    char inMsg[LBUF + 1];
    char recvCode;
    char recvPseudo[MAX_PSEUDO_LEN];
    int outLen;
    char code;
    int messageMode = 0;
    struct sockaddr_in sockServer;
    struct sockaddr_in sockFrom;
    socklen_t lFrom;

    if(N == 2 && strcmp(P[1], "3") == 0){
        code = CODE_LIST;

    } else if(N == 4 && strcmp(P[1], "4") == 0){
        if(strlen(P[2]) == 0 || strlen(P[2]) >= MAX_PSEUDO_LEN){
            fprintf(stderr, "[CLIENT] ERROR: invalid destination pseudo (1..%d chars).\n", MAX_PSEUDO_LEN - 1);
            return 1;
        }
        if(strlen(P[3]) == 0){
            fprintf(stderr, "[CLIENT] ERROR: empty message is not allowed.\n");
            return 1;
        }
        messageMode = 1;
        code = CODE_TO_PSEUDO;

    } else if(N == 3 && strcmp(P[1], "5") == 0){
        if(strlen(P[2]) == 0){
            fprintf(stderr, "[CLIENT] ERROR: empty message is not allowed.\n");
            return 1;
        }
        code = CODE_TO_ALL;

    } else if(N == 3 && strcmp(P[1], "0") == 0){
        if(strlen(P[2]) == 0 || strlen(P[2]) >= MAX_PSEUDO_LEN){
            fprintf(stderr, "[CLIENT] ERROR: invalid pseudo (1..%d chars).\n", MAX_PSEUDO_LEN - 1);
            return 1;
        }
        code = CODE_LEAVE;

    } else if(N == 2 || N == 3){
        code = CODE_BROADCAST;
        if(strlen(P[1]) == 0 || strlen(P[1]) >= MAX_PSEUDO_LEN){
            fprintf(stderr, "[CLIENT] ERROR: invalid pseudo (1..%d chars).\n", MAX_PSEUDO_LEN - 1);
            return 1;
        }

        if(N == 3){
            if(strlen(P[2]) != 1 || !isValidCode(P[2][0]) || P[2][0] == CODE_LIST || P[2][0] == CODE_TO_PSEUDO){
                fprintf(stderr, "[CLIENT] ERROR: invalid code. Use 0,1,2,9. List: %s 3 | Private: %s 4 <pseudo> <msg> | All: %s 5 <msg>\n", P[0], P[0], P[0]);
                return 1;
            }
            code = P[2][0];
        }

    } else {
        fprintf(stderr, "[CLIENT] Usage:\n");
        fprintf(stderr, "  %s <pseudo> [0|1|2|9]\n", P[0]);
        fprintf(stderr, "  %s 3\n", P[0]);
        fprintf(stderr, "  %s 4 <pseudo> <message>\n", P[0]);
        fprintf(stderr, "  %s 5 <message>\n", P[0]);
        return 1;
    }

    if((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
        perror("socket");
        return 2;
    }

    bzero(&sockServer, sizeof(sockServer));
    sockServer.sin_family = AF_INET;
    sockServer.sin_port = htons(PORT);
    if(inet_aton(SERVER_IP, &sockServer.sin_addr) == 0){
        fprintf(stderr, "[CLIENT] ERROR: invalid server address: %s\n", SERVER_IP);
        return 3;
    }

    if(messageMode){
        outLen = buildToPseudoMessage(P[2], P[3], outMsg, sizeof(outMsg));
    } else if(code == CODE_TO_ALL){
        outLen = buildBeuipMessage(code, P[2], outMsg, sizeof(outMsg));
    } else if(code == CODE_LEAVE){
        outLen = buildBeuipMessage(code, P[2], outMsg, sizeof(outMsg));
    } else {
        outLen = buildBeuipMessage(code, (code == CODE_LIST) ? "" : P[1], outMsg, sizeof(outMsg));
    }
    if(outLen < 0){
        fprintf(stderr, "[CLIENT] ERROR: message too long\n");
        return 4;
    }

    if(sendto(sid, outMsg, outLen, 0, (struct sockaddr *)&sockServer, sizeof(sockServer)) == -1){
        perror("sendto");
        return 5;
    }

    if(code == CODE_TO_PSEUDO){
        printf("[CODE 4] TO: %s:%d | DEST: %s | MSG: %s\n", SERVER_IP, PORT, P[2], P[3]);
    } else if(code == CODE_TO_ALL){
        printf("[CODE 5] TO: %s:%d | DEST: ALL | MSG: %s\n", SERVER_IP, PORT, P[2]);
    } else if(code == CODE_LIST){
        printf("[CODE 3] TO: %s:%d | EVENT: LIST REQUEST\n", SERVER_IP, PORT);
    } else if(code == CODE_LEAVE){
        printf("[CODE 0] TO: %s:%d | EVENT: LEAVE | PSEUDO: %s\n", SERVER_IP, PORT, P[2]);
    } else {
        printf("[CODE %c] TO: %s:%d | PSEUDO: %s\n", code, SERVER_IP, PORT, P[1]);
    }

    if(code == CODE_LIST){
        printf("[CODE 3] STATUS: SENT | Server prints list locally\n");
        return 0;
    }

    if(code == CODE_TO_PSEUDO){
        printf("[CODE 4] STATUS: SENT\n");
        return 0;
    }

    if(code == CODE_TO_ALL){
        printf("[CODE 5] STATUS: SENT\n");
        return 0;
    }

    if(code == CODE_LEAVE){
        printf("[CODE 0] STATUS: SENT\n");
        return 0;
    }

    lFrom = sizeof(sockFrom);
    n = recvfrom(sid, (void *)inMsg, LBUF, 0, (struct sockaddr *)&sockFrom, &lFrom);
    if(n == -1){
        perror("recvfrom");
        return 6;
    }

    inMsg[n] = '\0';
    if(!parseBeuipMessage(inMsg, n, &recvCode, recvPseudo, sizeof(recvPseudo))){
        printf("[CLIENT] RX: non-BEUIP payload: <%s>\n", inMsg);
        return 0;
    }

    printf("[CODE %c] FROM: %s:%d | PSEUDO: %s\n", recvCode,
           inet_ntoa(sockFrom.sin_addr), ntohs(sockFrom.sin_port), recvPseudo);
    return 0;
}
