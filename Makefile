audio2pcm:main.o
	gcc -g -o audio2pcm main.o -lavformat -lavcodec -lswresample -lavutil
	rm *.o
main.o:main.c
	gcc -g -c main.c -o main.o

clean:
	rm audio2pcm
