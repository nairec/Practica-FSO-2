/*****************************************************************************/
/* */
/* pilota2.c                                                                    */
/* */                  
/* Versió seqüencial adaptada a winsuport2 i memòria compartida IPC amb semàfors.         */
/* */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "winsuport2.h"
#include "memoria.h"
#include "missatge.h"
#include "semafor.h"

/* --- Definicions de constants --- */
//#define MAX_THREADS	10 [maxim de fils] 
//#define MAXBALLS	(MAX_THREADS-1) [maxim de pilotes] No fa falta perque cada procés pilota nomes controla UNA pilota
//[validacio de dimensions] Les dimensions ja venen validades pel pare mur2
//#define MIN_FIL	10
//#define MAX_FIL	50
//#define MIN_COL	10
//#define MAX_COL	80

/* Constants per a la creació dels blocs del joc */
//#define BLKSIZE	3 [Mida dels blocs en caràcters] No es necessita per al moviment de la pilota, només per crear blocs
//#define BLKGAP	2 [Espai entre blocs] Només per a la creació inicial del taulell
#define BLKCHAR 'B' // identificar blocs de multiplicació
#define WLLCHAR '#' // identificar parets indestructibles
#define FRNTCHAR 'T' // identificar blocs frontissa (tipus A)
#define LONGMISS 65 // Mida del buffer per missatges
//controlar l'atribut invers en dibuixar (win_escricar)
#define NO_INV 0
#define INVERS 1
#define MAX_VEL_F 2.0

/* Constants per enviar missatges */
#define TIPUS_CONTROL 1
#define TIPUS_NOVA_PILOTA 2
#define TIPUS_INCREMENT_PODER 3
#define TIPUS_PODER_ACTIU 4
#define TIPUS_PODER_NO_ACTIU 5
#define TIPUS_PILOTA_SACRIFICI 6
#define TIPUS_REGISTRE_PILOTA 7

/* Struct de tipus Paleta */
typedef struct {
	int fila;
	int col_inicial;
	int col_actual;
	int dir_lateral;
	int dir_vertical;
    int salt_vertical;
	int amplada;
	int id;
	pthread_t thread_id;
} paleta_t;

/* Text d'ajuda que es mostra si s'executa el programa sense arguments */
char *descripcio[] = {
	"\n",
	"Aquest programa implementa una versio basica del joc Arkanoid o Breakout:\n",
	"generar un camp de joc rectangular amb una porteria, una paleta que s\'ha\n",
	"de moure amb el teclat per a cobrir la porteria, i una pilota que rebota\n",
	"contra les parets del camp, a la paleta i els blocs. El programa acaba si\n",
	"la pilota surt per la porteria o no queden mes blocs. Tambe es pot acabar\n",
	"amb la tecla RETURN.\n",
	"\n",
	"  Arguments del programa:\n",
	"\n",
	"       $ ./mur0 fitxer_config [retard]\n",
	"\n",
	"     El primer argument ha de ser el nom d\'un fitxer de text amb la\n",
	"     configuracio de la partida, on la primera fila inclou informacio\n",
	"     del camp de joc (enters), la segona fila indica posicio i mida\n",
	"     de la paleta (enters) i la tercera fila indica posicio\n",
	"     i velocitat de la pilota (valors reals):\n",
	"          num_files  num_columnes  mida_porteria\n",
	"          pos_col_paleta  mida_paleta\n",
	"          pos_fila   pos_columna   vel_fila  vel_columna\n",
	"\n",
	"     Alternativament, es pot donar el valor 0 per especificar configuracio\n",
	"     automatica (pantalla completa, porteria calculada, paleta al mig, etc).\n",
	"*"
};

/* --- Variables Globals --- */
int fi = 0;
float vel_f;

/* Variables de l'entorn de joc (llegides des dels arguments) */
int n_fil, n_col;		/* dimensions del camp de joc */
int m_por;			    /* mida de la porteria (en caracters) */
int nblocs;         /* nombre de blocs restants per trencar */
int n_pal;      /* nombre de paletes en joc */
int retard;			    /* valor del retard de moviment, en mil.lisegons */
int c_pal, m_pal;
char strin[LONGMISS];	/* variable per a generar missatges de text a la pantalla */

/* Variables globals per a la memòria compartida (IPC) i semàfors */
int id_mem;             /* identificador de la memòria compartida creada */
int id_sem_curses;      /* identificador del semàfor de curses */
int id_sem_memoria;     /* identificador del semàfor de memòria */
int id_mis;
int id_mis_thread;      /* identificador de la bústia de missatges dels threads */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *p_mem;            /* punter cap a la zona de memòria mapejada */
int *p_nblocs;          /* punter al comptador de blocs compartit */
int *p_npilotes;        /* punter al comptador de pilotes compartit */
int final_joc_offset;
int *p_final_joc;

