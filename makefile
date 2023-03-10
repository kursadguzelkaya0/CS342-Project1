all: proctopk
proctopk: proctopk.c
	gcc -Wall -g -o proctopk proctopk.c
clean:
	rm -fr proctopk proctopk.o *~