#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "creme.h"

static int creme_send_message(int sid, const struct sockaddr_in *dest, const char *msg, int len){
    return sendto(sid, msg, len, MSG_CONFIRM, (const struct sockaddr *)dest, sizeof(*dest));
}

const char *creme_version(void){
    return CREME_VERSION;
}

const char *creme_addrip(unsigned long address){
    static char buffer[16];

    snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
             (unsigned int)(address >> 24 & 0xFF),
             (unsigned int)(address >> 16 & 0xFF),
             (unsigned int)(address >> 8 & 0xFF),
             (unsigned int)(address & 0xFF));
    return buffer;
}

int creme_is_valid_code(char code){
    return (code == BEUIP_CODE_LEAVE || code == BEUIP_CODE_BROADCAST ||
            code == BEUIP_CODE_ACK || code == BEUIP_CODE_LIST ||
            code == BEUIP_CODE_TO_PSEUDO || code == BEUIP_CODE_TO_ALL ||
            code == BEUIP_CODE_TEXT);
}

int creme_parse_header(const char *msg, int n, char *code, const char **payload, int *payloadLen){
    if(n < 6) return 0;
    if(!creme_is_valid_code(msg[0])) return 0;
    if(strncmp(msg + 1, BEUIP_TAG, 5) != 0) return 0;

    *code = msg[0];
    *payload = msg + 6;
    *payloadLen = n - 6;
    return 1;
}

int creme_copy_payload_string(const char *payload, int payloadLen, char *out, size_t outSize){
    if(payloadLen <= 0) return 0;
    if((size_t)payloadLen >= outSize){
        payloadLen = (int)outSize - 1;
    }

    memcpy(out, payload, payloadLen);
    out[payloadLen] = '\0';
    return 1;
}