int nblocs_offset;      /* desplaçament del comptador de blocs a memòria compartida */
int npilotes_offset;    /* desplaçament del comptador de pilotes a memòria compartida */

/* variables de poder */
int poder_actiu = 0;

/* Prototipus de funcions */
char comprovar_bloc(int f, int c);
float control_impacte2(int c_pil, float velc0, int c_pal, int m_pal);
int mou_pilota(int c_pal, int m_pal, float* pos_f, float* pos_c, float* vel_f, float* vel_c, char ball_id);

/* * Donada una posició on la pilota ha xocat, comprova si és un bloc de lletres.
 * Si ho és, esborra tot el bloc de la pantalla i redueix el comptador de blocs.
 */
char comprovar_bloc(int f, int c)
{
	int col;
    waitS(id_sem_curses);
	char quin = win_quincar(f, c);
    signalS(id_sem_curses);

    if ((quin == BLKCHAR || quin == FRNTCHAR)) {
        col = c;

        /* Esborrar cap a la dreta fins trobar un espai buit */
        waitS(id_sem_curses);
        while (win_quincar(f, col) != ' ') {
            win_escricar(f, col, ' ', NO_INV);
            col++;
        }
        signalS(id_sem_curses);
        col = c - 1;
        /* Esborrar cap a l'esquerra fins trobar un espai buit */
        waitS(id_sem_curses);
        while (win_quincar(f, col) != ' ') {
            win_escricar(f, col, ' ', NO_INV);
            col--;
        }
        signalS(id_sem_curses);

        /* Només decrementem el comptador si és un bloc trencable (A o B) */
        if ((quin == BLKCHAR || quin == FRNTCHAR) && (p_nblocs != NULL)) {
            waitS(id_sem_memoria);
            (*p_nblocs)--; /* Decrementem el total de blocs pendents */
            signalS(id_sem_memoria);
        }

    }

    if (quin == WLLCHAR && poder_actiu) {
        waitS(id_sem_curses);
        win_escricar(f, c, ' ', NO_INV);
        signalS(id_sem_curses);
    }

    return quin;  /* Retornem el tipus de bloc ('A', 'B', '#', etc.) */
}

/* * Crea un nou procés pilota en la posició indicada amb la velocitat invertida.
 * Retorna 0 si s'ha creat correctament, -1 si hi ha error.
 */
int crear_nova_pilota(int f_bloc, int c_bloc, float vel_f, float vel_c, int retard, char ball_id)
{
    missatge_t msg;

    msg.fila = f_bloc;
    msg.columna = c_bloc;   
    msg.vel_f = -vel_f;
    msg.vel_c = -vel_c;
    msg.retard = retard;
    msg.tipus = TIPUS_NOVA_PILOTA;

    sendM(id_mis, &msg, sizeof(msg));
    return 0;
}

int enviar_increment_poder() {
    missatge_t msg;

    msg.tipus = TIPUS_INCREMENT_PODER;

    sendM(id_mis, &msg, sizeof(msg));
    return 0;
}

/* * Calcula l'efecte de la pilota depenent d'on impacti sobre la paleta.
 * Si pica a les vores, el rebot és més inclinat.
 */
float control_impacte2(int c_pil, float velc0, int c_pal, int m_pal) {
	int distApal;
	float vel_c;

	distApal = c_pil - c_pal;
	if (distApal >= 2*m_pal/3) vel_c = 0.5;
	else if (distApal <= m_pal/3) vel_c = -0.5;
	else if (distApal == m_pal/2) vel_c = 0.0;
	else vel_c = velc0;
	return vel_c;
}

/* * Funció principal de moviment de la pilota.
 * Calcula la següent posició i gestiona els rebots amb parets, blocs i paleta.
 * Retorna 1 si s'ha d'acabar el joc (es guanya o es perd), 0 si s'ha de continuar.
 */
