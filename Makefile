triceps: triceps.c
	cc -o triceps triceps.c

biceps: biceps.c
	cc -o biceps biceps.c -Wall -lreadline

biceps-debug: biceps.c
	cc -o biceps-debug biceps.c -Wall -Werror -lreadline -g

biceps-valgrind: biceps-debug
	valgrind --leak-check=full ./biceps-debug

clean:
	rm -f triceps
	rm -f biceps
	rm -f biceps-debug

