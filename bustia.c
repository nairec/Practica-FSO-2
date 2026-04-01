/**********************************************************************/
/*                                                                    */
/*  bustia.c - Implementació de cues de missatges (System V IPC)      */
/*                                                                    */
/**********************************************************************/

#include "bustia.h"

/**********************************************************************/
/* ini_mis - Crea una cola de missatges privada                       */
/* Retorna: identificador de la cola, o -1 si error                   */
/**********************************************************************/
int ini_mis(void)
{
    int id_mis;
    
    /* Crear cola de mensajes privada (IPC_PRIVATE) */
    /* 0600 = permisos: solo el propietario puede leer/escribir */
    id_mis = msgget(IPC_PRIVATE, 0600);
    
    if (id_mis == -1) {
        perror("Error en msgget");
        return -1;
    }
    
    return id_mis;
}

/**********************************************************************/
/* elim_mis - Elimina la cola de mensajes                             */
/**********************************************************************/
void elim_mis(int id_mis)
{
    int r;
    
    r = msgctl(id_mis, IPC_RMID, NULL);
    if (r == -1) {
        perror("Error en msgctl (eliminar cola)");
    }
}

/**********************************************************************/
/* sendM - Envia un missatge a la cola                                */
/* Retorna: 0 si éxito, -1 si error                                   */
/**********************************************************************/
int sendM(int id_mis, missatge_t *msg)
{
    int r;
    
    /* msgsnd: enviar mensaje a la cola */
    /* sizeof(*msg) - sizeof(long) = tamaño de los datos sin mtype */
    r = msgsnd(id_mis, msg, sizeof(*msg) - sizeof(long), 0);
    
    if (r == -1) {
        perror("Error en msgsnd");
        return -1;
    }
    
    return 0;
}

/**********************************************************************/
/* receiveM - Rep un missatge de la cola (BLOQUEANTE)                 */
/* Retorna: 0 si éxito, -1 si error                                   */
/**********************************************************************/
int receiveM(int id_mis, missatge_t *msg)
{
    int r;
    
    /* msgrcv: recibir mensaje (bloqueante) */
    /* 0 = cualquier tipo de mensaje */
    r = msgrcv(id_mis, msg, sizeof(*msg) - sizeof(long), 0, 0);
    
    if (r == -1) {
        perror("Error en msgrcv");
        return -1;
    }
    
    return 0;
}

/**********************************************************************/
/* receiveM_nowait - Rep un missatge de la cola (NO BLOQUEANTE)       */
/* Retorna: 0 si éxito, -1 si no hay mensaje o error                  */
/**********************************************************************/
int receiveM_nowait(int id_mis, missatge_t *msg)
{
    int r;
    
    /* IPC_NOWAIT = no bloquear si no hay mensajes */
    r = msgrcv(id_mis, msg, sizeof(*msg) - sizeof(long), 0, IPC_NOWAIT);
    
    if (r == -1) {
        /* Si no hay mensajes, no es un error real */
        if (errno == ENOMSG) {
            return -1;  /* Simplemente no hay mensaje */
        }
        perror("Error en msgrcv");
        return -1;
    }
    
    return 0;
}