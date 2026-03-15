/*****
* Serveur BEUIP (UDP)
* - Broadcast de presence au demarrage
* - Reception des messages BEUIP
* - Table (IP + pseudo) sans doublons
* - Accuse de reception pour les messages code '1'
*****/

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#define PORT 9998
#define LBUF 512
#define MAX_PEERS 255
#define MAX_PSEUDO_LEN 128
#define BEUIP_TAG "BEUIP"
#define CODE_BROADCAST '1'
#define CODE_ACK '2'
#define CODE_LIST '3'
#define CODE_TO_PSEUDO '4'
#define CODE_TO_ALL '5'
#define CODE_LEAVE '0'
#define CODE_TEXT '9'
#define BROADCAST_IP "192.168.88.255"

struct peer_entry {
    unsigned int ip; /* host byte order */
    char pseudo[MAX_PSEUDO_LEN];
};

static struct peer_entry peers[MAX_PEERS];
static int nPeers = 0;
static char *addrip(unsigned long A);
static volatile sig_atomic_t stopRequested = 0;

static void onSignal(int sig){
    (void)sig;
    stopRequested = 1;
}

static void printPeerList(void){
    int i;

    printf("[CODE 3] PEERS ONLINE: %d\n", nPeers);
    if(nPeers == 0){
        printf("[CODE 3] LIST: (empty)\n");
        return;
    }

    for(i = 0; i < nPeers; i++){
        printf("[PEER %02d] %-16s IP: %s\n", i + 1, peers[i].pseudo, addrip(peers[i].ip));
    }
}

static char *addrip(unsigned long A){
    static char b[16];
    sprintf(b, "%u.%u.%u.%u",
            (unsigned int)(A >> 24 & 0xFF),
            (unsigned int)(A >> 16 & 0xFF),
            (unsigned int)(A >> 8 & 0xFF),
            (unsigned int)(A & 0xFF));
    return b;
}

static int isValidCode(char code){
    return (code == CODE_LEAVE || code == CODE_BROADCAST || code == CODE_ACK ||
            code == CODE_LIST || code == CODE_TO_PSEUDO || code == CODE_TO_ALL ||
            code == CODE_TEXT);
}

static int parseHeader(const char *msg, int n, char *code, const char **payload, int *payloadLen){
    if(n < 6) return 0; /* code + BEUIP(5) */
    if(!isValidCode(msg[0])) return 0;
    if(strncmp(msg + 1, BEUIP_TAG, 5) != 0) return 0;

    *code = msg[0];
    *payload = msg + 6;
    *payloadLen = n - 6;
    return 1;
}

static int copyPayloadString(const char *payload, int payloadLen, char *out, size_t outSize){
    if(payloadLen <= 0) return 0;
    if((size_t)payloadLen >= outSize){
        payloadLen = (int)outSize - 1;
    }
    memcpy(out, payload, payloadLen);
    out[payloadLen] = '\0';
    return 1;
}

