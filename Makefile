all : main

main : main.c coroutine.c
	gcc -g -Wall -o $@ $^

check: 
	./main

clean :
	rm main
