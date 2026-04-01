/**********************************************************************/
/*                                                                    */
/*  bustia.h - Interficie per a la comunicació amb cues de missatges  */
/*                                                                    */
/**********************************************************************/

#ifndef _BUSTIA_H
#define _BUSTIA_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Estructura del mensaje para crear nuevas pilotas */
typedef struct {
    long mtype;         /* Tipo de mensaje (debe ser el primer campo) */
    int fila;           /* Fila donde crear la nueva pilota */
    int columna;        /* Columna donde crear la nueva pilota */
    float vel_f;        /* Velocidad vertical */
    float vel_c;        /* Velocidad horizontal */
    int c_pal;          /* Posición de la paleta (columna) */
    int m_pal;          /* Tamaño de la paleta */
} missatge_t;

/* Constantes para los tipos de mensaje */
#define MSG_NOVA_PILOTA  1   /* Solicitud de nueva pilota */
#define MSG_FI            2   /* Mensaje de finalización (opcional) */

/* Prototipos de funciones */
int ini_mis(void);                     /* Crear una nueva cola de mensajes */
void elim_mis(int id_mis);             /* Eliminar la cola de mensajes */
int sendM(int id_mis, missatge_t *msg);  /* Enviar un mensaje */
int receiveM(int id_mis, missatge_t *msg); /* Recibir mensaje (bloqueante) */
int receiveM_nowait(int id_mis, missatge_t *msg); /* Recibir sin bloquear */

#endif