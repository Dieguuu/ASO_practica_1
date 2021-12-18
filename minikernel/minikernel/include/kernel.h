/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/* Incluimos la librería string.h */
#include "string.h"

/* Constantes que referencian el tipo del mutex */
#define NO_RECURSIVO 0
#define RECURSIVO 1

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
        int id;				/* ident. del proceso */
        int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
        contexto_t contexto_regs;	/* copia de regs. de UCP */
        void * pila;			/* dir. inicial de la pila */
		BCPptr siguiente;		/* puntero a otro BCP */
		void *info_mem;			/* descriptor del mapa de memoria */

		/* Elementos necesarios para la llamada dormir */
		unsigned int seconds;	/* Tiempo de bloqueo del proceso */

		/* Elementos necesarios para la realización del mutex */
		int descriptor[NUM_MUT_PROC]; /* Descriptores de mutex */
} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;

/* Definición de la estructura correspondiente al mutex */
typedef struct{
	char name[MAX_NOM_MUT]; /* Nombre del mutex */
	int type; 				/* NO_RECURSIVO|RECURSIVO */
	lista_BCPs waiting_process;	/* Procesos esperando al mutex */
	int waiting_process_amount;	/* Número de procesos en la lista de espera del mutex */
	BCPptr lock_process;			/* Proceso usando el mutex */
	int lock_amount;		/* Veces que ha sido bloqueado el mutex */
} mutex;

/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};

/* Variable global que representa la cola de procesos bloqueados */
lista_BCPs lista_bloqueados = {NULL, NULL};

/* Lista de procesos en espera para crear un mutex */
lista_BCPs lista_espera_mutex = {NULL, NULL};

/* Variable global que representa la cola de mutex */
mutex lista_mutex[NUM_MUT];

/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int obtener_id_pr();

/* Rutina de bloqueo de proceso */
int dormir(unsigned int seconds);

/* Rutinas de tratamiento de mutex */
int crear_mutex(char *nombre, int tipo);
int abrir_mutex(char *nombre);
int lock(unsigned int mutexid);
int unlock(unsigned int mutexid);
int cerrar_mutex(unsigned int mutexid);

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{obtener_id_pr},
					{dormir},
					{crear_mutex},
					{abrir_mutex},
					{lock},
					{unlock},
					{cerrar_mutex}};

#endif /* _KERNEL_H */

