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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include "creme.h"

static volatile sig_atomic_t stopRequested = 0;

static void onSignal(int sig){
    (void)sig;
    stopRequested = 1;
}

int main(int N, char *P[]){
    int sid, n;
    char buf[BEUIP_LBUF + 1];
    creme_peer_table peers;
    struct sockaddr_in sockRemote;
    struct sockaddr_in sockBroadcast;
    struct sigaction sa;
    socklen_t ls;

    if(N != 2){
        fprintf(stderr, "Usage: %s <pseudo>\n", P[0]);
        return 1;
    }

    if(strlen(P[1]) == 0 || strlen(P[1]) >= BEUIP_MAX_PSEUDO_LEN){
        fprintf(stderr, "Invalid pseudo (1..%d chars).\n", BEUIP_MAX_PSEUDO_LEN - 1);
        return 1;
    }

    creme_init_peer_table(&peers);

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

    if(creme_enable_broadcast(sid) == -1){
        perror("setsockopt SO_BROADCAST");
        close(sid);
        return 2;
    }

    if(creme_enable_recv_timeout(sid, 1) == -1){
        perror("setsockopt SO_RCVTIMEO");
        close(sid);
        return 2;
    }

    if(creme_bind_any(sid, BEUIP_PORT) == -1){
        perror("bind");
        close(sid);
        return 3;
    }

    printf("[SERVER] BEUIP listening on UDP %d | pseudo=%s\n", BEUIP_PORT, P[1]);

    if(!creme_prepare_ipv4_addr(&sockBroadcast, BEUIP_BROADCAST_IP, BEUIP_PORT)){
        fprintf(stderr, "Invalid broadcast address: %s\n", BEUIP_BROADCAST_IP);
        close(sid);
        return 4;
    }

    if(creme_send_presence(sid, &sockBroadcast, P[1]) == -1){
        fprintf(stderr, "Initial message too long\n");
        perror("sendto broadcast");
    } else {
        #ifdef TRACE
        printf("[TRACE] broadcast presence sent to %s:%d\n", BEUIP_BROADCAST_IP, BEUIP_PORT);
        #endif
    }

    for(;;){
        if(stopRequested){
            break;
        }

        ls = sizeof(sockRemote);
        n = recvfrom(sid, (void *)buf, BEUIP_LBUF, 0, (struct sockaddr *)&sockRemote, &ls);
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
        creme_handle_server_datagram(sid, P[1], &peers, &sockRemote, ls, buf, n);
    }

    creme_send_leave(sid, &sockBroadcast, P[1]);

    close(sid);
    printf("[SERVER] Shutdown complete | code=0 sent\n");

    return 0;
}