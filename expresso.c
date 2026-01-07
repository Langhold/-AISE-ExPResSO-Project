
/* /!\ pour tout non-utilisateur de processeur Apple Silicon, remplacer les 2 includes par include <unistd.h> et remplacer la section dans expresso_worker_count() */
#define _DARWIN_C_SOURCE // -> #define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include "expresso.h"
#include <unistd.h>


int start;
int * index_threads;

pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_t * threads;

schedule_t schedule = SCHEDULE_STATIC;

work_t * tasks = NULL;
work_t ** task_threads_list;

size_t task_count = 0;
size_t tasks_per_thread = 0;

int end(int error){
	
	free(threads);
	free(index_threads);
	work_t *tmp;
	while (tasks) {
		tmp = tasks;
		tasks = tasks->next;
		free(tmp);
	}
	return error;
}


void push(work_t ** tasks, void (* work) (void *), void * data){
	work_t * element = (work_t *) malloc(sizeof(work_t));
	if(!element) exit(EXIT_FAILURE);
	element->work = work;
	element->data = data;
	element->next = *tasks;
	*tasks = element;
}

void parse_task(const size_t nb_threads){
	
	pthread_mutex_lock(&mutex);
	tasks_per_thread = task_count / nb_threads;
	work_t * current = tasks;
	
	for (int i = 0; i<nb_threads; ++i) {
		work_t * head = current;
		
		/* Parse the chained list:	 task_threads_list[0] = T1 -> T2 -> T3
									 task_threads_list[1] = T4 -> T5 -> T6
									 task_threads_list[2] = T7 -> T8 -> T9
													...
		 */
		for (int j = 0; j < tasks_per_thread; ++j) {
			if (!current) break;
			current = current->next;
		}
		task_threads_list[i]=head;
	}
	
	pthread_mutex_unlock(&mutex);
}

void *work(void *arg){
	int index = *(int *) arg;
	pthread_mutex_lock(&mutex);
	
	while (!start){
		pthread_cond_wait(&cond, &mutex);
	}
	pthread_mutex_unlock(&mutex);
	work_t * task = task_threads_list[index];
	
	
	for(int i = 0; i<tasks_per_thread; ++i) {
		task->work(task->data);
		task = task->next;
	}
	
	return NULL;
}



int expresso_initialize(void){
	printf("expresso launched...\n");
	start = 0;
	size_t nb_threads = expresso_worker_count();
	threads = (pthread_t *) malloc(sizeof(pthread_t)*(nb_threads-1)); //-1 excludes the principal thread
	
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	int error = 0;
	
	task_threads_list = (work_t **) malloc(nb_threads * sizeof(work_t *));
	for (size_t i = 0; i < nb_threads; i++)
		task_threads_list[i] = NULL;
	
	index_threads = (int *) malloc(nb_threads * sizeof(int));
	for(int i = 1; i<nb_threads; ++i){
		index_threads[i] = i;
		/* The first worker makes the second task because the principal thread is making the first one */
		error = pthread_create(&threads[i-1], NULL, &work, &index_threads[i]);
		if (error) {
			for (int j = 0; j < i; ++j) {
				pthread_join(threads[j], NULL);
			}
			return error;
			
		}
	}
	return error;
}

size_t expresso_worker_count(void){
	char * env = getenv("EXPRESSO_WORKER_COUNT");
	//if(!env) return (size_t) sysconf(_SC_NPROCESSORS_ONLN);
	if(!env){
		int ncpu;
		size_t size;
		sysctlbyname("hw.ncpu", &ncpu, &size, NULL, 0);
		return ncpu;
	}

	return atoi(env);
}

schedule_t expresso_schedule_get(void){
	return schedule;
}

void expresso_schedule_set(schedule_t policy){
	schedule = policy;
}

int expresso_task(void (* work) (void *), void * data){
	if(work == NULL){
		return -1;
	}
	pthread_mutex_lock(&mutex);
	
	push(&tasks, work, data);
	
	++task_count;
	pthread_mutex_unlock(&mutex);
	return 0;
}

int expresso_weighted_task(void (* work) (void *), void * data, unsigned int weight){
	return 0;
}

int expresso_wait(void){
	size_t nb_threads = expresso_worker_count();
	/* if nb_threads=0, makes the execution sequential */
	if (nb_threads != 1) {
		parse_task(nb_threads);
	}
	start = 1;
	pthread_cond_broadcast(&cond);
	
	/* The principal thread make the first task */
	work_t *task = task_threads_list[0];
	while (task) {
		task->work(task->data);
		task = task->next;
	}
	return 0;
}

void expresso_stats(void){ }

double expresso_wall_time(void){
	return 0.;
}

int expresso_finalize(void){
	
	int error = 0;
	for(int i = 0; i<expresso_worker_count()-1; ++i){
		error = pthread_join(threads[i], NULL);
		if (error) {
			return end(error);
		}
	}
	return end(error);
}