int mou_pilota(int c_pal, int m_pal, float* pos_f, float* pos_c, float* vel_f, float* vel_c, char ball_id)
{
	int f_h, c_h;
	int f_pil, c_pil; //posicio actual de la pilota (enter)
	char rh, rv, rd;
	int fora = 0; /* Booleà: indica si la pilota ha caigut per la porteria */
	char tipus_bloc;  /* Per guardar el tipus de bloc impactat */

	//posicio inicial de la pilota (enter)
	f_pil= (int)*pos_f;
	c_pil= (int)*pos_c;

	/* Calcular següent posició */
	f_h = *pos_f + *vel_f;
	c_h = *pos_c + *vel_c;
	rh = rv = rd = ' ';

	/* Només mirem rebots si canvia la posició visual (enters) de la pilota */
	if ((f_h != f_pil) || (c_h != c_pil)) {

		/* Comprovar rebot vertical (sostre, paleta, o bloc a dalt/baix) */
		if (f_h != f_pil) {
            waitS(id_sem_curses);
            rv = win_quincar(f_h, c_pil);
            signalS(id_sem_curses);
            if (rv != ' ') {
                tipus_bloc = comprovar_bloc(f_h, c_pil);
                /* Si és bloc 'B', crear nova pilota */
                if (tipus_bloc == BLKCHAR) {
                    /* Crear nova pilota a la posició del bloc amb velocitat invertida */
                    crear_nova_pilota(f_h, c_pil, *vel_f, *vel_c, retard, ball_id);
                }
                if (tipus_bloc == FRNTCHAR) {
                    enviar_increment_poder();
                }
                if (rv == '0')
                    *vel_c = control_impacte2(c_pil, *vel_c, c_pal, m_pal);
                *vel_f = -(*vel_f);
                f_h = *pos_f + *vel_f;
                c_h = *pos_c + *vel_c; 
            }
        }

		/* Comprovar rebot horitzontal (parets laterals o costats dels blocs) */
		if (c_h != c_pil) {
			waitS(id_sem_curses);
            rh = win_quincar(f_pil, c_h);
            signalS(id_sem_curses);
            if (rh != ' ') {
                tipus_bloc = comprovar_bloc(f_pil, c_h);

                if (tipus_bloc == BLKCHAR) {
                    crear_nova_pilota(f_pil, c_h, *vel_f, *vel_c, retard, ball_id);
                }
                if (tipus_bloc == FRNTCHAR) {
                    enviar_increment_poder();
                }

                *vel_c = -(*vel_c);
                f_h = *pos_f + *vel_f;
                c_h = *pos_c + *vel_c;
            }
        }

		/* Comprovar rebot diagonal (caires de les estructures) */
		if ((f_h != f_pil) && (c_h != c_pil)) {
            waitS(id_sem_curses);
            rd = win_quincar(f_h, c_h);
            signalS(id_sem_curses);
            if (rd != ' ') {
                tipus_bloc = comprovar_bloc(f_h, c_h);

                if (tipus_bloc == BLKCHAR) {
                    crear_nova_pilota(f_h, c_h, *vel_f, *vel_c, retard, ball_id);
                }
                if (tipus_bloc == FRNTCHAR) {
                    enviar_increment_poder();
                }

                *vel_f = -(*vel_f);
                *vel_c = -(*vel_c);
                f_h = *pos_f + *vel_f;
                c_h = *pos_c + *vel_c;
            }
        }

		/* Si l'espai està lliure, moure la pilota i redibuixar */
		waitS(id_sem_curses);
        rd = win_quincar(f_h, c_h); // reutilitzem rd per no crear una nova variable
        signalS(id_sem_curses);
		if (rd == ' ') {
            waitS(id_sem_curses);
			win_escricar(f_pil, c_pil, ' ', NO_INV);
            signalS(id_sem_curses);
			*pos_f += *vel_f;
			*pos_c += *vel_c;
			f_pil = f_h;
			c_pil = c_h;

			/* Si estem dins del tauler, la pintem. Si passem la línia, s'ha colat */
			if (f_pil != n_fil - 1) {
                waitS(id_sem_curses);
                if (poder_actiu) win_escricar(f_pil, c_pil, ball_id, INVERS);
                else win_escricar(f_pil, c_pil, ball_id, NO_INV);
                signalS(id_sem_curses);
            }
			else {
                fora = 1;
                /* Missatge de sacrifici de pilota */
                missatge_t missatge;
                missatge.tipus = TIPUS_PILOTA_SACRIFICI;
                missatge.vel_f = *vel_f;
                sendM(id_mis, &missatge, sizeof(missatge));

            }
		}
	} else {
		/* Encara que no canviï de quadrat a la pantalla, actualitzem coordenades reals */
		*pos_f += *vel_f;
		*pos_c += *vel_c;
	}

    return fora;  /* La pilota ha sortit */
}

