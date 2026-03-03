#ifndef GESCOM_H
#define GESCOM_H

#define NBMAXC 10
#define HISTORY_FILE ".biceps_history"
#define HISTORY_SIZE 100

typedef struct{
    char *name;
    int (*f)(int, char **);
} comInt;

//char *copyString(char *s); 4.4 remplace par strdup()
void updateComInt(char *bicepsVersion);
void listComInt(void);
void execLine(char *line);
char *getHistoryPath(void);

#endif /* GESCOM_H */