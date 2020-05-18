CC		=  gcc
CFLAGS  = -lpthread -Wall   

INCDIR		= ./include
TESTDIR 	= ./testfile
DEBUG 		= -DDEBUG

TESTFILE = test.txt

.PHONY: test1 clean

all: ./supermarket

./supermarket: ./supermarket.c
	$(CC) ./supermarket.c $(CFLAGS) -o $@ 

test:
	(./supermarket testfile/config.txt & echo $$! > supermarket.PID) &
	sleep 25s; \
	kill -1 $$(cat supermarket.PID); \
	./analisi.sh $$(cat supermarket.PID); \
	
clean:
	rm ./supermarket
	rm ./statsfile.log
	rm supermarket.PID