void* bustia_thread(void* arg) {
    missatge_t missatge;

    missatge.tipus = TIPUS_CONTROL;
    sendM(id_mis_thread, &missatge, sizeof(missatge));
    int n;

    while (!fi) {
        n = receiveM(id_mis_thread, &missatge);

        if (n != sizeof(missatge)) {
            continue; // Descartem missatge mal format
        }

        if (missatge.tipus == TIPUS_CONTROL) { 
            // Missatge de control no bloquejant mentre no hi hagi altres missatges
            missatge.tipus = TIPUS_CONTROL;
            sendM(id_mis_thread, &missatge, sizeof(missatge));
        }

        if (missatge.tipus == TIPUS_PODER_ACTIU) {
            pthread_mutex_lock(&mutex);
            poder_actiu = 1;
            pthread_mutex_unlock(&mutex);
        }

        if (missatge.tipus == TIPUS_PODER_NO_ACTIU) {
            pthread_mutex_lock(&mutex);
            poder_actiu = 0;
            pthread_mutex_unlock(&mutex);
        }

        if (missatge.tipus == TIPUS_PILOTA_SACRIFICI) {
            vel_f += missatge.vel_f;
            if (vel_f > MAX_VEL_F) vel_f = MAX_VEL_F;
            if (vel_f < -MAX_VEL_F) vel_f = -MAX_VEL_F;
        }
        win_retard(retard);
    }
    return NULL;

}

/* --- Programa Principal --- */
int main(int n_args, char *ll_args[])
{
    float pos_f, pos_c, vel_c;
    char ball_id;

    /* Comprovació d'arguments */
    if (n_args != 19) {
        fprintf(stderr, "Error: Nombre d'arguments incorrecte\n");
        fprintf(stderr, "Ús: pilota2 id_mem id_sem_curses id_sem_memoria id_mis n_fil n_col m_por c_pal m_pal pos_f pos_c vel_f vel_c ball_id retard nblocs_offset npilotes_offset final_joc_offset\n");
        fprintf(stderr, "Arguments detectats: %d\n", n_args);
        exit(1);
    }

    /* Llegir arguments de la línia de comandes */
    id_mem = atoi(ll_args[1]);
    id_sem_curses = atoi(ll_args[2]);
    id_sem_memoria = atoi(ll_args[3]);
    id_mis = atoi(ll_args[4]);
    n_fil = atoi(ll_args[5]);
    n_col = atoi(ll_args[6]);
    m_por = atoi(ll_args[7]);
    c_pal = atoi(ll_args[8]);
    m_pal = atoi(ll_args[9]);
    pos_f = atof(ll_args[10]);      /* Posició fila inicial de la pilota */
    pos_c = atof(ll_args[11]);     /* Posició columna inicial de la pilota */
    vel_f = atof(ll_args[12]);     /* Velocitat fila */
    vel_c = atof(ll_args[13]);     /* Velocitat columna */
    ball_id = ll_args[14][0];      /* Caràcter identificador de la pilota */
    retard = atoi(ll_args[15]);    /* Retard entre moviments */
    nblocs_offset = atoi(ll_args[16]); /* Offset del nombre de blocs restants */
    npilotes_offset = atoi(ll_args[17]); /* Offset del nombre de pilotes en joc */
    final_joc_offset = atoi(ll_args[18]); /* Offset de la bandera de fi */

    /* Connectar a la memòria compartida */
    p_mem = map_mem(id_mem);
    if (p_mem == NULL) {
        fprintf(stderr, "Error: No s'ha pogut connectar a la memòria compartida\n");
        exit(1);
    }

    /* Inicialitzar punters als comptadors compartits */
    p_nblocs = (int *)((char *)p_mem + nblocs_offset);
    p_npilotes = (int *)((char *)p_mem + npilotes_offset);
    p_final_joc = (int *)((char *)p_mem + final_joc_offset);

    win_set(p_mem, n_fil, n_col);

    
    id_mis_thread = ini_mis();
    if (id_mis_thread == -1) {
        fprintf(stderr, "Error al crear la bústia de thread\n");
        exit(4);
    }

    /* Comunicar al procés principal l'id de la bústia */
    missatge_t missatge;
    missatge.tipus = TIPUS_REGISTRE_PILOTA;
    missatge.id_bustia = id_mis_thread;
    sendM(id_mis, &missatge, sizeof(missatge));

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, bustia_thread, NULL) != 0) {
        fprintf(stderr, "Error al crear el thread de la bústia\n");
        exit(5);
    }

    while (!fi) {
        fi = mou_pilota(c_pal, m_pal, &pos_f, &pos_c, &vel_f, &vel_c, ball_id);
        waitS(id_sem_memoria);
        if (p_final_joc != NULL && *p_final_joc) {
            fi = 1;
        }
        signalS(id_sem_memoria);
        /* Pausa per controlar la velocitat */
        win_retard(retard);
    }

    if (fi) {
        waitS(id_sem_memoria);
        if (p_npilotes != NULL && *p_npilotes > 0) {
            (*p_npilotes)--; /* Decrementem el nombre de pilotes en joc */
        }
        signalS(id_sem_memoria);
    }

    pthread_join(thread_id, NULL);
    pthread_mutex_destroy(&mutex);
    return 0;
}
