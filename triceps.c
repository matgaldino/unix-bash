/* triceps.c : le tres rudimentaire interpreteur de commande des etudiants de
  Polytech-Sorbonne
 a n'utiliser QUE si votre biceps ne fonctionne pas */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int run = 1, debug=0;

#define MAXPAR 10 /* nombre maximum de mots dans la commande */

char * Mots[MAXPAR];
int NMots;

void Sortie(void) { run = 0; }

void executeCommande(void)
{
   if (strcmp(Mots[0],"exit") == 0) return Sortie();
   fprintf(stderr,"%s : commande inexistante !\n",Mots[0]);
   return;
}

int traiteCommande(char * b) /* separe la ligne de commande en mots */
{
char *d, *f;
int mode =1;
   d=b;
   f=b+strlen(b);
   NMots=0;
   while (d<f) {
     if (mode) { /* on cherche le 1er car. non separateur */
        if ((*d != ' ') && (*d != '\t')) {
           if (NMots == MAXPAR) break;
           Mots[NMots++] = d;
           mode = 0;
        }
     } else { /* on cherche le 1er separateur */
        if ((*d == ' ') || (*d == '\t')) {
          *d = '\0';
          mode = 1;
        }
     } 
     d++;
   }
   if (debug) printf("Il y a %d mots\n",NMots);
   return NMots;
}

int main(int N, char * P[])
{
char *buf=NULL;
size_t lb=0;
int n, i;
   if (N > 2) {
      fprintf(stderr,"Utilisation : %s [-d]\n",P[0]); return 1;
   }
   if (N == 2) {
      if (strcmp(P[1],"-d") == 0) debug=1;
      else fprintf(stderr,"parametre %s invalide !\n",P[1]);
   }
   while (run) {
     printf("triceps> ");
     if ((n = getline(&buf, &lb, stdin)) != -1) {
         if (buf[n-1] == '\n') buf[--n] = '\0';
         if (debug) printf("Lu %d car.: %s\n",n,buf);
     } else break;
     /* traitement de la commande */
     n = traiteCommande(buf);
     if (debug) {
        printf("La commande est %s !\n",Mots[0]);
        if (n > 1) {
           printf("Les parametres sont :\n");
           for (i=1; i<n; i++) printf("\t%s\n",Mots[i]);
        }
     }
     if (n>0) executeCommande();
     free(buf);
     n=0;
     buf=NULL;
   }
   printf("Bye !\n");
   return 0;
}

