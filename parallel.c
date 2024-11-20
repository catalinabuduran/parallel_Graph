// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */
static pthread_mutex_t sum_mutex;
static pthread_mutex_t neighbour_status;

/* TODO: Define graph task argument. */
typedef struct {
	unsigned int node_index;
} graph_task_arg_t;

// Forward declaration of process_node
static void process_node(unsigned int idx);

void process_node_wrapper(void *arg)
{
	unsigned int idx = *(unsigned int *)arg;

	process_node(idx);
	sem_post(&tp->semaphore);
}

static void process_node(unsigned int idx)
{
	os_node_t *node;
    // Add the current node value to the overall sum.
	if (graph->visited[idx] == PROCESSING) {
		pthread_mutex_lock(&sum_mutex);
		node = graph->nodes[idx];
		sum += node->info;
		pthread_mutex_unlock(&sum_mutex);
		graph->visited[idx] = DONE;

    // Create tasks and add them to the task queue for the neighboring nodes.
		for (unsigned int i = 0; i < node->num_neighbours; i++) {
			pthread_mutex_lock(&neighbour_status);
			if (graph->visited[node->neighbours[i]] == NOT_VISITED) {
				graph->visited[node->neighbours[i]] = PROCESSING;
				pthread_mutex_unlock(&neighbour_status);

				//Create a new task
				graph_task_arg_t *new_task_arg = malloc(sizeof(graph_task_arg_t));

				if (new_task_arg == NULL)
					exit(EXIT_FAILURE);
				new_task_arg->node_index = node->neighbours[i];
				os_task_t *new_task = create_task(process_node_wrapper, new_task_arg, free);
				//Put a new task to threadpool task queue
				enqueue_task(tp, new_task);
			} else {
				pthread_mutex_unlock(&neighbour_status);
			}
		}
	}
	sem_post(&tp->semaphore);
}



int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* TODO: Initialize graph synchronization mechanisms. */
	pthread_mutex_init(&sum_mutex, NULL);
	pthread_mutex_init(&neighbour_status, NULL);
	graph->visited[0] = PROCESSING;
	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);

	return 0;
}
