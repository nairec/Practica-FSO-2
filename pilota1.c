/*****************************************************************************/
/* */
/* pilota1.c                                                                 */
/* */
/* Fase 1: Procés independent per al moviment de la pilota.                 */
/* Sense semàfors, sense sincronització completa.                           */
/* */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "winsuport2.h"
#include "memoria.h"

/* --- Definicions de constants --- */
#define BLKCHAR 'B'
#define WLLCHAR '#'
#define FRNTCHAR 'A'
#define NO_INV 0
#define INVERS 1

static char id_pilota_visible(int id)
{
	if (id < 9) return (char)('a' + id);
	return (char)('c' + ((id - 9) % ('z' - 'c' + 1)));
}

/* --- Variables Globals --- */
int n_fil, n_col;
int m_por;
int retard;
int id_mem;
void *p_mem;
int *p_nblocs;
int *p_npilotes;
int *p_final_joc;
int nblocs_offset;
int npilotes_offset;
int final_joc_offset;

/* * Comprova si la pilota ha xocat amb un bloc i l'esborra */
char comprovar_bloc(int f, int c)
{
	int col;
	char quin = win_quincar(f, c);
	char tipus_bloc = ' ';

	if ((quin == BLKCHAR || quin == FRNTCHAR)) {
		tipus_bloc = quin;
		col = c;

		while (win_quincar(f, col) != ' ') {
			win_escricar(f, col, ' ', NO_INV);
			col++;
		}
		col = c - 1;

		while (win_quincar(f, col) != ' ') {
			win_escricar(f, col, ' ', NO_INV);
			col--;
		}

		if (p_nblocs != NULL) {
			(*p_nblocs)--;
		}
	}

	return tipus_bloc;
}

/* * Crea un nou procés pilota */
int crear_nova_pilota(int f_bloc, int c_bloc, float vel_f, float vel_c)
{
	/* Phase1: spawn new process directly (no messages) */
	char id_mem_s[20], n_fil_s[20], n_col_s[20], m_por_s[20];
	char c_pal_s[20], m_pal_s[20], pos_f_s[20], pos_c_s[20];
	char vel_f_s[20], vel_c_s[20], ball_id_s[20], retard_s[20];
	char nblocs_offset_s[20], npilotes_offset_s[20], final_joc_offset_s[20];
	pid_t pid;
	/* assign visible id index based on shared npilotes counter */
	if (p_npilotes != NULL) {
		(*p_npilotes)++;
	}
	int id_index = (p_npilotes != NULL) ? (*p_npilotes - 1) : 0;
	char id_char = id_pilota_visible(id_index);

	sprintf(id_mem_s, "%d", id_mem);
	sprintf(n_fil_s, "%d", n_fil);
	sprintf(n_col_s, "%d", n_col);
	sprintf(m_por_s, "%d", m_por);
	sprintf(c_pal_s, "%d", 0);
	sprintf(m_pal_s, "%d", 0);
	sprintf(pos_f_s, "%f", (float)f_bloc);
	sprintf(pos_c_s, "%f", (float)c_bloc);
	sprintf(vel_f_s, "%f", -vel_f);
	sprintf(vel_c_s, "%f", -vel_c);
	sprintf(ball_id_s, "%c", id_char);
	sprintf(retard_s, "%d", retard);
	sprintf(nblocs_offset_s, "%d", nblocs_offset);
	sprintf(npilotes_offset_s, "%d", npilotes_offset);
	sprintf(final_joc_offset_s, "%d", final_joc_offset);

	pid = fork();
	if (pid == 0) {
		execlp("./pilota1", "pilota1", id_mem_s, n_fil_s, n_col_s, m_por_s,
			   c_pal_s, m_pal_s, pos_f_s, pos_c_s, vel_f_s, vel_c_s,
			   ball_id_s, retard_s, nblocs_offset_s, npilotes_offset_s, final_joc_offset_s, (char *)NULL);
		exit(1);
	}
	return 0;
}

/* * Calcula l'efecte de rebot en la paleta */
float control_impacte1(int c_pil, float velc0, int c_pal, int m_pal)
{
	int distApal;
	float vel_c;

	if (m_pal <= 0) return velc0;

	distApal = c_pil - c_pal;
	if (distApal >= 2*m_pal/3) vel_c = 0.5;
	else if (distApal <= m_pal/3) vel_c = -0.5;
	else if (distApal == m_pal/2) vel_c = 0.0;
	else vel_c = velc0;
	return vel_c;
}

