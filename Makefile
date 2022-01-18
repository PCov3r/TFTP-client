CC=gcc
CFLAGS=-W -Wall
EXEC=gettftp

all: $(EXEC)

gettftp: gettftp.o
	$(CC) -o gettftp gettftp.o

gettftp.o: gettftp.c
	$(CC) -o gettftp.o -c gettftp.c $(CFLAGS)

clean:
	rm -rf *.o

mrproper: clean
	rm -rf gettftp
