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
#include "winsuport2.h"
#include "memoria.h"
#include "missatge.h"
#include "semafor.h"

/* --- Definicions de constants --- */
//#define MAX_THREADS	10 [maxim de fils] No fa falta perque a pilota2 no es creen fils (threads), només processos
//#define MAXBALLS	(MAX_THREADS-1) [maxim de pilotes] No fa falta perque cada procés pilota nomes controla UNA pilota
//[validacio de dimensions] Les dimensions ja venen validades pel pare mur2
//#define MIN_FIL	10
//#define MAX_FIL	50
//#define MIN_COL	10
//#define MAX_COL	80

/* Constants per a la creació dels blocs del joc */
//#define BLKSIZE	3 [Mida dels blocs en caràcters] No es necessita per al moviment de la pilota, només per crear blocs
//#define BLKGAP	2 [Espai entre blocs] Només per a la creació inicial del taulell
#define BLKCHAR 'B' // identificar blocs indestructibles en col.lisions
#define WLLCHAR '#' // identificar parets indestructibles
#define FRNTCHAR 'A' // identificar blocs frontissa (tipus A)
#define LONGMISS 65 // Mida del buffer per missatges
//controlar l'atribut invers en dibuixar (win_escricar)
#define NO_INV 0
#define INVERS 1

/* Constants per enviar missatges */
#define TIPUS_CONTROL 1
#define TIPUS_NOVA_PILOTA 2

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

/* Variables de l'entorn de joc (llegides des dels arguments) */
int n_fil, n_col;		/* dimensions del camp de joc */
int m_por;			    /* mida de la porteria (en caracters) */
int nblocs;         /* nombre de blocs restants per trencar */
int n_pal;      /* nombre de paletes en joc */
int retard;			    /* valor del retard de moviment, en mil.lisegons */
char strin[LONGMISS];	/* variable per a generar missatges de text a la pantalla */

/* Variables globals per a la memòria compartida (IPC) i semàfors */
int id_mem;             /* identificador de la memòria compartida creada */
int id_sem_curses;      /* identificador del semàfor de curses */
int id_sem_memoria;     /* identificador del semàfor de memòria */
int id_mis;

void *p_mem;            /* punter cap a la zona de memòria mapejada */
int *p_nblocs;          /* punter al comptador de blocs compartit */
int *p_npilotes;        /* punter al comptador de pilotes compartit */
paleta_t *p_paletes;    /* punter al vector de paletes a memòria compartida */

int nblocs_offset;      /* desplaçament del comptador de blocs a memòria compartida */
int npilotes_offset;    /* desplaçament del comptador de pilotes a memòria compartida */
int paletes_offset;     /* desplaçament del vector de paletes a memòria compartida */

/* Prototipus de funcions */
char comprovar_bloc(int f, int c);
float control_impacte2(int c_pil, float velc0, int c_pal, int m_pal);
int mou_pilota(float pos_f, float pos_c, float vel_f, float vel_c, char ball_id);

/* * Donada una posició on la pilota ha xocat, comprova si és un bloc de lletres.
 * Si ho és, esborra tot el bloc de la pantalla i redueix el comptador de blocs.
 */
