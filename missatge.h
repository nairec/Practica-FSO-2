/**********************************************************************/
/*                                                                    */
/*  Codi de les rutines dels missatges.                               */
/*                                                                    */
/*  Semantica : Pas de missatges amb comunicacio indirecta, simetrica */
/*              i bloquejant a la funcio "receiveM" quan no hi ha mis-*/
/*		satges i a la funcio "sendM" quan la cua de missatges */
/*		esta plena.                                           */
/*  Implementacio : Fent servir les rutines del SO UNIX que implemen- */
/*                  ten missatges amb una semantica diferent.         */
/*  Notes : S'ha afegit una nova rutina "elim_mis" per questions      */
/*          d'adaptacio de la implementacio. En UNIX quan un proces   */
/*          acaba no allibera els missatges que ha creat o utilitzat. */
/*                                                                    */
/*  Autor : Carles Aliagas                                            */
/**********************************************************************/
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <assert.h>
#include <stdio.h>

#define TAM_MAX_MIS 128
#define UNIC 1
#define TIPUS_FI_JUEGO 3

typedef struct {
	long tipus;	/* tipus del missatge a enviar. Es far servir per
			   quan es volen tenir prioritats als missatges */
	char missatge[TAM_MAX_MIS];
	} tmis;

typedef struct {
    long tipus;  /* Debe ser el primer campo para msgrcv */
    /* No necesita más campos */
} missatge_fi_t;

int ini_mis();
void elim_mis(int id_mis);
void sendM (int id_mis, void * missatge, int nbytes);
int receiveM (int id_mis, void * missatge);

typedef struct {
    int tipus; // Control o nova pilota
    int fila;
    int columna;
    int c_pal;
    int m_pal;
    float vel_f;
    float vel_c;
    int retard;
} missatge_nova_pilota_t;