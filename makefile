all: proctopk threadtopk

proctopk: proctopk.c
	gcc -Wall -g -o proctopk proctopk.c

threadtopk: threadtopk.c
	gcc -Wall -g -o threadtopk threadtopk.c

clean:
	rm -fr *~ proctopk threadtopk