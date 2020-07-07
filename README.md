# SupermarketSOLProject
Implementation of a supermarket system for the course "operative system and laboratory" at IT university.

How to use the program:

1) Make -> To build the .c file
2) Make test -> To run the test 
3) Make clean -> To clean the directory 

To modify config parameters -> Modify the ./testfile/config.txt

config parameters:  
K -> Number of supermarket checkouts  
C -> Number of max customers in the supermarket initially  
E -> Number of customers that need to exit before make enter other E customers  
T -> Min time for customers to buy  
P -> Number of max products for each customers  
S -> Seconds to do a product by the cashiers  
S1 -> If the are S1 queues with <=1 customers -> Close one of them  
S2 -> If the is a queue with more then S2 customers in queue -> Open another supermarket checkouts  
smopen -> How much sm are opened at the start of the program  
directornews -> Update time used from sm to send queue length to director   


TIP: You can change the line "$(CC) ./supermarket.c $(CFLAGS) -o $@ " in the makefile to "$(CC) -g $(DEBUG) ./supermarket.c $(CFLAGS) -o $@" to compile with the debug option. The debug option prints some information on stdout usefull to understand how the program is working.

