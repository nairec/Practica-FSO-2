/*****************************************************************************/
/* */
/* pilota1.c                                                                    */
/* */
/* Programa inicial d'exemple per a les practiques 2 d'FSO.                  */
/* Versió seqüencial adaptada a winsuport2 i memòria compartida IPC.         */
/* */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "winsuport2.h"
#include "memoria.h"

/* --- Definicions de constants --- */
//#define MAX_THREADS	10 [maxim de fils] No fa falta perque a pilota1 no es creen fils (threads), nomes processos
//#define MAXBALLS	(MAX_THREADS-1) [maxim de pilotes] No fa falta perque cada procés pilota nomes controla UNA pilota
//[validacio de dimensions] Les dimensions ja venen validades pel pare mur1
//#define MIN_FIL	10
//#define MAX_FIL	50
//#define MIN_COL	10
//#define MAX_COL	80

/* Constants per a la creació dels blocs del joc */
//#define BLKSIZE	3 [Mida dels blocs en caràcters] No es necessita per al moviment de la pilota, només per crear blocs
//#define BLKGAP	2 [Espai entre blocs] Només per a la creació inicial del taulell
#define BLKCHAR 'B' //[identificar blocs indestructibles en col.lisions]
#define WLLCHAR '#' //[identificar parets indestructibles]
#define FRNTCHAR 'A' //[identificar blocs frontissa (tipus A)]
#define LONGMISS 65 //[Mida del buffer per missatges]
//[controlar l'atribut invers en dibuixar (win_escricar)]
#define NO_INV 0
#define INVERS 1

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
int retard;			    /* valor del retard de moviment, en mil.lisegons */
char strin[LONGMISS];	/* variable per a generar missatges de text a la pantalla */

/* Variables globals per a la memòria compartida (IPC) */
int id_mem;             /* identificador de la memòria compartida creada */
void *p_mem;            /* punter cap a la zona de memòria mapejada */

/* Prototipus de funcions */
char comprovar_bloc(int f, int c);
float control_impacte2(int c_pil, float velc0, int c_pal, int m_pal);
int mou_pilota(int f_pal, int c_pal, int m_pal, float pos_f, float pos_c, float vel_f, float vel_c, char ball_id);

/* * Donada una posició on la pilota ha xocat, comprova si és un bloc de lletres.
 * Si ho és, esborra tot el bloc de la pantalla i redueix el comptador de blocs.
 */
char comprovar_bloc(int f, int c)
{
	int col;
	char quin = win_quincar(f, c);
	char tipus_bloc = ' ';

	/* Comprovar si és un bloc (qualsevol tipus) */
    if (quin == BLKCHAR || quin == FRNTCHAR || quin == 'B') {
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
        if (quin == BLKCHAR || quin == FRNTCHAR) {
            nblocs--; /* Decrementem el total de blocs pendents */
        }
        /* Nota: Si és FRNTCHAR ('A'), no decrementem nblocs perquè no és trencable? */
        /* Segons l'enunciat, 'A' també es trenca, així que cal revisar la definició */
    }

    return tipus_bloc;  /* Retornem el tipus de bloc ('A', 'B', '#', etc.) */
}

/* * Crea un nou procés pilota en la posició indicada amb la velocitat invertida.
 * Retorna 0 si s'ha creat correctament, -1 si hi ha error.
 */