char comprovar_bloc(int f, int c)
{
	int col;
	char quin = win_quincar(f, c);
	char tipus_bloc = ' ';

    if ((quin == BLKCHAR || quin == FRNTCHAR)) {
        tipus_bloc = quin;  /* Guardem el tipus abans d'esborrar */
        col = c;

        /* Esborrar cap a la dreta fins trobar un espai buit */
        while (win_quincar(f, col) != ' ') {
            win_escricar(f, col, ' ', NO_INV);
            col++;
        }
        col = c - 1;
        /* Esborrar cap a l'esquerra fins trobar un espai buit */
        while (win_quincar(f, col) != ' ') {
            win_escricar(f, col, ' ', NO_INV);
            col--;
        }

        /* Només decrementem el comptador si és un bloc trencable (A o B) */
        if ((quin == BLKCHAR || quin == FRNTCHAR) && (p_nblocs != NULL)) {
            (*p_nblocs)--; /* Decrementem el total de blocs pendents */
        }
    }

    return tipus_bloc;  /* Retornem el tipus de bloc ('A', 'B', '#', etc.) */
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
int mou_pilota(float pos_f, float pos_c, float vel_f, float vel_c, char ball_id)
{
	int f_h, c_h;
	int f_pil, c_pil; //posicio actual de la pilota (enter)
	char rh, rv, rd;
	int fora = 0; /* Booleà: indica si la pilota ha caigut per la porteria */
	char tipus_bloc;  /* Per guardar el tipus de bloc impactat */
    char ha_sortit = 0;

	//posicio inicial de la pilota (enter)
	f_pil= (int)pos_f;
	c_pil= (int)pos_c;

	/* Bucle infinit mentre la pilota estigui en joc */
    while (!ha_sortit) {

		/* Calcular següent posició */
		f_h = pos_f + vel_f;
		c_h = pos_c + vel_c;
		rh = rv = rd = ' ';

		/* Només mirem rebots si canvia la posició visual (enters) de la pilota */
		if ((f_h != f_pil) || (c_h != c_pil)) {

			/* Comprovar rebot vertical (sostre, paleta, o bloc a dalt/baix) */
			if (f_h != f_pil) {
               	waitS(id_sem_curses);
                rv = win_quincar(f_h, c_pil);
                if (rv != ' ') {
                    waitS(id_sem_memoria);
                    tipus_bloc = comprovar_bloc(f_h, c_pil);
                    signalS(id_sem_memoria);
                    /* Si és bloc 'B', crear nova pilota */
                    if (tipus_bloc == BLKCHAR) {
                        /* Crear nova pilota a la posició del bloc amb velocitat invertida */
                        crear_nova_pilota(f_h, c_pil, vel_f, vel_c, retard, ball_id);
                    }

                    if (rv == '0'){
                        /* veure paleta amb la què impacta */
                        char paleta_trobada = 0;
                        int i = 0;
                        waitS(id_sem_memoria);
                        while (!paleta_trobada && i < n_pal) {
                            if (p_paletes[i].fila == f_h && c_pil >= p_paletes[i].col_actual && c_pil < p_paletes[i].col_actual + p_paletes[i].amplada) {
                                vel_c = control_impacte2(c_pil, vel_c, p_paletes[i].col_actual, p_paletes[i].amplada);
                                paleta_trobada = 1;
                            }
                            i++;
                        }
                        signalS(id_sem_memoria);
                    
                    }
                    vel_f = -vel_f;
                    f_h = pos_f + vel_f;
                }
                signalS(id_sem_curses);
            }

			/* Comprovar rebot horitzontal (parets laterals o costats dels blocs) */
			if (c_h != c_pil) {
			    waitS(id_sem_curses);
                rh = win_quincar(f_pil, c_h);
                if (rh != ' ') {
                    waitS(id_sem_memoria);
                    tipus_bloc = comprovar_bloc(f_pil, c_h);
                    signalS(id_sem_memoria);

                    if (tipus_bloc == BLKCHAR) {
                        crear_nova_pilota(f_pil, c_h, vel_f, vel_c, retard, ball_id);
                    }

                    vel_c = -vel_c;
                    c_h = pos_c + vel_c;
                }
                signalS(id_sem_curses);
            }

			/* Comprovar rebot diagonal (caires de les estructures) */
			if ((f_h != f_pil) && (c_h != c_pil)) {
                waitS(id_sem_curses);
                rd = win_quincar(f_h, c_h);
                if (rd != ' ') {
                    waitS(id_sem_memoria);
                    tipus_bloc = comprovar_bloc(f_h, c_h);
                    signalS(id_sem_memoria);

                    if (tipus_bloc == BLKCHAR) {
                        crear_nova_pilota(f_h, c_h, vel_f, vel_c, retard, ball_id);
                    }

                    vel_f = -vel_f;
                    vel_c = -vel_c;
                    f_h = pos_f + vel_f;
                    c_h = pos_c + vel_c;
                }
                signalS(id_sem_curses);
            }

			/* Si l'espai està lliure, moure la pilota i redibuixar */
			waitS(id_sem_curses);
			if (win_quincar(f_h, c_h) == ' ') {
				win_escricar(f_pil, c_pil, ' ', NO_INV);
				pos_f += vel_f;
				pos_c += vel_c;
				f_pil = f_h;
				c_pil = c_h;

				/* Si estem dins del tauler, la pintem. Si passem la línia, s'ha colat */
				if (f_pil != n_fil - 1) win_escricar(f_pil, c_pil, ball_id, INVERS);
				else fora = 1;
			}
			signalS(id_sem_curses);
		} else {
			/* Encara que no canviï de quadrat a la pantalla, actualitzem coordenades reals */
			pos_f += vel_f;
			pos_c += vel_c;
		}

		/* Si la pilota ha sortit, sortir del bucle */
        if (fora) {
            waitS(id_sem_memoria);
            if (p_npilotes != NULL && *p_npilotes > 0) {
                (*p_npilotes)--; /* Decrementem el nombre de pilotes en joc */
            }
            signalS(id_sem_memoria);
            ha_sortit = 1;
        };

        /* Pausa per controlar la velocitat */
        win_retard(retard);
    }

    waitS(id_sem_curses);
    /* Netejar la pilota de la pantalla abans de sortir */
    win_escricar(f_pil, c_pil, ' ', NO_INV);
    signalS(id_sem_curses);
    return 1;  /* La pilota ha sortit */
}

/* --- Programa Principal --- */
int main(int n_args, char *ll_args[])
{
    float pos_f, pos_c, vel_f, vel_c;
    char ball_id;

    /* Comprovació d'arguments: esperem 17 arguments + nom del programa */
    /* Format: id_mem id_sem_curses id_sem_memoria id_mis n_fil n_col n_pal m_por pos_f pos_c vel_f vel_c ball_id retard nblocs_offset paletes_offset */
    if (n_args != 18) {
        fprintf(stderr, "Error: Nombre d'arguments incorrecte\n");
        fprintf(stderr, "Ús: pilota2 id_mem id_sem_curses id_sem_memoria id_mis n_fil n_col n_pal m_por pos_f pos_c vel_f vel_c ball_id retard nblocs_offset paletes_offset\n");
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
    n_pal = atoi(ll_args[7]);
    m_por = atoi(ll_args[8]);
    pos_f = atof(ll_args[9]);      /* Posició fila inicial de la pilota */
    pos_c = atof(ll_args[10]);     /* Posició columna inicial de la pilota */
    vel_f = atof(ll_args[11]);     /* Velocitat fila */
    vel_c = atof(ll_args[12]);     /* Velocitat columna */
    ball_id = ll_args[13][0];      /* Caràcter identificador de la pilota */
    retard = atoi(ll_args[14]);    /* Retard entre moviments */
    nblocs_offset = atoi(ll_args[15]); /* Offset del nombre de blocs restants */
    npilotes_offset = atoi(ll_args[16]); /* Offset del nombre de pilotes en joc */
    paletes_offset = atoi(ll_args[17]); /* Offset del nombre de paletes en joc */

    /* Connectar a la memòria compartida */
    p_mem = map_mem(id_mem);
    if (p_mem == NULL) {
        fprintf(stderr, "Error: No s'ha pogut connectar a la memòria compartida\n");
        exit(1);
    }

    /* Inicialitzar punters als comptadors compartits */
    p_nblocs = (int *)((char *)p_mem + nblocs_offset);
    p_npilotes = (int *)((char *)p_mem + npilotes_offset);
    p_paletes = (paleta_t *)((char *)p_mem + paletes_offset);

    win_set(p_mem, n_fil, n_col);

    mou_pilota(pos_f, pos_c, vel_f, vel_c, ball_id);
    return 0;
}
