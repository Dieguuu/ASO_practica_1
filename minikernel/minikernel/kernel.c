/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	/* Por limpieza en la ejecución se comenta esta parte
	printk("-> NO HAY LISTOS. ESPERA INT\n"); */

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	/* Al terminar el proceso se cierran todos los mutex que utilizaban */
	for(int i = 0; i < NUM_MUT_PROC; i++){
		if(p_proc_actual->descriptor[i] != 0) cerrar_mutex(p_proc_actual->descriptor[i]);
	}

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

        return;
}


void round_robin(){
	if(p_proc_actual->estado == LISTO) p_proc_actual->robin_seconds--;

		if(p_proc_actual->robin_seconds <= 0){
			printk("Ronda del proceso terminada, llamando a interrupción de software.\n");

			/* Como se indica en el manual del minikernel, las interrupciones de Software se
				realizan para el tratamiento de cambios de contexto involuntarios */
			activar_int_SW();
		}
}



/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	/* Para mayor limpieza en la ejecución del código se prescinde de este print
	printk("-> TRATANDO INT. DE RELOJ\n");	*/
	timer();
	round_robin();

        return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw(){

	printk("-> TRATANDO INT. SW\n");

	robin_process_change();

	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;

		/* A la hora de crear el proceso deben establecerse el número de TICKS
			de round robin que debe estar ejecutándose */
		p_proc->robin_seconds = TICKS_POR_RODAJA;

		/* lo inserta al final de cola de listos */
		insertar_ultimo(&lista_listos, p_proc);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){

	printk("-> FIN PROCESO %d\n", p_proc_actual->id);

	liberar_proceso();

        return 0; /* no deber�a llegar aqui */
}

int obtener_id_pr(){
	int id = p_proc_actual->id;
	printk("ID del proceso actual es: %d\n", id);
	return id;
}

/*
 *	Función dormir, apartado 1 de la práctica
 */
int dormir(unsigned int seconds){

	printk("Ha entrado en la función dormir.\n");

	/*	Lectura del registro 1 que almacena la variable con los segundos
		que debe bloquearse el proceso	*/
	unsigned int sleeping_seconds = (unsigned int)leer_registro(1);

	/*	Guardado del nivel de interrupción	*/
	int interruption_level = fijar_nivel_int(NIVEL_3);

	/* Guardado del proceso actual que quiere bloquearse	*/
	BCPptr actual_process = p_proc_actual;

	/*	Actualización del estado del proceso en el BCP	*/
	actual_process->estado = BLOQUEADO;

	/* Actualización del número de segundos que debe dormir el proceso ajustado
		a la velocidad del reloj */
	actual_process->seconds = sleeping_seconds*TICK;

	/* Intercambio del proceso entre lista_listos y lista_bloqueados */
	eliminar_elem(&lista_listos, actual_process);
	insertar_ultimo(&lista_bloqueados, actual_process);

	/* Llamada al planificador que devuelve nuevo proceso en ejecución */
	p_proc_actual = planificador();

	/* Restauración del nivel de interrupción */
	fijar_nivel_int(interruption_level);

	/* Cambio de contexto entre el nuevo proceso a ejecutar y el proceso bloqueado */
	cambio_contexto(&(actual_process->contexto_regs), &(p_proc_actual->contexto_regs));

	return 1;
}

void process_unlock(BCPptr sleeping_process){
	
	/* Guardado del nivel de interrupción y cambio a nivel 3 */
	int interruption_level = fijar_nivel_int(NIVEL_3);

	/* Cambio de estado del proceso desbloqueado */
	sleeping_process->estado = LISTO;

	/* Intercambio del proceso entre lista bloqueados y lista listos */
	eliminar_elem(&lista_bloqueados, sleeping_process);
	insertar_ultimo(&lista_listos, sleeping_process);	

	/* Restauración del nivel de interrupción */
	fijar_nivel_int(interruption_level);
	return;
}

void timer(){

	/* Puntero que apunta al primer proceso de la lista_bloqueados */
	BCPptr sleeping_process = lista_bloqueados.primero;

	/* Bucle que recorre la lista de procesos bloqueados hasta el final */
	while(sleeping_process != NULL){

		BCPptr siguiente = sleeping_process->siguiente;

		/* Reducción de el tiempo que el proceso debe permanecer bloqueado */
		sleeping_process->seconds--;
		printk("Proceso id %d restan %d TICKs de reloj para despertar.\n",sleeping_process->id, sleeping_process->seconds);

		/* Comprobación de que el proceso debe dejar de estar bloqueado */
		if(sleeping_process->seconds <= 0){

			/* Llamada a la función de desbloqueo */
			process_unlock(sleeping_process);
		}

		/* Avance del puntero hacia el siguiente proceso bloqueado */
		sleeping_process = siguiente;
	}
	return;
}

