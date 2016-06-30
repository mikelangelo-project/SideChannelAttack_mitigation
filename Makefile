FillingTheCache : EvictionSet.o main.o
	cc -o FillingTheCache EvictionSet.o main.o -l ssl -l gnutls -l gmp -l crypto

main.o : main.c
	cc -c main.c

EvictionSet.o : EvictionSet.c
	cc -c EvictionSet.c
