######################################
# Nazev: Makefile
# Ucel kodu: IPK proj3
#      makefile pro preklad programu rdtclient a rdtserver
# Autor: Barbora Skrivankova, xskriv01, FIT VUT
# Vytvoreno: duben 2013
######################################

C++FLAGS=-std=c++98 -Wall -pedantic -g

all: rdtclient

rdtclient: rdtclient.o
	g++ $(C++FLAGS) rdtclient.o -o rdtclient

rdtclient.o: rdtclient.cpp
	g++ $(C++FLAGS) -c rdtclient.cpp

clean: 
	rm -f *.o