static int parseToPseudoPayload(const char *payload, int payloadLen,
                                char *pseudoDest, size_t pseudoDestSize,
                                char *text, size_t textSize){
    const char *sep;
    int pseudoLen;
    int textLen;

    if(payloadLen < 3) return 0; /* a\0b */
    sep = memchr(payload, '\0', payloadLen);
    if(sep == NULL) return 0;

    pseudoLen = (int)(sep - payload);
    textLen = payloadLen - pseudoLen - 1;
    if(pseudoLen <= 0 || textLen <= 0) return 0;

    if((size_t)pseudoLen >= pseudoDestSize){
        pseudoLen = (int)pseudoDestSize - 1;
    }
    if((size_t)textLen >= textSize){
        textLen = (int)textSize - 1;
    }

    memcpy(pseudoDest, payload, pseudoLen);
    pseudoDest[pseudoLen] = '\0';
    memcpy(text, sep + 1, textLen);
    text[textLen] = '\0';

    return 1;
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

static int findPeer(unsigned int ipHostOrder, const char *pseudo){
    int i;
    for(i = 0; i < nPeers; i++){
        if(peers[i].ip == ipHostOrder && strcmp(peers[i].pseudo, pseudo) == 0){
            return i;
        }
    }
    return -1;
}

static int findPeerByPseudo(const char *pseudo){
    int i;
    for(i = 0; i < nPeers; i++){
        if(strcmp(peers[i].pseudo, pseudo) == 0){
            return i;
        }
    }
    return -1;
}

static int findPeerByIp(unsigned int ipHostOrder){
    int i;
    for(i = 0; i < nPeers; i++){
        if(peers[i].ip == ipHostOrder){
            return i;
        }
    }
    return -1;
}

static void removePeer(unsigned int ipHostOrder, const char *pseudo){
    int idx;
    int i;

    idx = findPeer(ipHostOrder, pseudo);
    if(idx < 0){
        return;
    }

    for(i = idx; i < nPeers - 1; i++){
        peers[i] = peers[i + 1];
    }
    nPeers--;

    #ifdef TRACE
    printf("[TRACE] - peer %s (%s), total=%d\n", pseudo, addrip(ipHostOrder), nPeers);
    #endif
}

static void addPeer(unsigned int ipHostOrder, const char *pseudo){
    if(findPeer(ipHostOrder, pseudo) >= 0){
        return;
    }

    if(nPeers >= MAX_PEERS){
        fprintf(stderr, "Table peers pleine (%d). Ignore %s/%s\n",
                MAX_PEERS, addrip(ipHostOrder), pseudo);
        return;
    }

    peers[nPeers].ip = ipHostOrder;
    strncpy(peers[nPeers].pseudo, pseudo, MAX_PSEUDO_LEN - 1);
    peers[nPeers].pseudo[MAX_PSEUDO_LEN - 1] = '\0';
    nPeers++;

    #ifdef TRACE
    printf("[TRACE] + peer %s (%s), total=%d\n", pseudo, addrip(ipHostOrder), nPeers);
    #endif
}

int main(int N, char *P[]){
    int sid, n, yes = 1;
    struct timeval rcvTimeout;
    char buf[LBUF + 1];
    char code;
    char pseudo[MAX_PSEUDO_LEN];
    char pseudoDest[MAX_PSEUDO_LEN];
    char text[LBUF + 1];
    char outMsg[LBUF + 1];
    int outLen;
    const char *payload;
    int payloadLen;
    int idx;
    struct sockaddr_in sockConf;
    struct sockaddr_in sockRemote;
    struct sockaddr_in sockBroadcast;
    struct sockaddr_in sockDest;
    struct sigaction sa;
    socklen_t ls;

    if(N != 2){
        fprintf(stderr, "Utilisation : %s pseudo\n", P[0]);
        return 1;
    }

    if(strlen(P[1]) == 0 || strlen(P[1]) >= MAX_PSEUDO_LEN){
        fprintf(stderr, "Pseudo invalide (1..%d caracteres).\n", MAX_PSEUDO_LEN - 1);
        return 1;
    }

    if((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
        perror("socket");
        return 2;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if(setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) == -1){
        perror("setsockopt SO_BROADCAST");
        return 2;
    }

    rcvTimeout.tv_sec = 1;
    rcvTimeout.tv_usec = 0;
    if(setsockopt(sid, SOL_SOCKET, SO_RCVTIMEO, &rcvTimeout, sizeof(rcvTimeout)) == -1){
        perror("setsockopt SO_RCVTIMEO");
        return 2;
    }

    memset(&sockConf, 0, sizeof(sockConf));
    sockConf.sin_family = AF_INET;
    sockConf.sin_port = htons(PORT);
    sockConf.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sid, (struct sockaddr *)&sockConf, sizeof(sockConf)) == -1){
        perror("bind");
        return 3;
    }

    printf("[SERVER] BEUIP listening on UDP %d | pseudo=%s\n", PORT, P[1]);

    memset(&sockBroadcast, 0, sizeof(sockBroadcast));
    sockBroadcast.sin_family = AF_INET;
    sockBroadcast.sin_port = htons(PORT);
    if(inet_pton(AF_INET, BROADCAST_IP, &sockBroadcast.sin_addr) != 1){
        fprintf(stderr, "Adresse broadcast invalide: %s\n", BROADCAST_IP);
        return 4;
    }

    outLen = buildBeuipMessage(CODE_BROADCAST, P[1], outMsg, sizeof(outMsg));
    if(outLen < 0){
        fprintf(stderr, "Message initial trop long\n");
        return 4;
    }

    if(sendto(sid, outMsg, outLen, MSG_CONFIRM,
              (struct sockaddr *)&sockBroadcast, sizeof(sockBroadcast)) == -1){
        perror("sendto broadcast");
    } else {
        #ifdef TRACE
        printf("[TRACE] broadcast presence envoye vers %s:%d\n", BROADCAST_IP, PORT);
        #endif
    }

    for(;;){
        unsigned int remoteIpHost;

        if(stopRequested){
            break;
        }

        ls = sizeof(sockRemote);
        n = recvfrom(sid, (void *)buf, LBUF, 0, (struct sockaddr *)&sockRemote, &ls);
        if(n == -1){
            if(errno == EINTR && stopRequested){
                break;
            }
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                continue;
            }
            perror("recvfrom");
            continue;
        }

        buf[n] = '\0';
        remoteIpHost = ntohl(sockRemote.sin_addr.s_addr);

        if(!parseHeader(buf, n, &code, &payload, &payloadLen)){
            #ifdef TRACE
            printf("[TRACE] message ignore (header invalide) depuis %s\n",
                   addrip(ntohl(sockRemote.sin_addr.s_addr)));
            #endif
            continue;
        }

        if((code == CODE_LIST || code == CODE_TO_PSEUDO || code == CODE_TO_ALL) &&
           remoteIpHost != INADDR_LOOPBACK){
            fprintf(stderr, "[CODE %c] FROM: %s | STATUS: REFUSED (non-local source)\n",
                    code, addrip(remoteIpHost));
            continue;
        }

        if(code == CODE_LEAVE){
            if(!copyPayloadString(payload, payloadLen, pseudo, sizeof(pseudo))){
                fprintf(stderr, "[CODE 0] FROM: %s | STATUS: INVALID PAYLOAD\n",
                        addrip(remoteIpHost));
                continue;
            }
            removePeer(remoteIpHost, pseudo);
            printf("[CODE 0] FROM: %s | EVENT: LEAVE | PSEUDO: %s\n", addrip(remoteIpHost), pseudo);
            continue;
        }

        if(code == CODE_LIST){
            if(payloadLen != 0){
                fprintf(stderr, "[CODE 3] FROM: %s | STATUS: INVALID PAYLOAD\n",
                        addrip(remoteIpHost));
                continue;
            }
            printf("[CODE 3] FROM: %s | EVENT: LIST REQUEST\n",
                   addrip(remoteIpHost));
            printPeerList();
            continue;
        }

        if(code == CODE_TO_PSEUDO){
            if(!parseToPseudoPayload(payload, payloadLen,
                                     pseudoDest, sizeof(pseudoDest),
                                     text, sizeof(text))){
                fprintf(stderr, "[CODE 4] FROM: %s | STATUS: INVALID PAYLOAD\n",
                    addrip(remoteIpHost));
                continue;
            }

            idx = findPeerByPseudo(pseudoDest);
            if(idx < 0){
                fprintf(stderr, "[CODE 4] FROM: %s | STATUS: DEST NOT FOUND | TO: %s\n",
                        addrip(remoteIpHost), pseudoDest);
                continue;
            }

            outLen = buildBeuipMessage(CODE_TEXT, text, outMsg, sizeof(outMsg));
            if(outLen < 0){
                fprintf(stderr, "message code=9 trop long\n");
                continue;
            }

            memset(&sockDest, 0, sizeof(sockDest));
            sockDest.sin_family = AF_INET;
            sockDest.sin_port = htons(PORT);
            sockDest.sin_addr.s_addr = htonl(peers[idx].ip);

            if(sendto(sid, outMsg, outLen, MSG_CONFIRM,
                      (struct sockaddr *)&sockDest, sizeof(sockDest)) == -1){
                perror("sendto code=9");
                continue;
            }

            printf("[CODE 4] FROM: %s | TO: %s | MSG: %s\n",
                   addrip(remoteIpHost), pseudoDest, text);
            continue;
        }

        if(code == CODE_TO_ALL){
            int i;
            if(!copyPayloadString(payload, payloadLen, text, sizeof(text))){
                fprintf(stderr, "[CODE 5] FROM: %s | STATUS: INVALID PAYLOAD\n", addrip(remoteIpHost));
                continue;
            }

            outLen = buildBeuipMessage(CODE_TEXT, text, outMsg, sizeof(outMsg));
            if(outLen < 0){
                fprintf(stderr, "message code=9 trop long\n");
                continue;
            }

            for(i = 0; i < nPeers; i++){
                if(strcmp(peers[i].pseudo, P[1]) == 0) continue;

                memset(&sockDest, 0, sizeof(sockDest));
                sockDest.sin_family = AF_INET;
                sockDest.sin_port = htons(PORT);
                sockDest.sin_addr.s_addr = htonl(peers[i].ip);

                if(sendto(sid, outMsg, outLen, MSG_CONFIRM,
                          (struct sockaddr *)&sockDest, sizeof(sockDest)) == -1){
                    perror("sendto code=9 (broadcast msg)");
                }
            }

            printf("[CODE 5] FROM: %s | TO: ALL | MSG: %s\n", addrip(remoteIpHost), text);
            continue;
        }

        if(code == CODE_TEXT){
            if(!copyPayloadString(payload, payloadLen, text, sizeof(text))){
                fprintf(stderr, "[CODE 9] FROM: %s | STATUS: INVALID PAYLOAD\n",
                        addrip(remoteIpHost));
                continue;
            }

            idx = findPeerByIp(remoteIpHost);
            if(idx < 0){
                fprintf(stderr, "[CODE 9] FROM: %s | STATUS: UNKNOWN SOURCE | MSG: %s\n",
                        addrip(remoteIpHost), text);
                continue;
            }

            printf("[CODE 9] FROM: %s (%s) | MSG: %s\n",
                   peers[idx].pseudo, addrip(remoteIpHost), text);
            continue;
        }

        if(!copyPayloadString(payload, payloadLen, pseudo, sizeof(pseudo))){
            fprintf(stderr, "[CODE %c] FROM: %s | STATUS: INVALID PAYLOAD\n",
                  code, addrip(remoteIpHost));
            continue;
        }

         addPeer(remoteIpHost, pseudo);
        printf("[CODE %c] FROM: %s | PSEUDO: %s\n",
             code, addrip(remoteIpHost), pseudo);

        if(code == CODE_BROADCAST){
            outLen = buildBeuipMessage(CODE_ACK, P[1], outMsg, sizeof(outMsg));
            if(outLen < 0){
                fprintf(stderr, "ACK non envoye: message trop long\n");
                continue;
            }

            if(sendto(sid, outMsg, outLen, MSG_CONFIRM,
                      (struct sockaddr *)&sockRemote, ls) == -1){
                perror("sendto ack");
            }
        }
    }

    outLen = buildBeuipMessage(CODE_LEAVE, P[1], outMsg, sizeof(outMsg));
    if(outLen >= 0){
        sendto(sid, outMsg, outLen, MSG_CONFIRM,
               (struct sockaddr *)&sockBroadcast, sizeof(sockBroadcast));
    }

    close(sid);
    printf("[SERVER] Shutdown complete | code=0 sent\n");

    return 0;
}
