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

static int validateArgs(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <pseudo>\n", argv[0]);
        return 1;
    }
    if(strlen(argv[1]) == 0 || strlen(argv[1]) >= BEUIP_MAX_PSEUDO_LEN){
        fprintf(stderr, "Invalid pseudo (1..%d chars).\n", BEUIP_MAX_PSEUDO_LEN - 1);
        return 1;
    }
    return 0;
}

static void setupSignals(void){
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static int configureServerSocket(int sid){
    if(creme_enable_broadcast(sid) == -1){ perror("setsockopt SO_BROADCAST"); return 2; }
    if(creme_enable_recv_timeout(sid, 1) == -1){ perror("setsockopt SO_RCVTIMEO"); return 2; }
    if(creme_bind_any(sid, BEUIP_PORT) == -1){ perror("bind"); return 3; }
    return 0;
}

static int createServerSocket(void){
    int sid;

    sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sid < 0){
        perror("socket");
        return -1;
    }
    return sid;
}

static int prepareBroadcastAddr(struct sockaddr_in *sockBroadcast){
    if(!creme_prepare_ipv4_addr(sockBroadcast, BEUIP_BROADCAST_IP, BEUIP_PORT)){
        fprintf(stderr, "Invalid broadcast address: %s\n", BEUIP_BROADCAST_IP);
        return 4;
    }
    return 0;
}

static void sendInitialPresence(int sid, const struct sockaddr_in *sockBroadcast, const char *pseudo){
    if(creme_send_presence(sid, sockBroadcast, pseudo) == -1){
        fprintf(stderr, "Initial message too long\n");
        perror("sendto broadcast");
        return;
    }
    #ifdef TRACE
    printf("[TRACE] broadcast presence sent to %s:%d\n", BEUIP_BROADCAST_IP, BEUIP_PORT);
    #endif
}

static int recvServerDatagram(int sid, char *buf, struct sockaddr_in *sockRemote, socklen_t *ls){
    *ls = sizeof(*sockRemote);
    return recvfrom(sid, (void *)buf, BEUIP_LBUF, 0, (struct sockaddr *)sockRemote, ls);
}

static int shouldStopServerLoop(int n){
    if(n != -1) return 0;
    if(errno == EINTR && stopRequested) return 1;
    if(errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    perror("recvfrom");
    return 0;
}

static void runServerLoop(int sid, const char *pseudo, creme_peer_table *peers){
    int n;
    char buf[BEUIP_LBUF + 1];
    struct sockaddr_in sockRemote;
    socklen_t ls;

    while(!stopRequested){
        n = recvServerDatagram(sid, buf, &sockRemote, &ls);
        if(shouldStopServerLoop(n)) break;
        if(n == -1) continue;
        buf[n] = '\0';
        creme_handle_server_datagram(sid, pseudo, peers, &sockRemote, ls, buf, n);
    }
}

static int initServer(struct sockaddr_in *sockBroadcast, creme_peer_table *peers){
    int rc;
    int sid;

    creme_init_peer_table(peers);
    sid = createServerSocket();
    if(sid < 0) return -1;
    setupSignals();
    rc = configureServerSocket(sid);
    if(rc != 0){ close(sid); return -rc; }
    rc = prepareBroadcastAddr(sockBroadcast);
    if(rc != 0){ close(sid); return -rc; }
    return sid;
}

static int exitCodeFromInit(int sid){
    return sid < 0 ? -sid : sid;
}

static void shutdownServer(int sid, const struct sockaddr_in *sockBroadcast, const char *pseudo){
    creme_send_leave(sid, sockBroadcast, pseudo);
    close(sid);
    printf("[SERVER] Shutdown complete | code=0 sent\n");
}

int main(int N, char *P[]){
    int sid;
    creme_peer_table peers;
    struct sockaddr_in sockBroadcast;
    if(validateArgs(N, P) != 0) return 1;
    sid = initServer(&sockBroadcast, &peers);
    if(sid < 0) return exitCodeFromInit(sid);
    printf("[SERVER] BEUIP listening on UDP %d | pseudo=%s\n", BEUIP_PORT, P[1]);
    sendInitialPresence(sid, &sockBroadcast, P[1]);
    runServerLoop(sid, P[1], &peers);
    shutdownServer(sid, &sockBroadcast, P[1]);
    return 0;
}