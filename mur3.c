/*****************************************************************************/
/* */
/* mur3.c                                                                    */
/* */
/* Programa inicial d'exemple per a les practiques 2 d'FSO.                  */
/* Versió seqüencial adaptada a winsuport2 i memòria compartida IPC.         */
/* */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "winsuport2.h"
#include "memoria.h"
#include "semafor.h"
#include "missatge.h"

/* --- Definicions de constants --- */
#define MAX_THREADS	10
#define MAXBALLS	(MAX_THREADS-1)
#define MIN_FIL	10
#define MAX_FIL	50
#define MIN_COL	10
#define MAX_COL	80

/* Constants per a la creació dels blocs del joc */
#define BLKSIZE	3
#define BLKGAP	2
#define BLKCHAR 'B'
#define WLLCHAR '#'
#define FRNTCHAR 'A'
#define LONGMISS	65∫

/* COnstants per enviar missatges */
#define TIPUS_CONTROL 1
#define TIPUS_NOVA_PILOTA 2

/* Struct de tipus Paleta */
typedef struct {
	int fila;
	int col_inicial;
	int col_actual;
	int dir_lateral;
	int dir_vertical;
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
/* Variables de l'entorn de joc */
int n_fil, n_col;		/* dimensions del camp de joc */
int m_por;			    /* mida de la porteria (en caracters) */
int nblocs = 0;         /* nombre de blocs restants per trencar */
int retard;			    /* valor del retard de moviment, en mil.lisegons */
char strin[LONGMISS];	/* variable per a generar missatges de text a la pantalla */

/* Llista de paletes i comptador */
paleta_t paletes[MAX_THREADS];
int n_paletes = 0;

/* Variables de la pilota */
int ball_id;
char id_char;
int f_pil, c_pil;		/* posicio de la pilota, en valor enter (per pintar a pantalla) */
float pos_f, pos_c;		/* posicio real de la pilota, en valor real (per a moviments suaus) */
float vel_f, vel_c;		/* velocitat de la pilota (components horitzontal i vertical) */

/* Variables globals per a la memòria compartida (IPC) i semàfors */
int id_mem;             /* identificador de la memòria compartida creada */
int id_sem;             /* identificador del semàfor */
int id_mis;				/* identificador de la bustia */
void *p_mem;            /* punter cap a la zona de memòria mapejada */
int nblocs_offset;       /* offset dins de la memòria compartida on es guarda el nombre de blocs restants */
int npilotes_offset;	  /* offset dins de la memòria compartida on es guarda el nombre de pilotes en joc */
int paletes_offset; 	/* offset dins de la memòria compartida on es guarda la informació de les paletes */
int *p_npilotes; 		/* punter al comptador de pilotes compartit */
int *p_nblocs;          /* punter al comptador de blocs compartit */

/* Variables de temps */
int milisegons = 0, segons = 0, minuts = 0;

/* Variables per a la conversió de valors a cadenes */
char id_mem_s[20], id_sem_s[20], id_mis_s[20],n_fil_s[20], n_col_s[20], m_por_s[20], pos_f_s[20], pos_c_s[20], vel_f_s[20], vel_c_s[20], ball_id_s[20], retard_s[20], nblocs_offset_s[20], npilotes_offset_s[20], paletes_offset_s[20];

/* * Llegeix els paràmetres del joc des d'un fitxer de text.
 * Retorna 0 si tot va bé, o un codi d'error (1-5) si algun paràmetre és incorrecte.
 */
int carrega_configuracio(FILE * fit)
{
	int ret = 0;
	int i = 0;

	fscanf(fit, "%d %d %d\n", &n_fil, &n_col, &m_por);
	fscanf(fit, "%f %f %f %f\n", &pos_f, &pos_c, &vel_f, &vel_c);

	/* Llegim files començant amb el caràcter P per inicialitzar les paletes*/
	while (i < MAX_THREADS && fscanf(fit, "P %d %d %d %d\n", 
        &paletes[i].fila, &paletes[i].col_inicial, &paletes[i].dir_lateral, &paletes[i].mida) == 4) {
        paletes[i].id = i + 1;  // ID automátic
        i++;
    }
	n_paletes = i;

    /* Validació de les dimensions i posicions per evitar errors gràfics */
	if ((n_fil != 0) || (n_col != 0)) {
		if ((n_fil < MIN_FIL) || (n_fil > MAX_FIL) || (n_col < MIN_COL) || (n_col > MAX_COL))
			ret = 1;
		else if (m_por > n_col - 3)
			ret = 2;
		else if ((pos_f < 1) || (pos_f >= n_fil - 3) || (pos_c < 1) || (pos_c > n_col - 1))
			ret = 3;
	}
	if ((vel_f < -1.0) || (vel_f > 1.0) || (vel_c < -1.0) || (vel_c > 1.0))
		ret = 4;

	for (int j = 0; j < n_paletes; j++) {
		if ((paletes[j].amplada < 1) || (paletes[j].amplada >= n_col - 3) || (paletes[j].col_inicial < 1) || (paletes[j].col_inicial + paletes[j].amplada > n_col - 1))
			ret = 5;
			break;
	}

	if (ret != 0) {
		fprintf(stderr, "Error en fitxer de configuracio:\n");
		switch (ret) {
		case 1: fprintf(stderr, "\tdimensions del camp de joc incorrectes\n"); break;
		case 2: fprintf(stderr, "\tmida de la porteria incorrecta\n"); break;
		case 3: fprintf(stderr, "\tposicio de la pilota incorrecta\n"); break;
		case 4: fprintf(stderr, "\tvelocitat de la pilota incorrecta\n"); break;
		case 5: fprintf(stderr, "\tposicio o mida de la paleta incorrectes\n"); break;
		}
	}
	fclose(fit);
	return (ret);
}

/* * Prepara la memòria compartida, dibuixa els elements del joc (murs, blocs,
 * paleta, pilota) i inicialitza les variables automàtiques si s'escau.
 */
int inicialitza_joc(void)
{
	int i, mida_mem;
	int i_port, f_port;
	int c, nb, offset;

    /* win_ini retorna la mida necessària de memòria per la configuració actual */
	mida_mem = win_ini(&n_fil, &n_col, '+', INVERS);

	if (mida_mem < 0) {
		fprintf(stderr, "Error en la creacio del taulell de joc.\n");
		return (mida_mem);
	}

	nblocs_offset = mida_mem; // reservem espai després del buffer de la pantalla
	npilotes_offset = mida_mem + sizeof(int); // reservem espai per un int més (npilotes)
	paletes_offset = mida_mem + 2*sizeof(int); // reservem espai per un int més (paletes)
	mida_mem += sizeof(int); // sumem 4 bytes per nblocs
	mida_mem += sizeof(int); // sumem 4 bytes per npilotes
	mida_mem += sizeof(paleta_t) * n_paletes; // sumem espai per les paletes
	
	/* Creació i assignació de la memòria compartida per a la pantalla */
	id_mem = ini_mem(mida_mem);
	p_mem = map_mem(id_mem);
	win_set(p_mem, n_fil, n_col);

	p_nblocs = (int *)((char *)p_mem + nblocs_offset);
	*p_nblocs = 0;

	p_npilotes = (int *)((char *)p_mem + npilotes_offset);
	*p_npilotes = 0;

	paleta_t *p_paletes = (paleta_t *)((char *)p_mem + paletes_offset);
	for (i = 0; i < n_paletes; i++) {
		p_paletes[i] = paletes[i]; // Copiem la informació de les paletes a la memòria compartida
		p_paletes[i].col_actual = paletes[i].col_inicial; // Inicialitzem la columna actual
		p_paletes[i].dir_vertical = 0; // Inicialitzem la direcció vertical	
	}



    /* Càlcul de la porteria inferior */
	if (m_por > n_col - 2) m_por = n_col - 2;
	if (m_por == 0) m_por = 3 * (n_col - 2) / 4;
	i_port = n_col / 2 - m_por / 2 - 1;
	f_port = i_port + m_por - 1;
	for (i = i_port; i <= f_port; i++)
		win_escricar(n_fil - 2, i, ' ', NO_INV);

	n_fil = n_fil - 1;
	f_pal = n_fil - 2;

	
	/* Ubicar i dibuixar les paletes */
	for (int p = 0; p < n_paletes; p++) {
		for (int i = 0; i < paletes[p].mida; i++) {
			win_escricar(paletes[p].fila, paletes[p].col_inicial + i, '0', INVERS);
		}
	}

    /* Ubicar i dibuixar la pilota a la posició inicial */
	if (pos_f > n_fil - 1) pos_f = n_fil - 1;
	if (pos_c > n_col - 1) pos_c = n_col - 1;
	f_pil = pos_f;
	c_pil = pos_c;
	win_escricar(f_pil, c_pil, '1', INVERS);

    /* Generació dels blocs a destruir (files 3, 4 i 5) */
	nb = 0;
	*p_nblocs = n_col / (BLKSIZE + BLKGAP) - 1;
	offset = (n_col - *p_nblocs * (BLKSIZE + BLKGAP) + BLKGAP) / 2;
	for (i = 0; i < *p_nblocs; i++) {
		for (c = 0; c < BLKSIZE; c++) {
			win_escricar(3, offset + c, FRNTCHAR, INVERS);
			nb++;
			win_escricar(4, offset + c, BLKCHAR, NO_INV);
			nb++;
			win_escricar(5, offset + c, FRNTCHAR, INVERS);
			nb++;
		}
		offset += BLKSIZE + BLKGAP;
	}
	*p_nblocs = nb / BLKSIZE;

    /* Generació de les defenses indestructibles (fila 6) */
	nb = n_col / (BLKSIZE + 2 * BLKGAP) - 2;
	offset = (n_col - nb * (BLKSIZE + 2 * BLKGAP) + BLKGAP) / 2;
	for (i = 0; i < nb; i++) {
		for (c = 0; c < BLKSIZE + BLKGAP; c++) {
			win_escricar(6, offset + c, WLLCHAR, NO_INV);
		}
		offset += BLKSIZE + 2 * BLKGAP;
	}

	sprintf(strin, "Tecles: \'%c\'-> Esquerra, \'%c\'-> Dreta, RETURN-> sortir\n", TEC_DRETA, TEC_ESQUER);
	win_escristr(strin);
	return (0);
}

/* * Mostra el missatge final de partida a la línia d'estat i espera a que
 * l'usuari premi una tecla per tancar l'aplicació.
 */
void mostra_final(char *miss)
{
	int lmarge;
	char marge[LONGMISS];

    /* Centra el text calculant el marge necessari */
	lmarge=(n_col+strlen(miss))/2;
	sprintf(marge,"%%%ds",lmarge);

	sprintf(strin, marge,miss);
	win_escristr(strin);
	win_update();
	getchar();
}

/* * Captura les entrades del teclat de l'usuari i desplaça la paleta.
 * Retorna 1 si es prem la tecla RETURN per abandonar la partida.
 */
int mou_paleta(void)
{
	int tecla, result;
	result = 0;
	tecla = win_gettec();
	if (tecla != 0) {
		if ((tecla == TEC_DRETA) && ((c_pal + m_pal) < n_col - 1)) {
		    	waitS(id_sem);
                /* Esborrar l'extrem esquerre i pintar el nou extrem dret */
				win_escricar(f_pal, c_pal, ' ', NO_INV);
				c_pal++;
				win_escricar(f_pal, c_pal + m_pal - 1, '0', INVERS);
				signalS(id_sem);
		}
		if ((tecla == TEC_ESQUER) && (c_pal > 1)) {
		    	waitS(id_sem);
                /* Esborrar l'extrem dret i pintar el nou extrem esquerre */
				win_escricar(f_pal, c_pal + m_pal - 1, ' ', NO_INV);
				c_pal--;
				win_escricar(f_pal, c_pal, '0', INVERS);
				signalS(id_sem);
		}
		if (tecla == TEC_RETURN) result = 1; /* L'usuari vol sortir */
		dirPaleta = tecla;
	}
	return (result);
}

void actualitza_temps(void)
{
	milisegons = milisegons + retard;
	if (milisegons >= 1000) {
		milisegons -= 1000;
		segons++;
		if (segons >= 60) {
			segons = 0;
			minuts++;
		}
	}
	char temps[20];
	sprintf(temps, "%02d:%02d", minuts, segons);
	waitS(id_sem);
	win_escristr(temps);
	signalS(id_sem);
}

static char id_pilota_visible(int id)
{
	/* Evitem '0', 'A' i 'B' perquè tenen significat especial al taulell */
	if (id < 9) return (char)('1' + id);
	return (char)('C' + ((id - 9) % ('Z' - 'C' + 1)));
}

void processa_bustia_no_blocant(void) {
	missatge_t missatge;
	int n;

	missatge.tipus = TIPUS_CONTROL;
	
	sendM(id_mis, &missatge, sizeof(missatge));

	while(1) {
		n = receiveM(id_mis, &missatge);

		if (n != sizeof(missatge)) {
			continue; // Descartem missatge mal format
		}

		if (missatge.tipus == TIPUS_CONTROL) {
			break; // Fi de la cua de missatges
		}

		if (missatge.tipus == TIPUS_NOVA_PILOTA) {
			if (ball_id >= MAXBALLS) {
				continue; /* No creem més processos del límit previst */
			}
			// Processar nova pilota
			id_char = id_pilota_visible(ball_id);

			sprintf(id_mem_s, "%d", id_mem);
			sprintf(id_sem_s, "%d", id_sem);
			sprintf(n_fil_s, "%d", n_fil);
			sprintf(n_col_s, "%d", n_col);
			sprintf(m_por_s, "%d", m_por);
			sprintf(c_pal_s, "%d", missatge.c_pal);
			sprintf(m_pal_s, "%d", m_pal);
			sprintf(f_pal_s, "%d", n_fil - 2);
			sprintf(pos_f_s, "%f", (float)missatge.fila);
			sprintf(pos_c_s, "%f", (float)missatge.columna);
			sprintf(vel_f_s, "%f", missatge.vel_f);
			sprintf(vel_c_s, "%f", missatge.vel_c);
			sprintf(ball_id_s, "%c", id_char);
			sprintf(retard_s, "%d", missatge.retard);
			sprintf(nblocs_offset_s, "%d", nblocs_offset);
			sprintf(npilotes_offset_s, "%d", npilotes_offset);

			sprintf(id_mis_s, "%d", id_mis);

			pid_t pid = fork();
			if (pid == 0)
			{
				/* Execució de ./pilota1 passant id_mem, posició i velocitat per argv */
				execlp("./pilota2", "pilota2", id_mem_s, id_sem_s, id_mis_s, n_fil_s, n_col_s, m_por_s, f_pal_s, c_pal_s, m_pal_s, pos_f_s, pos_c_s, vel_f_s, vel_c_s, ball_id_s, retard_s, nblocs_offset_s, npilotes_offset_s, (char *)NULL);
				exit(1);
			}
			if (pid > 0) { // sumem npilotes i augmentem l'id per la seguent pilota
				waitS(id_sem);
				(*p_npilotes)++;
				signalS(id_sem);
				ball_id++;
			}
		}
	}
}

/* --- Programa Principal --- */
int main(int n_args, char *ll_args[])
{
	int i, fi1 = 0, fi2 = 0;
	char missatge_final[50];
	ball_id = 0;
	FILE *fit_conf;
	
    /* 1. Comprovació d'arguments d'entrada */
	if ((n_args != 2) && (n_args != 3)) {
		i = 0;
		do fprintf(stderr, "%s", descripcio[i++]);
		while (descripcio[i][0] != '*');
		exit(1);
	}
	
	fit_conf = fopen(ll_args[1], "rt");
	if (!fit_conf) {
		fprintf(stderr, "Error: no s'ha pogut obrir el fitxer \'%s\'\n", ll_args[1]);
		exit(2);
	}
	
    /* 2. Càrrega del fitxer i configuració del retard del joc */
	if (carrega_configuracio(fit_conf) != 0) exit(3);
	
	if (n_args == 3) {
		retard = atoi(ll_args[2]);
		if (retard < 10) retard = 10;
		if (retard > 1000) retard = 1000;
	} else retard = 100;
	
	printf("Joc del Mur: prem RETURN per continuar:\n");
	getchar();
	
	/* 3. Inicialitzem el semàfor */
	id_sem = ini_sem(1);
	
	/*3.1 Inicialitzem la bústia */
	id_mis = ini_mis();
    if (id_mis == -1) {
		fprintf(stderr, "Error al crear la bústia\n");
        exit(4);
    }
	/* 3.2 Inicialització de memòria compartida i curses */
	if (inicialitza_joc() != 0) exit(4);


	/* Inicialització de punter a comptador de blocs*/
	p_nblocs = (int *)((char *)p_mem + nblocs_offset);
	/* Preparar arguments per passar a pilota2 */
	id_char = id_pilota_visible(ball_id);
	
    sprintf(id_mem_s, "%d", id_mem);
    sprintf(id_sem_s, "%d", id_sem);
    sprintf(id_mis_s, "%d", id_mis);
    sprintf(n_fil_s, "%d", n_fil);
    sprintf(n_col_s, "%d", n_col);
	sprintf(m_por_s, "%d", m_por);
	sprintf(f_pal_s, "%d", f_pal);
	sprintf(c_pal_s, "%d", c_pal);
	sprintf(m_pal_s, "%d", m_pal);
    sprintf(pos_f_s, "%f", pos_f);
    sprintf(pos_c_s, "%f", pos_c);
    sprintf(vel_f_s, "%f", vel_f);
    sprintf(vel_c_s, "%f", vel_c);
	sprintf(ball_id_s, "%c", id_char);
    sprintf(retard_s, "%d", retard);
	sprintf(nblocs_offset_s, "%d", nblocs_offset);
	sprintf(npilotes_offset_s, "%d", npilotes_offset);
	sprintf(paletes_offset_s, "%d", paletes_offset);

	/* Creació dels threads per a les paletes */
	for (int i = 0; i < n_paletes; i++) {
		pthread_create(&p_paletes[i].thread_id, NULL, mou_paleta_thread, &p_paletes[i]);
	}
	
	/* 4. Creació del procés fill per a la pilota */
	pid_t pid = fork();
	if (pid == 0)
	{
		/* Execució de ./pilota1 passant id_mem, posició i velocitat per argv */
		execlp("./pilota2", "pilota2", id_mem_s, id_sem_s, id_mis_s, n_fil_s, n_col_s, m_por_s, f_pal_s, c_pal_s, m_pal_s, pos_f_s, pos_c_s, vel_f_s, vel_c_s, ball_id_s, retard_s, nblocs_offset_s, npilotes_offset_s, paletes_offset_s, (char *)NULL);
		exit(1);
	}
	if (pid > 0) { // sumem npilotes i augmentem l'id per la seguent pilota
		waitS(id_sem);
		(*p_npilotes)++;
		signalS(id_sem);
		ball_id++;
	}

	do
	{
		/* 5. Bucle de gestió (Pare) */
		processa_bustia_no_blocant();
		/* Gestió del teclat */
		/* Control de minuts:segons */
		/* Refresc visual (propi de winsuport2) */
		fi1 = mou_paleta(); actualitza_temps(); win_update(); win_retard(retard);
		fi2 = (*p_nblocs == 0);
	} while (!fi1 && !fi2 && *p_npilotes > 0);
	sprintf(missatge_final, "Partida finalitzada, temps total: %02d:%02d", minuts, segons);
	mostra_final(missatge_final);
	if (fi2==1) {
		mostra_final("Has guanyat!");
		printf("Has guanyat!\n");
	}
	if (*p_npilotes == 0) {
		mostra_final("Has perdut!");
		printf("Has perdut!\n");
	}

	win_fi();
	elim_mem(id_mem);
	elim_sem(id_sem);
    elim_mis(id_mis);
}