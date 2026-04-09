CC = cc
CFLAGS = -Wall -Werror
DEBUG_CFLAGS = $(CFLAGS) -g -DTRACE
MEMLEAK_CFLAGS = -Wall -Werror -g -O0 -DNO_READLINE
READLINE_LIB = -lreadline

biceps: biceps.o gescom.o creme.o
	$(CC) $(CFLAGS) -o biceps biceps.o gescom.o creme.o $(READLINE_LIB) -lpthread

# Targets supplémentaires
all: biceps beuip

beuip: servbeuip clibeuip

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -o creme.o -c creme.c

servbeuip : servbeuip.c creme.h creme.o
	$(CC) $(CFLAGS) -o servbeuip servbeuip.c creme.o

clibeuip : clibeuip.c creme.h creme.o
	$(CC) $(CFLAGS) -o clibeuip clibeuip.c creme.o

biceps.o: biceps.c gescom.h
	$(CC) $(CFLAGS) -o biceps.o -c biceps.c

gescom.o: gescom.c gescom.h creme.h
	$(CC) $(CFLAGS) -o gescom.o -c gescom.c

biceps-debug: biceps.c gescom.c creme.c gescom.h creme.h
	$(CC) $(DEBUG_CFLAGS) -o biceps-debug biceps.c gescom.c creme.c $(READLINE_LIB) -lpthread

biceps-memory-leaks: biceps.c gescom.c creme.c gescom.h creme.h
	$(CC) $(MEMLEAK_CFLAGS) -o biceps-memory-leaks biceps.c gescom.c creme.c -lpthread

biceps-memory: biceps.c gescom.c creme.c gescom.h creme.h
	$(CC) $(MEMLEAK_CFLAGS) -o biceps biceps.c gescom.c creme.c -lpthread

memory-leak: biceps-memory-leaks biceps-memory
	valgrind --leak-check=full --track-origins=yes ./biceps-memory-leaks < /dev/null

biceps-valgrind: biceps-debug
	valgrind --leak-check=full --track-origins=yes ./biceps-debug

clean:
	rm -f biceps biceps-debug biceps-memory-leaks
	rm -f servbeuip clibeuip
	rm -f *.o