/*
 *
 *	Funciones relacionadas con el tratamiento de mutex
 *
 */

/* Función que revisa que el nombre introducido al nuevo mutex no se encuentra actualmente en uso */
int mutex_name_taken(char *mutex_name){

	/* Recorrido de la lista de mutex*/
	for(int i = 0; i < NUM_MUT; i++){
		/* En caso de encontrar un mutex con el mismo nombre al introducido, devolver error */
		if(strcmp(lista_mutex[i].name, mutex_name) == 0){
			return -1;
		}
	}

	return 1;
}

/* Función que busca la posición del mutex en la lista de mutex */
int mutex_search_name(char *mutex_name){

	for(int i = 0; i < NUM_MUT; i++){
		if(strcmp(lista_mutex[i].name, mutex_name) == 0){
			return i;
		}
	}
	return -1;
}

/* Función que revisa si hay un hueco en la lista de mutex para el nuevo mutex comprobando que
	el nombre sea igual a NULL */
int free_mutex_position(){

	/* Recorrido de la lista de mutex */
	for(int i = 0; i < NUM_MUT; i++){
		/* En caso de que el nombre sea igual a vacío significa que
			la posición en el array está vacía, se devuelve la posición libre */
		if(strcmp(lista_mutex[i].name, "") == 0){
			return i;
		}
	}
	return -1;
}

int check_mutex_id(unsigned int mutex_id){

	for(int i = 0; i < NUM_MUT_PROC; i++){
		if(p_proc_actual->descriptor[i] == mutex_id){
			return i;
		}
	}
	return -1;
}

/* Función que comprueba que el proceso que crea el mutex tenga descriptores libres */
int process_descriptors(BCPptr process){

	for(int i = 0; i < NUM_MUT_PROC; i++){
		if(process->descriptor[i] == 0){
			return i;
		}
	}
	return -1;
}

/* Genera la estructura del nuevo mutex con los datos introducidos */
mutex generate_mutex(char *nombre, int tipo){

	mutex generated_mutex;

	strcpy(generated_mutex.name, nombre);
	generated_mutex.type = tipo;
	generated_mutex.lock_process = NULL;
	generated_mutex.lock_amount = 0;
	generated_mutex.descriptor_amount = 1;

	return generated_mutex;
}

void erase_mutex(unsigned int mutex_id){

	mutex* actual_mutex = &lista_mutex[(mutex_id-1)];

	strcpy(actual_mutex->name,"");
}

/* Función que bloquea el mutex */
void block_mutex_process(){

	int interruption_level = fijar_nivel_int(NIVEL_3);
	
	BCPptr blocked_process = p_proc_actual;
	blocked_process->estado = BLOQUEADO;

	eliminar_elem(&lista_listos, blocked_process);
	insertar_ultimo(&lista_espera_mutex, blocked_process);

	p_proc_actual = planificador();

	fijar_nivel_int(interruption_level);

	cambio_contexto(&(blocked_process->contexto_regs),&(p_proc_actual->contexto_regs));
}

void unblock_mutex_process(){
	int interruption_level = fijar_nivel_int(NIVEL_3);

	BCPptr unblocked_process = lista_espera_mutex.primero;

	unblocked_process->estado = LISTO;

	eliminar_primero(&lista_espera_mutex);
	insertar_ultimo(&lista_listos, unblocked_process);

	fijar_nivel_int(interruption_level);
}

void block_locking_process(unsigned int mutex_id){

	int interruption_level = fijar_nivel_int(NIVEL_3);

	BCPptr locking_process = p_proc_actual;
	p_proc_actual->estado = BLOQUEADO;

	eliminar_elem(&lista_listos, locking_process);
	insertar_ultimo(&lista_mutex[(mutex_id - 1)].waiting_process, locking_process);

	p_proc_actual = planificador();

	fijar_nivel_int(interruption_level);

	cambio_contexto(&(locking_process->contexto_regs), &(p_proc_actual->contexto_regs));
}

void unblock_locking_process(unsigned int mutex_id){
	
	int interruption_level = fijar_nivel_int(NIVEL_3);
	BCPptr locking_process = lista_mutex[(mutex_id - 1)].waiting_process.primero;
	eliminar_primero(&lista_mutex[(mutex_id - 1)].waiting_process);
	insertar_ultimo(&lista_listos, locking_process);
	lista_mutex[(mutex_id - 1)].lock_process = locking_process;

	fijar_nivel_int(interruption_level);
}