/* * Funció principal de moviment de la pilota */
void mou_pilota_bucle(int c_pal, int m_pal, float *pos_f, float *pos_c, float *vel_f, float *vel_c, char ball_id)
{
	int f_h, c_h;
	int f_pil, c_pil;
	char rh, rv, rd;
	int fora = 0;
	char tipus_bloc;

	f_pil = (int)*pos_f;
	c_pil = (int)*pos_c;

	while (!fora) {
		f_h = *pos_f + *vel_f;
		c_h = *pos_c + *vel_c;
		rh = rv = rd = ' ';

		if ((f_h != f_pil) || (c_h != c_pil)) {

			if (f_h != f_pil) {
				rv = win_quincar(f_h, c_pil);
				if (rv != ' ') {
					tipus_bloc = comprovar_bloc(f_h, c_pil);
					if (tipus_bloc == BLKCHAR) {
						crear_nova_pilota(f_h, c_pil, *vel_f, *vel_c);
					}
					if (rv == '0')
						*vel_c = control_impacte1(c_pil, *vel_c, c_pal, m_pal);
					*vel_f = -(*vel_f);
					f_h = *pos_f + *vel_f;
				}
			}

			if (c_h != c_pil) {
				rh = win_quincar(f_pil, c_h);
				if (rh != ' ') {
					tipus_bloc = comprovar_bloc(f_pil, c_h);
					if (tipus_bloc == BLKCHAR) {
						crear_nova_pilota(f_pil, c_h, *vel_f, *vel_c);
					}
					*vel_c = -(*vel_c);
					c_h = *pos_c + *vel_c;
				}
			}

			if ((f_h != f_pil) && (c_h != c_pil)) {
				rd = win_quincar(f_h, c_h);
				if (rd != ' ') {
					tipus_bloc = comprovar_bloc(f_h, c_h);
					if (tipus_bloc == BLKCHAR) {
						crear_nova_pilota(f_h, c_h, *vel_f, *vel_c);
					}
					*vel_f = -(*vel_f);
					*vel_c = -(*vel_c);
					f_h = *pos_f + *vel_f;
					c_h = *pos_c + *vel_c;
				}
			}

			rd = win_quincar(f_h, c_h);
			if (rd == ' ') {
				win_escricar(f_pil, c_pil, ' ', NO_INV);
				*pos_f += *vel_f;
				*pos_c += *vel_c;
				f_pil = f_h;
				c_pil = c_h;

				if (f_pil != n_fil - 1)
					win_escricar(f_pil, c_pil, ball_id, NO_INV);
				else
					fora = 1;
			}
		} else {
			*pos_f += *vel_f;
			*pos_c += *vel_c;
		}

		if (!fora) {
			if (p_final_joc != NULL && *p_final_joc) break;
			win_retard(retard);
		}
	}

	win_escricar(f_pil, c_pil, ' ', NO_INV);
}

/* --- Programa Principal --- */
int main(int n_args, char *ll_args[])
{
	int c_pal, m_pal;
	float pos_f, pos_c, vel_f, vel_c;
	char ball_id_char;

	if (n_args != 16) {
		fprintf(stderr, "Error: Nombre d'arguments incorrecte\n");
		fprintf(stderr, "Ús: pilota1 id_mem n_fil n_col m_por c_pal m_pal pos_f pos_c vel_f vel_c ball_id retard nblocs_offset npilotes_offset final_joc_offset\n");
		exit(1);
	}

	id_mem = atoi(ll_args[1]);
	n_fil = atoi(ll_args[2]);
	n_col = atoi(ll_args[3]);
	m_por = atoi(ll_args[4]);
	c_pal = atoi(ll_args[5]);
	m_pal = atoi(ll_args[6]);
	pos_f = atof(ll_args[7]);
	pos_c = atof(ll_args[8]);
	vel_f = atof(ll_args[9]);
	vel_c = atof(ll_args[10]);
	ball_id_char = ll_args[11][0];
	retard = atoi(ll_args[12]);
	nblocs_offset = atoi(ll_args[13]);
	npilotes_offset = atoi(ll_args[14]);
	final_joc_offset = atoi(ll_args[15]);

	p_mem = map_mem(id_mem);
	if (p_mem == NULL) {
		fprintf(stderr, "Error: No s'ha pogut connectar a memòria compartida\n");
		exit(1);
	}

	win_set(p_mem, n_fil, n_col);
	p_nblocs = (int *)((char *)p_mem + nblocs_offset);
	p_npilotes = (int *)((char *)p_mem + npilotes_offset);
	p_final_joc = (int *)((char *)p_mem + final_joc_offset);

	mou_pilota_bucle(c_pal, m_pal, &pos_f, &pos_c, &vel_f, &vel_c, ball_id_char);

	/* Decrementar el comptador de pilotes abans de sortir */
	if (p_npilotes != NULL) (*p_npilotes)--;

	return 0;
}