int creme_parse_to_pseudo_payload(const char *payload, int payloadLen,
                                  char *pseudoDest, size_t pseudoDestSize,
                                  char *text, size_t textSize){
    const char *sep;
    int pseudoLen;
    int textLen;

    if(payloadLen < 3) return 0;
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

int creme_build_message(char code, const char *pseudo, char *out, size_t outSize){
    int needed;

    needed = 1 + 5;
    if(code != BEUIP_CODE_LIST){
        needed += (int)strlen(pseudo);
    }
    if((size_t)needed >= outSize){
        return -1;
    }

    out[0] = code;
    memcpy(out + 1, BEUIP_TAG, 5);
    if(code != BEUIP_CODE_LIST){
        memcpy(out + 6, pseudo, strlen(pseudo));
    }
    out[needed] = '\0';
    return needed;
}

int creme_build_to_pseudo_message(const char *pseudoDest, const char *text, char *out, size_t outSize){
    int needed;

    needed = 1 + 5 + (int)strlen(pseudoDest) + 1 + (int)strlen(text);
    if((size_t)needed >= outSize){
        return -1;
    }

    out[0] = BEUIP_CODE_TO_PSEUDO;
    memcpy(out + 1, BEUIP_TAG, 5);
    memcpy(out + 6, pseudoDest, strlen(pseudoDest));
    out[6 + strlen(pseudoDest)] = '\0';
    memcpy(out + 7 + strlen(pseudoDest), text, strlen(text));
    out[needed] = '\0';
    return needed;
}

int creme_parse_reply(const char *msg, int n, char *code, char *pseudo, size_t pseudoSize){
    const char *payload;
    int payloadLen;

    if(!creme_parse_header(msg, n, code, &payload, &payloadLen)) return 0;
    if(*code == BEUIP_CODE_LIST){
        if(payloadLen != 0) return 0;
        pseudo[0] = '\0';
        return 1;
    }

    return creme_copy_payload_string(payload, payloadLen, pseudo, pseudoSize);
}

int creme_prepare_ipv4_addr(struct sockaddr_in *addr, const char *ip, int port){
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    return inet_pton(AF_INET, ip, &addr->sin_addr) == 1;
}

int creme_enable_broadcast(int sid){
    int yes = 1;
    return setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
}

int creme_enable_recv_timeout(int sid, int timeoutSeconds){
    struct timeval timeout;

    timeout.tv_sec = timeoutSeconds;
    timeout.tv_usec = 0;
    return setsockopt(sid, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

int creme_bind_any(int sid, int port){
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return bind(sid, (struct sockaddr *)&addr, sizeof(addr));
}

int creme_send_presence(int sid, const struct sockaddr_in *dest, const char *pseudo){
    char msg[BEUIP_LBUF + 1];
    int len;

    len = creme_build_message(BEUIP_CODE_BROADCAST, pseudo, msg, sizeof(msg));
    if(len < 0) return -1;
    return creme_send_message(sid, dest, msg, len);
}

int creme_send_leave(int sid, const struct sockaddr_in *dest, const char *pseudo){
    char msg[BEUIP_LBUF + 1];
    int len;

    len = creme_build_message(BEUIP_CODE_LEAVE, pseudo, msg, sizeof(msg));
    if(len < 0) return -1;
    return creme_send_message(sid, dest, msg, len);
}

int creme_send_list_request(int sid, const struct sockaddr_in *dest){
    char msg[BEUIP_LBUF + 1];
    int len;

    len = creme_build_message(BEUIP_CODE_LIST, "", msg, sizeof(msg));
    if(len < 0) return -1;
    return creme_send_message(sid, dest, msg, len);
}

int creme_send_private_message(int sid, const struct sockaddr_in *dest, const char *pseudoDest, const char *text){
    char msg[BEUIP_LBUF + 1];
    int len;

    len = creme_build_to_pseudo_message(pseudoDest, text, msg, sizeof(msg));
    if(len < 0) return -1;
    return creme_send_message(sid, dest, msg, len);
}

int creme_send_broadcast_text(int sid, const struct sockaddr_in *dest, const char *text){
    char msg[BEUIP_LBUF + 1];
    int len;

    len = creme_build_message(BEUIP_CODE_TO_ALL, text, msg, sizeof(msg));
    if(len < 0) return -1;
    return creme_send_message(sid, dest, msg, len);
}

int creme_handle_server_datagram(int sid, const char *selfPseudo, creme_peer_table *table,
                                 const struct sockaddr_in *remote, socklen_t remoteLen,
                                 const char *buf, int n){
    unsigned int remoteIpHost;
    char code;
    const char *payload;
    int payloadLen;
    char pseudo[BEUIP_MAX_PSEUDO_LEN];
    char pseudoDest[BEUIP_MAX_PSEUDO_LEN];
    char text[BEUIP_LBUF + 1];
    char outMsg[BEUIP_LBUF + 1];
    int outLen;
    int idx;
    struct sockaddr_in sockDest;

    remoteIpHost = ntohl(remote->sin_addr.s_addr);

    if(!creme_parse_header(buf, n, &code, &payload, &payloadLen)){
        return 0;
    }

    if((code == BEUIP_CODE_LIST || code == BEUIP_CODE_TO_PSEUDO || code == BEUIP_CODE_TO_ALL) &&
       remoteIpHost != INADDR_LOOPBACK){
        fprintf(stderr, "[CODE %c] FROM: %s | STATUS: REFUSED (non-local source)\n",
                code, creme_addrip(remoteIpHost));
        return 1;
    }

    if(code == BEUIP_CODE_LEAVE){
        if(!creme_copy_payload_string(payload, payloadLen, pseudo, sizeof(pseudo))){
            fprintf(stderr, "[CODE 0] FROM: %s | STATUS: INVALID PAYLOAD\n",
                    creme_addrip(remoteIpHost));
            return 1;
        }
        creme_remove_peer(table, remoteIpHost, pseudo);
        printf("[CODE 0] FROM: %s | EVENT: LEAVE | PSEUDO: %s\n",
               creme_addrip(remoteIpHost), pseudo);
        return 1;
    }

    if(code == BEUIP_CODE_LIST){
        if(payloadLen != 0){
            fprintf(stderr, "[CODE 3] FROM: %s | STATUS: INVALID PAYLOAD\n",
                    creme_addrip(remoteIpHost));
            return 1;
        }
        printf("[CODE 3] FROM: %s | EVENT: LIST REQUEST\n",
               creme_addrip(remoteIpHost));
        creme_print_peer_list(table);
        return 1;
    }

    if(code == BEUIP_CODE_TO_PSEUDO){
        if(!creme_parse_to_pseudo_payload(payload, payloadLen,
                                          pseudoDest, sizeof(pseudoDest),
                                          text, sizeof(text))){
            fprintf(stderr, "[CODE 4] FROM: %s | STATUS: INVALID PAYLOAD\n",
                    creme_addrip(remoteIpHost));
            return 1;
        }

        idx = creme_find_peer_by_pseudo(table, pseudoDest);
        if(idx < 0){
            fprintf(stderr, "[CODE 4] FROM: %s | STATUS: DEST NOT FOUND | TO: %s\n",
                    creme_addrip(remoteIpHost), pseudoDest);
            return 1;
        }

        outLen = creme_build_message(BEUIP_CODE_TEXT, text, outMsg, sizeof(outMsg));
        if(outLen < 0){
            fprintf(stderr, "message code=9 trop long\n");
            return 1;
        }

        memset(&sockDest, 0, sizeof(sockDest));
        sockDest.sin_family = AF_INET;
        sockDest.sin_port = htons(BEUIP_PORT);
        sockDest.sin_addr.s_addr = htonl(table->entries[idx].ip);

        if(sendto(sid, outMsg, outLen, MSG_CONFIRM,
                  (const struct sockaddr *)&sockDest, sizeof(sockDest)) == -1){
            perror("sendto code=9");
            return 1;
        }

        printf("[CODE 4] FROM: %s | TO: %s | MSG: %s\n",
               creme_addrip(remoteIpHost), pseudoDest, text);
        return 1;
    }

    if(code == BEUIP_CODE_TO_ALL){
        int i;

        if(!creme_copy_payload_string(payload, payloadLen, text, sizeof(text))){
            fprintf(stderr, "[CODE 5] FROM: %s | STATUS: INVALID PAYLOAD\n",
                    creme_addrip(remoteIpHost));
            return 1;
        }

        outLen = creme_build_message(BEUIP_CODE_TEXT, text, outMsg, sizeof(outMsg));
        if(outLen < 0){
            fprintf(stderr, "message code=9 trop long\n");
            return 1;
        }

        for(i = 0; i < table->count; i++){
            if(strcmp(table->entries[i].pseudo, selfPseudo) == 0) continue;

            memset(&sockDest, 0, sizeof(sockDest));
            sockDest.sin_family = AF_INET;
            sockDest.sin_port = htons(BEUIP_PORT);
            sockDest.sin_addr.s_addr = htonl(table->entries[i].ip);

            if(sendto(sid, outMsg, outLen, MSG_CONFIRM,
                      (const struct sockaddr *)&sockDest, sizeof(sockDest)) == -1){
                perror("sendto code=9 (broadcast msg)");
            }
        }

        printf("[CODE 5] FROM: %s | TO: ALL | MSG: %s\n",
               creme_addrip(remoteIpHost), text);
        return 1;
    }

    if(code == BEUIP_CODE_TEXT){
        idx = creme_find_peer_by_ip(table, remoteIpHost);
        if(!creme_copy_payload_string(payload, payloadLen, text, sizeof(text))){
            fprintf(stderr, "[CODE 9] FROM: %s | STATUS: INVALID PAYLOAD\n",
                    creme_addrip(remoteIpHost));
            return 1;
        }
        if(idx < 0){
            fprintf(stderr, "[CODE 9] FROM: %s | STATUS: UNKNOWN SOURCE | MSG: %s\n",
                    creme_addrip(remoteIpHost), text);
            return 1;
        }

        printf("[CODE 9] FROM: %s (%s) | MSG: %s\n",
               table->entries[idx].pseudo, creme_addrip(remoteIpHost), text);
        return 1;
    }

    if(!creme_copy_payload_string(payload, payloadLen, pseudo, sizeof(pseudo))){
        fprintf(stderr, "[CODE %c] FROM: %s | STATUS: INVALID PAYLOAD\n",
                code, creme_addrip(remoteIpHost));
        return 1;
    }

    if(creme_add_peer(table, remoteIpHost, pseudo) < 0){
        fprintf(stderr, "Table peers pleine (%d). Ignore %s/%s\n",
                BEUIP_MAX_PEERS, creme_addrip(remoteIpHost), pseudo);
        return 1;
    }

    printf("[CODE %c] FROM: %s | PSEUDO: %s\n",
           code, creme_addrip(remoteIpHost), pseudo);

    if(code == BEUIP_CODE_BROADCAST){
        outLen = creme_build_message(BEUIP_CODE_ACK, selfPseudo, outMsg, sizeof(outMsg));
        if(outLen < 0){
            fprintf(stderr, "ACK non envoye: message trop long\n");
            return 1;
        }

        if(sendto(sid, outMsg, outLen, MSG_CONFIRM,
                  (const struct sockaddr *)remote, remoteLen) == -1){
            perror("sendto ack");
        }
    }

    return 1;
}

void creme_init_peer_table(creme_peer_table *table){
    table->count = 0;
}

void creme_print_peer_list(const creme_peer_table *table){
    int i;

    printf("[CODE 3] PEERS ONLINE: %d\n", table->count);
    if(table->count == 0){
        printf("[CODE 3] LIST: (empty)\n");
        return;
    }

    for(i = 0; i < table->count; i++){
        printf("[PEER %02d] %-16s IP: %s\n", i + 1,
               table->entries[i].pseudo, creme_addrip(table->entries[i].ip));
    }
}

int creme_find_peer(const creme_peer_table *table, unsigned int ipHostOrder, const char *pseudo){
    int i;

    for(i = 0; i < table->count; i++){
        if(table->entries[i].ip == ipHostOrder && strcmp(table->entries[i].pseudo, pseudo) == 0){
            return i;
        }
    }
    return -1;
}

int creme_find_peer_by_pseudo(const creme_peer_table *table, const char *pseudo){
    int i;

    for(i = 0; i < table->count; i++){
        if(strcmp(table->entries[i].pseudo, pseudo) == 0){
            return i;
        }
    }
    return -1;
}

int creme_find_peer_by_ip(const creme_peer_table *table, unsigned int ipHostOrder){
    int i;

    for(i = 0; i < table->count; i++){
        if(table->entries[i].ip == ipHostOrder){
            return i;
        }
    }
    return -1;
}

int creme_add_peer(creme_peer_table *table, unsigned int ipHostOrder, const char *pseudo){
    if(creme_find_peer(table, ipHostOrder, pseudo) >= 0){
        return 0;
    }
    if(table->count >= BEUIP_MAX_PEERS){
        return -1;
    }

    table->entries[table->count].ip = ipHostOrder;
    strncpy(table->entries[table->count].pseudo, pseudo, BEUIP_MAX_PSEUDO_LEN - 1);
    table->entries[table->count].pseudo[BEUIP_MAX_PSEUDO_LEN - 1] = '\0';
    table->count++;
    return 1;
}

int creme_remove_peer(creme_peer_table *table, unsigned int ipHostOrder, const char *pseudo){
    int idx;
    int i;

    idx = creme_find_peer(table, ipHostOrder, pseudo);
    if(idx < 0){
        return 0;
    }

    for(i = idx; i < table->count - 1; i++){
        table->entries[i] = table->entries[i + 1];
    }
    table->count--;
    return 1;
}