void free_blocked_process(unsigned int mutex_id){

	int interruption_level = fijar_nivel_int(NIVEL_3);

	mutex* actual_mutex = &lista_mutex[(mutex_id - 1)];

	BCPptr waiting_process = actual_mutex->waiting_process.primero;
	BCPptr next_waiting_process = NULL;

	while(waiting_process != NULL){
		next_waiting_process = waiting_process->siguiente;

		eliminar_primero(&actual_mutex->waiting_process);
		insertar_ultimo(&lista_listos, waiting_process);

		waiting_process->estado = LISTO;

		waiting_process = next_waiting_process;
	}

	fijar_nivel_int(interruption_level);
}

int check_locking_process(unsigned int mutexid, BCPptr process){
	mutex* selected_mutex = &lista_mutex[(mutexid-1)];

	if(selected_mutex->lock_process == process){
		return 1;
	}

	return -1;
}

/*
 *	Crear mutex
 */

int crear_mutex(char *nombre, int tipo){

	/* Lectura del registro 1 para obtener el nombre */
	char* mutex_name = (char*)leer_registro(1);

	/* Lectura del registro 2 para obtener el tipo */
	int mutex_type = (int)leer_registro(2);

	/* Se guarda el nivel de interrupción */
	int interruption_level = fijar_nivel_int(NIVEL_1);

	/* Comprobación del tamaño del nombre introducido */
	if(strlen(mutex_name) > (MAX_NOM_MUT - 1)){
		/* En caso de que el nombre introducido sea mayor que el tamaño
			del nombre del mutex, este se acorta. */
		printk("Nombre introducido mayor de los permitido, el nombre se acortará.\n");
		mutex_name[MAX_NOM_MUT] = '\0';
	}

	if(process_descriptors(p_proc_actual) == -1){
		printk("El proceso que va a crear el mutex no tiene descriptores libres\n");
		fijar_nivel_int(interruption_level);
		return -1;
	}

	if(mutex_name_taken(mutex_name) == -1){
		printk("Ya existe un mutex con el mismo nombre.\n");
		fijar_nivel_int(interruption_level);
		return -2;
	}

	if(free_mutex_position() == -1){
		printk("No hay hueco en la lista de mutex, bloqueando proceso hasta que quede hueco libre.\n");
		block_mutex_process();
		fijar_nivel_int(interruption_level);
	}

	/* Se genera un mutex con el nombre y el tipo introducidos */
	mutex generated_mutex = generate_mutex(mutex_name, mutex_type);

	/* Se añade el mutex generado a la primera posición libre de la lista de mutex */
	lista_mutex[free_mutex_position()] = generated_mutex;

	/* Se añade el descriptor del mutex creado al proceso que lo ha creado */
	p_proc_actual->descriptor[process_descriptors(p_proc_actual)] = (free_mutex_position() + 1);

	fijar_nivel_int(interruption_level);
	printk("Mutex %s creado con éxito.\n",mutex_name);
	return 0;
}

/*
 * Abrir mutex
 */

int abrir_mutex(char *nombre){

	printk("Comenzando a abrir el mutex.\n");

	char* mutex_name = (char*)leer_registro(1);
	int interruption_level = fijar_nivel_int(NIVEL_1);

	if(process_descriptors(p_proc_actual) == -1){
		printk("El proceso que intenta abrir el mutex no tiene descriptores libres.\n");
		fijar_nivel_int(interruption_level);
		return -1;
	}

	if(mutex_search_name(mutex_name) == -1){
		printk("No existe un mutex con el nombre introducido.\n");
		fijar_nivel_int(interruption_level);
		return -2;
	}

	p_proc_actual->descriptor[process_descriptors(p_proc_actual)] = (mutex_search_name(mutex_name) + 1);
	lista_mutex[mutex_search_name(mutex_name)].descriptor_amount++;
	
	fijar_nivel_int(interruption_level);
	printk("Mutex abierto correctamente.\n");
	return (mutex_search_name(mutex_name) + 1);
}

/*
 *	Lock
 */

