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

#include "creme.h"

#define SERVER_IP "127.0.0.1"

int main(int N, char *P[]){
    int sid, n;
    char inMsg[BEUIP_LBUF + 1];
    char recvCode;
    char recvPseudo[BEUIP_MAX_PSEUDO_LEN];
    char code;
    struct sockaddr_in sockServer;
    struct sockaddr_in sockFrom;
    socklen_t lFrom;

    if(N == 2 && strcmp(P[1], "3") == 0){
        code = BEUIP_CODE_LIST;

    } else if(N == 4 && strcmp(P[1], "4") == 0){
        if(strlen(P[2]) == 0 || strlen(P[2]) >= BEUIP_MAX_PSEUDO_LEN){
            fprintf(stderr, "[CLIENT] ERROR: invalid destination pseudo (1..%d chars).\n", BEUIP_MAX_PSEUDO_LEN - 1);
            return 1;
        }
        if(strlen(P[3]) == 0){
            fprintf(stderr, "[CLIENT] ERROR: empty message is not allowed.\n");
            return 1;
        }
        code = BEUIP_CODE_TO_PSEUDO;

    } else if(N == 3 && strcmp(P[1], "5") == 0){
        if(strlen(P[2]) == 0){
            fprintf(stderr, "[CLIENT] ERROR: empty message is not allowed.\n");
            return 1;
        }
        code = BEUIP_CODE_TO_ALL;

    } else if(N == 3 && strcmp(P[1], "0") == 0){
        if(strlen(P[2]) == 0 || strlen(P[2]) >= BEUIP_MAX_PSEUDO_LEN){
            fprintf(stderr, "[CLIENT] ERROR: invalid pseudo (1..%d chars).\n", BEUIP_MAX_PSEUDO_LEN - 1);
            return 1;
        }
        code = BEUIP_CODE_LEAVE;

    } else if(N == 2 || N == 3){
        code = BEUIP_CODE_BROADCAST;
        if(strlen(P[1]) == 0 || strlen(P[1]) >= BEUIP_MAX_PSEUDO_LEN){
            fprintf(stderr, "[CLIENT] ERROR: invalid pseudo (1..%d chars).\n", BEUIP_MAX_PSEUDO_LEN - 1);
            return 1;
        }

        if(N == 3){
            if(strlen(P[2]) != 1 || !creme_is_valid_code(P[2][0]) ||
               P[2][0] == BEUIP_CODE_LIST || P[2][0] == BEUIP_CODE_TO_PSEUDO){
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

    if(!creme_prepare_ipv4_addr(&sockServer, SERVER_IP, BEUIP_PORT)){
        fprintf(stderr, "[CLIENT] ERROR: invalid server address: %s\n", SERVER_IP);
        return 3;
    }

    if(code == BEUIP_CODE_TO_PSEUDO){
        if(creme_send_private_message(sid, &sockServer, P[2], P[3]) == -1){
            fprintf(stderr, "[CLIENT] ERROR: message too long\n");
            return 4;
        }
    } else if(code == BEUIP_CODE_TO_ALL){
        if(creme_send_broadcast_text(sid, &sockServer, P[2]) == -1){
            fprintf(stderr, "[CLIENT] ERROR: message too long\n");
            return 4;
        }
    } else if(code == BEUIP_CODE_LIST){
        if(creme_send_list_request(sid, &sockServer) == -1){
            fprintf(stderr, "[CLIENT] ERROR: message too long\n");
            return 4;
        }
    } else if(code == BEUIP_CODE_LEAVE){
        if(creme_send_leave(sid, &sockServer, P[2]) == -1){
            fprintf(stderr, "[CLIENT] ERROR: message too long\n");
            return 4;
        }
    } else {
        if(creme_send_presence(sid, &sockServer, P[1]) == -1){
            fprintf(stderr, "[CLIENT] ERROR: message too long\n");
            return 4;
        }
    }

    if(code == BEUIP_CODE_TO_PSEUDO){
        printf("[CODE 4] TO: %s:%d | DEST: %s | MSG: %s\n", SERVER_IP, BEUIP_PORT, P[2], P[3]);
    } else if(code == BEUIP_CODE_TO_ALL){
        printf("[CODE 5] TO: %s:%d | DEST: ALL | MSG: %s\n", SERVER_IP, BEUIP_PORT, P[2]);
    } else if(code == BEUIP_CODE_LIST){
        printf("[CODE 3] TO: %s:%d | EVENT: LIST REQUEST\n", SERVER_IP, BEUIP_PORT);
    } else if(code == BEUIP_CODE_LEAVE){
        printf("[CODE 0] TO: %s:%d | EVENT: LEAVE | PSEUDO: %s\n", SERVER_IP, BEUIP_PORT, P[2]);
    } else {
        printf("[CODE %c] TO: %s:%d | PSEUDO: %s\n", code, SERVER_IP, BEUIP_PORT, P[1]);
    }

    if(code == BEUIP_CODE_LIST){
        printf("[CODE 3] STATUS: SENT | Server prints list locally\n");
        return 0;
    }

    if(code == BEUIP_CODE_TO_PSEUDO){
        printf("[CODE 4] STATUS: SENT\n");
        return 0;
    }

    if(code == BEUIP_CODE_TO_ALL){
        printf("[CODE 5] STATUS: SENT\n");
        return 0;
    }

    if(code == BEUIP_CODE_LEAVE){
        printf("[CODE 0] STATUS: SENT\n");
        return 0;
    }

    lFrom = sizeof(sockFrom);
    n = recvfrom(sid, (void *)inMsg, BEUIP_LBUF, 0, (struct sockaddr *)&sockFrom, &lFrom);
    if(n == -1){
        perror("recvfrom");
        return 6;
    }

    inMsg[n] = '\0';
    if(!creme_parse_reply(inMsg, n, &recvCode, recvPseudo, sizeof(recvPseudo))){
        printf("[CLIENT] RX: non-BEUIP payload: <%s>\n", inMsg);
        return 0;
    }

    printf("[CODE %c] FROM: %s:%d | PSEUDO: %s\n", recvCode,
           inet_ntoa(sockFrom.sin_addr), ntohs(sockFrom.sin_port), recvPseudo);
    return 0;
}
