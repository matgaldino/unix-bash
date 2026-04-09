#ifndef CREME_H
#define CREME_H

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define CREME_VERSION "1.0"

#define BEUIP_PORT 9998
#define BEUIP_LBUF 512
#define BEUIP_MAX_PEERS 255
#define BEUIP_MAX_PSEUDO_LEN 128
#define BEUIP_TAG "BEUIP"
#define BEUIP_CODE_BROADCAST '1'
#define BEUIP_CODE_ACK '2'
#define BEUIP_CODE_LIST '3'
#define BEUIP_CODE_TO_PSEUDO '4'
#define BEUIP_CODE_TO_ALL '5'
#define BEUIP_CODE_LEAVE '0'
#define BEUIP_CODE_TEXT '9'
#ifndef BEUIP_BROADCAST_IP
#define BEUIP_BROADCAST_IP "192.168.88.255"
#endif

typedef struct {
    unsigned int ip;
    char pseudo[BEUIP_MAX_PSEUDO_LEN];
} creme_peer_entry;

typedef struct {
    creme_peer_entry entries[BEUIP_MAX_PEERS];
    int count;
} creme_peer_table;

const char *creme_version(void);
const char *creme_addrip(unsigned long address);

int creme_is_valid_code(char code);
int creme_parse_header(const char *msg, int n, char *code, const char **payload, int *payloadLen);
int creme_copy_payload_string(const char *payload, int payloadLen, char *out, size_t outSize);
int creme_parse_to_pseudo_payload(const char *payload, int payloadLen,
                                  char *pseudoDest, size_t pseudoDestSize,
                                  char *text, size_t textSize);
int creme_build_message(char code, const char *pseudo, char *out, size_t outSize);
int creme_build_to_pseudo_message(const char *pseudoDest, const char *text, char *out, size_t outSize);
int creme_parse_reply(const char *msg, int n, char *code, char *pseudo, size_t pseudoSize);

int creme_prepare_ipv4_addr(struct sockaddr_in *addr, const char *ip, int port);
int creme_enable_broadcast(int sid);
int creme_enable_recv_timeout(int sid, int timeoutSeconds);
int creme_bind_any(int sid, int port);

int creme_send_presence(int sid, const struct sockaddr_in *dest, const char *pseudo);
int creme_send_leave(int sid, const struct sockaddr_in *dest, const char *pseudo);
int creme_send_list_request(int sid, const struct sockaddr_in *dest);
int creme_send_private_message(int sid, const struct sockaddr_in *dest, const char *pseudoDest, const char *text);
int creme_send_broadcast_text(int sid, const struct sockaddr_in *dest, const char *text);

int creme_handle_server_datagram(int sid, const char *selfPseudo, creme_peer_table *table,
                                 const struct sockaddr_in *remote, socklen_t remoteLen,
                                 const char *buf, int n);

void creme_init_peer_table(creme_peer_table *table);
void creme_print_peer_list(const creme_peer_table *table);
int creme_find_peer(const creme_peer_table *table, unsigned int ipHostOrder, const char *pseudo);
int creme_find_peer_by_pseudo(const creme_peer_table *table, const char *pseudo);
int creme_find_peer_by_ip(const creme_peer_table *table, unsigned int ipHostOrder);
int creme_add_peer(creme_peer_table *table, unsigned int ipHostOrder, const char *pseudo);
int creme_remove_peer(creme_peer_table *table, unsigned int ipHostOrder, const char *pseudo);

#endif