int crear_nova_pilota(int f_bloc, int c_bloc, int c_pal, int m_pal, float vel_f, float vel_c, int retard)
{
    pid_t pid;
    char id_mem_str[20], n_fil_str[20], n_col_str[20], m_por_str[20];
    char f_pal_str[20], c_pal_str[20], m_pal_str[20];
    char pos_f_str[20], pos_c_str[20], vel_f_str[20], vel_c_str[20];
    char retard_str[20], ball_id_str[5];
    static int next_ball_id = 1;  /* Per assignar identificadors únics */

    /* Crear el nou procés */
    pid = fork();

    if (pid == -1) {
        perror("Error en fork per crear nova pilota");
        return -1;
    }

    if (pid == 0) {
        /* Procés fill: executar el nou programa pilota */

        /* Convertir paràmetres a strings */
        sprintf(id_mem_str, "%d", id_mem);
        sprintf(n_fil_str, "%d", n_fil);
        sprintf(n_col_str, "%d", n_col);
        sprintf(m_por_str, "%d", m_por);

        /* La paleta té les mateixes coordenades que la pilota original */
        /* Necessitem obtenir la posició actual de la paleta de la memòria compartida */
        /* Per ara, utilitzem valors fixos; després es llegiran de memòria compartida */
        sprintf(f_pal_str, "%d", n_fil - 2);  /* Fila de la paleta */
        sprintf(c_pal_str, "%d", c_pal);      /* Columna de la paleta */
        sprintf(m_pal_str, "%d", m_pal);      /* Mida de la paleta */

        /* Posició: la mateixa fila del bloc, columna centrada */
        /* La pilota apareix a la posició on era el bloc */
        sprintf(pos_f_str, "%f", (float)f_bloc);
        sprintf(pos_c_str, "%f", (float)c_bloc);

        /* Velocitat: invertim la direcció perquè surti en direcció contrària */
        sprintf(vel_f_str, "%f", -vel_f);
        sprintf(vel_c_str, "%f", -vel_c);

        sprintf(retard_str, "%d", retard);

        /* Identificador de la nova pilota: dígit o lletra */
        if (next_ball_id <= 9) {
            sprintf(ball_id_str, "%d", next_ball_id);
        } else {
            sprintf(ball_id_str, "%c", 'a' + (next_ball_id - 10));
        }
        next_ball_id++;

        /* Executar el programa pilota1 */
        execlp("./pilota1", "pilota1", id_mem_str, n_fil_str, n_col_str, m_por_str, f_pal_str, c_pal_str, m_pal_str, pos_f_str, pos_c_str, vel_f_str, vel_c_str,retard_str, ball_id_str, (char *)NULL);

        /* Si arribem aquí, execlp ha fallat */
        perror("execlp fallida en crear nova pilota");
        exit(1);
    }

    /* Procés pare: continuar amb la pilota actual */
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
int mou_pilota(int f_pal, int c_pal, int m_pal, float pos_f, float pos_c, float vel_f, float vel_c, char ball_id)
{
	int f_h, c_h;
	int f_pil, c_pil; //posicio actual de la pilota (enter)
	char rh, rv, rd;
	int fora = 0; /* Booleà: indica si la pilota ha caigut per la porteria */
	char tipus_bloc;  /* Per guardar el tipus de bloc impactat */

	//posicio inicial de la pilota (enter)
	f_pil= (int)pos_f;
	c_pil= (int)pos_c;

	/* Bucle infinit mentre la pilota estigui en joc */
    while (1) {

		/* Calcular següent posició */
		f_h = pos_f + vel_f;
		c_h = pos_c + vel_c;
		rh = rv = rd = ' ';

		/* Només mirem rebots si canvia la posició visual (enters) de la pilota */
		if ((f_h != f_pil) || (c_h != c_pil)) {

			/* Comprovar rebot vertical (sostre, paleta, o bloc a dalt/baix) */
			if (f_h != f_pil) {
                rv = win_quincar(f_h, c_pil);
                if (rv != ' ') {
                    tipus_bloc = comprovar_bloc(f_h, c_pil);

                    /* Si és bloc 'B', crear nova pilota */
                    if (tipus_bloc == BLKCHAR) {
                        /* Crear nova pilota a la posició del bloc amb velocitat invertida */
                        crear_nova_pilota(f_h, c_pil, c_pal, m_pal, vel_f, vel_c, retard);
                    }

                    if (rv == '0')
                        vel_c = control_impacte2(c_pil, vel_c, c_pal, m_pal);
                    vel_f = -vel_f;
                    f_h = pos_f + vel_f;
                }
            }

			/* Comprovar rebot horitzontal (parets laterals o costats dels blocs) */
			if (c_h != c_pil) {
                rh = win_quincar(f_pil, c_h);
                if (rh != ' ') {
                    tipus_bloc = comprovar_bloc(f_pil, c_h);

                    if (tipus_bloc == BLKCHAR) {
                        crear_nova_pilota(f_pil, c_h, c_pal, m_pal, vel_f, vel_c, retard);
                    }

                    vel_c = -vel_c;
                    c_h = pos_c + vel_c;
                }
            }

			/* Comprovar rebot diagonal (caires de les estructures) */
			if ((f_h != f_pil) && (c_h != c_pil)) {
                rd = win_quincar(f_h, c_h);
                if (rd != ' ') {
                    tipus_bloc = comprovar_bloc(f_h, c_h);

                    if (tipus_bloc == BLKCHAR) {
                        crear_nova_pilota(f_h, c_h, c_pal, m_pal, vel_f, vel_c, retard);
                    }

                    vel_f = -vel_f;
                    vel_c = -vel_c;
                    f_h = pos_f + vel_f;
                    c_h = pos_c + vel_c;
                }
            }

			/* Si l'espai està lliure, moure la pilota i redibuixar */
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
		} else {
			/* Encara que no canviï de quadrat a la pantalla, actualitzem coordenades reals */
			pos_f += vel_f;
			pos_c += vel_c;
		}

		/* Si la pilota ha sortit, sortir del bucle */
        if (fora) break;

        /* Pausa per controlar la velocitat */
        win_retard(retard);
    }

    /* Netejar la pilota de la pantalla abans de sortir */
    win_escricar(f_pil, c_pil, ' ', NO_INV);
    return 1;  /* La pilota ha sortit */
}

/* --- Programa Principal --- */
int main(int n_args, char *ll_args[])
{
	int f_pal, c_pal, m_pal;
    float pos_f, pos_c, vel_f, vel_c;
    char ball_id;

    /* Comprovació d'arguments: esperem 13 arguments */
    /* Format: id_mem n_fil n_col m_por f_pal c_pal m_pal pos_f pos_c vel_f vel_c ball_id retard */
    if (n_args != 14) {
        fprintf(stderr, "Error: Nombre d'arguments incorrecte\n");
        fprintf(stderr, "Ús: pilota1 id_mem n_fil n_col m_por f_pal c_pal m_pal pos_f pos_c vel_f vel_c ball_id retard\n");
        fprintf(stderr, "Arguments detectats: %d", n_args);
        exit(1);
    }

    /* Llegir arguments de la línia de comandes */
    id_mem = atoi(ll_args[1]);
    n_fil = atoi(ll_args[2]);
    n_col = atoi(ll_args[3]);
    m_por = atoi(ll_args[4]);
    f_pal = atoi(ll_args[5]);      /* Fila de la paleta (sempre n_fil-2) */
    c_pal = atoi(ll_args[6]);      /* Columna inicial de la paleta */
    m_pal = atoi(ll_args[7]);      /* Mida de la paleta */
    pos_f = atof(ll_args[8]);      /* Posició fila inicial de la pilota */
    pos_c = atof(ll_args[9]);      /* Posició columna inicial de la pilota */
    vel_f = atof(ll_args[10]);     /* Velocitat fila */
    vel_c = atof(ll_args[11]);     /* Velocitat columna */
    ball_id = ll_args[12][0];      /* Caràcter identificador de la pilota */
    retard = atoi(ll_args[13]);    /* Retard entre moviments */

    /* Connectar a la memòria compartida */
    p_mem = map_mem(id_mem);
    if (p_mem == NULL) {
        fprintf(stderr, "Error: No s'ha pogut connectar a la memòria compartida\n");
        exit(1);
    }

    /* Inicialitzar la biblioteca winsuport2 amb el punter de memòria */
    win_set(p_mem, n_fil, n_col);

    /* Obtenir el nombre de blocs de la memòria compartida */
    /* NOTA: nblocs hauria d'estar a la memòria compartida. Per ara, el llegim del pare */
    /* En aquesta fase inicial, nblocs es passa per argument? */
    nblocs = 10;  /* Temporal: després es llegirà de memòria compartida */

    /* Bucle principal: moure la pilota fins que surti */
    mou_pilota(f_pal, c_pal, m_pal, pos_f, pos_c, vel_f, vel_c, ball_id);

    /* Alliberar recursos (no cal win_fi() perquè només el pare ho fa) */
    /* Nota: no es fa elim_mem() perquè el pare és qui gestiona la memòria */

	return (0);
}