int lock(unsigned int mutexid){

	printk("Comenzando a realizar el lock.\n");

	unsigned int mutex_id = (unsigned int)leer_registro(1);

	int interruption_level = fijar_nivel_int(NIVEL_1);

	if(check_mutex_id(mutex_id) == -1){
		printk("El proceso no cuenta con el mutex indicado entre sus descriptores.\n");
		fijar_nivel_int(interruption_level);
		return -1;
	}

	mutex* selected_mutex = &lista_mutex[(mutex_id-1)];

	while(selected_mutex->lock_process != p_proc_actual && selected_mutex->lock_process != NULL){
		printk("El mutex ya está lock, bloqueando proceso.\n");
		block_locking_process(mutex_id);
	}

	selected_mutex->lock_process = p_proc_actual;

	if(selected_mutex->lock_amount == 1 && selected_mutex->type == NO_RECURSIVO){
		printk("El mutex no es recursivo.\n");
		fijar_nivel_int(interruption_level);
		return -2;
	}

	selected_mutex->lock_amount++;
	fijar_nivel_int(interruption_level);
	printk("Lock realizado con éxito.\n");
	return 0;
}

/*
 *	Unlock
 */

int unlock(unsigned int mutexid){

	printk("Comenzando a realizar el unlock.\n");
	unsigned int mutex_id = (unsigned int)leer_registro(1);
	int interruption_level = fijar_nivel_int(NIVEL_1);

	if(check_mutex_id(mutex_id) == -1){
		printk("El proceso no cuenta con el descriptor suministrado.\n");
		fijar_nivel_int(interruption_level);
		return -1;
	}

	if(lista_mutex[(mutex_id-1)].lock_process != p_proc_actual){
		printk("El proceso está intentando hacer unlock a un mutex que no está usando.\n");
		fijar_nivel_int(interruption_level);
		return -2;
	}

	lista_mutex[(mutex_id-1)].lock_amount--;

	if(lista_mutex[(mutex_id-1)].lock_amount != 0){
		printk("Mutex recursivo.\n");
		fijar_nivel_int(interruption_level);
		return 0;
	}

	lista_mutex[(mutex_id - 1)].lock_process = NULL;

	if(lista_mutex[(mutex_id-1)].waiting_process.primero == NULL){
		printk("No quedan procesos esperando al mutex. Unlock realizado con éxito.\n");
		fijar_nivel_int(interruption_level);
		return 0;
	}

	unblock_locking_process(mutex_id);
	fijar_nivel_int(interruption_level);
	printk("Unlock realizado con éxito.\n");	
	return 0;
}

/*
 *	Cerrar mutex
 */

int cerrar_mutex(unsigned int mutexid){

	unsigned int mutex_id = (unsigned int)leer_registro(1);
	if(mutex_id < 16) mutexid = mutex_id;
	int interruption_level = fijar_nivel_int(NIVEL_1);

	

	if(check_mutex_id(mutexid) == -1){
		printk("El proceso no cuenta con el descriptor suministrado.\n");
		fijar_nivel_int(interruption_level);
		return -1;
	}

	mutex* actual_mutex = &lista_mutex[(mutexid-1)];

	if(actual_mutex->lock_process == p_proc_actual){
		printk("El proceso está bloqueando el proceso, desbloqueando.\n");
		actual_mutex->lock_process = NULL;
		actual_mutex->lock_amount = 0;
	}

	p_proc_actual->descriptor[check_mutex_id(mutexid)] = 0;
	actual_mutex->descriptor_amount--;

	if(actual_mutex->descriptor_amount == 0){
		printk("No quedan procesos utilizando el mutex. Borrando el mutex.\n");
		erase_mutex(mutexid);
		printk("Mutex borrado correctamente.\n");
		if(lista_espera_mutex.primero != NULL) unblock_mutex_process();
		fijar_nivel_int(interruption_level);
		return 0;
	}

	if(actual_mutex->waiting_process.primero != NULL) unblock_locking_process(mutexid);
	fijar_nivel_int(interruption_level);
	printk("Mutex cerrado correctamente.\n");
	return 0;
}


/*
 *
 *	Round robin
 *
 */



void robin_process_change(){

	BCPptr actual_process = p_proc_actual;
	int interruption_level = fijar_nivel_int(NIVEL_3);

	eliminar_primero(&lista_listos);
	insertar_ultimo(&lista_listos, actual_process);

	p_proc_actual = planificador();
	if(p_proc_actual->robin_seconds < TICKS_POR_RODAJA) p_proc_actual->robin_seconds = TICKS_POR_RODAJA;
	
	fijar_nivel_int(interruption_level);
	cambio_contexto(&(actual_process->contexto_regs), &(p_proc_actual->contexto_regs));
}

/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
