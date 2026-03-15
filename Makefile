udp: servudp cliudp

beuip: servbeuip clibeuip

creme.o: creme.c creme.h
	cc -o creme.o -c creme.c -Wall

cliudp : cliudp.c
	cc -Wall -o cliudp cliudp.c

servudp : servudp.c
	cc -Wall -o servudp servudp.c

servbeuip : servbeuip.c creme.h creme.o
	cc -Wall -o servbeuip servbeuip.c creme.o

clibeuip : clibeuip.c creme.h creme.o
	cc -Wall -o clibeuip clibeuip.c creme.o

triceps: triceps.c
	cc -o triceps triceps.c

biceps: biceps.o gescom.o
	cc -o biceps biceps.o gescom.o -Wall -lreadline

biceps.o: biceps.c gescom.h
	cc -o biceps.o -c biceps.c -Wall

gescom.o: gescom.c gescom.h
	cc -o gescom.o -c gescom.c -Wall

biceps-debug: biceps.c gescom.c gescom.h
	cc -o biceps-debug biceps.c gescom.c -Wall -Werror -lreadline -g -DTRACE

biceps-valgrind: biceps-debug
	valgrind --leak-check=full ./biceps-debug

clean:
	rm -f triceps
	rm -f biceps
	rm -f biceps-debug
	rm -f creme.o
	rm -f servbeuip clibeuip
	rm -f cliudp servudp
	rm -f *.o