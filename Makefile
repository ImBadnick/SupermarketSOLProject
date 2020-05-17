CC		=  gcc
CFLAGS  = -lpthread -Wall   

INCDIR		= ./include
TESTDIR 	= ./testfile
DEBUG 		= -DDEBUG


ObjFlags = -c -fPIC -I./include/ -o
TESTFILE = test.txt

.PHONY: test1 clean debug

all: ./supermarket

./supermarket: ./supermarket.c
	$(CC) $(DEBUG) ./supermarket.c $(CFLAGS) -o $@ 

test:
	@(./supermarket testfile/test.txt & echo $$! > supermarket.PID)
	@sleep 2s
	@kill -3 $$(cat supermarket.PID)
	@./analisi.sh
	
clean:
	rm ./supermarket
	rm ./statsfile.log
	rm supermarket.PID

