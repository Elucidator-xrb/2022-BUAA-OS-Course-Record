#!/bin/bash

vpath %.h include
vpath %.c code

fibo : main.o fibo.o
	gcc -o fibo code/main.o code/fibo.o

main.o : main.c fibo.h
	gcc -c -o code/main.o code/main.c -I "include/"
fibo.o : fibo.c fibo.h
	gcc -c -o code/fibo.o code/fibo.c -I "include/"

.PHONY : clean
clean : 
	rm code/*.o

# Another Method
# all :
# 		$(MAKE) --directory="code"
# 		gcc -o fibo code/main.o code/fibo.o
#
# clean :
# 		$(MAKE) clean --directory="code"
