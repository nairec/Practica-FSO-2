# Makefile per a la Pràctica 2 d'FSO
# Compilació dels programes mur0, mur1, pilota1, mur2 i pilota2

CC = gcc
CFLAGS = -Wall -g 
LIBS = -lcurses

# Objectes comuns per a les fases amb IPC (memòria, semàfors i missatges)
OBJS_COMUNS = winsuport2.o memoria.o semafor.o missatge.o
LDFLAGS = -lcurses -lpthread

# Objectius principals
all: mur0 mur1 pilota1 mur2 pilota2 mur3

# Rutines compartides (llibreria gràfica i IPC)
winsuport2.o: winsuport2.c winsuport2.h
	$(CC) $(CFLAGS) -c winsuport2.c -o winsuport2.o

memoria.o: memoria.c memoria.h
	$(CC) $(CFLAGS) -c memoria.c -o memoria.o

semafor.o: semafor.c semafor.h
	$(CC) $(CFLAGS) -c semafor.c -o semafor.o

missatge.o: missatge.c missatge.h
	$(CC) $(CFLAGS) -c missatge.c -o missatge.o

# --- FASE 0  ---
mur0.o: mur0.c winsuport2.h memoria.h
	$(CC) $(CFLAGS) -c mur0.c -o mur0.o

mur0: mur0.o winsuport2.o memoria.o
	$(CC) $(CFLAGS) mur0.o winsuport2.o memoria.o -o mur0 $(LIBS)

# --- FASE 1 ---
mur1.o: mur1.c winsuport2.h memoria.h
	$(CC) $(CFLAGS) -c mur1.c -o mur1.o

mur1: mur1.o winsuport2.o memoria.o
	$(CC) $(CFLAGS) mur1.o winsuport2.o memoria.o -o mur1 $(LIBS)

pilota1.o: pilota1.c winsuport2.h memoria.h
	$(CC) $(CFLAGS) -c pilota1.c -o pilota1.o

pilota1: pilota1.o winsuport2.o memoria.o
	$(CC) $(CFLAGS) pilota1.o winsuport2.o memoria.o -o pilota1 $(LIBS)

# --- FASE 2 ---
mur2.o: mur2.c winsuport2.h memoria.h semafor.h missatge.h
	$(CC) $(CFLAGS) -c mur2.c -o mur2.o

mur2: mur2.o $(OBJS_COMUNS)
	$(CC) -o mur2 mur2.o $(OBJS_COMUNS) $(LDFLAGS)

pilota2.o: pilota2.c winsuport2.h memoria.h semafor.h missatge.h
	$(CC) $(CFLAGS) -c pilota2.c -o pilota2.o

pilota2: pilota2.o $(OBJS_COMUNS)
	$(CC) -o pilota2 pilota2.o $(OBJS_COMUNS) $(LDFLAGS)

# --- FASE 3 ---
mur3.o: mur3.c winsuport2.h memoria.h semafor.h missatge.h
	$(CC) $(CFLAGS) -c mur3.c -o mur3.o

mur3: mur3.o $(OBJS_COMUNS)
	$(CC) -o mur3 mur3.o $(OBJS_COMUNS) $(LDFLAGS)

# --- Neteja ---
# Executar 'make clean' per esborrar binaris i fitxers objecte 
clean:
	rm -f *.o mur0 mur1 pilota1 mur2 pilota2 mur3