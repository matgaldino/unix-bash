/*****
* Exemple de client UDP
* socket en mode non connecte
*****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define LBUF 512

/* parametres :
        P[1] = nom de la machine serveur
        P[2] = port
        P[3] = message
*/
int main(int N, char*P[])
{
int sid;
int n;
char buf[LBUF+1];
struct hostent *h;
struct sockaddr_in Sock;
struct sockaddr_in SockAck;
socklen_t lAck;

    if (N != 4) {
        fprintf(stderr,"Utilisation : %s nom_serveur port message\n", P[0]);
        return(1);
    }
    /* creation du socket */
    if ((sid=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0) {
        perror("socket");
        return(2);
    }
    /* recuperation adresse du serveur */
    if (!(h=gethostbyname(P[1]))) {
        perror(P[1]);
        return(3);
    }
    bzero(&Sock,sizeof(Sock));
    Sock.sin_family = AF_INET;
    bcopy(h->h_addr,&Sock.sin_addr,h->h_length);
    Sock.sin_port = htons(atoi(P[2]));
    if (sendto(sid,P[3],strlen(P[3]),0,(struct sockaddr *)&Sock,
                           sizeof(Sock))==-1) {
        perror("sendto");
        return(4);
    }
    printf("Envoi OK !\n");

    lAck = sizeof(SockAck);
    n = recvfrom(sid, (void *)buf, LBUF, 0, (struct sockaddr *)&SockAck, &lAck);
    if (n == -1) {
        perror("recvfrom");
        return(5);
    }
    buf[n] = '\0';
    printf("AR recu : <%s>\n", buf);

    return 0;
}

