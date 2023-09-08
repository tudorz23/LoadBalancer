/* Copyright 2023 <> */
#ifndef LOAD_BALANCER_H_
#define LOAD_BALANCER_H_

#include "server.h"

#define INIT_RING_CAPACITY 3
#define REPLICA_FACTOR 100000

struct load_balancer;
typedef struct load_balancer load_balancer;

/**
 * init_load_balancer() - initializes the memory for a new load balancer and its fields and
 *                        returns a pointer to it
 *
 * Return: pointer to the load balancer struct
 */
load_balancer *init_load_balancer();

/**
 * free_load_balancer() - frees the memory of every field that is related to the
 * load balancer (servers, hashring)
 *
 * @arg1: Load balancer to free
 */
void free_load_balancer(load_balancer *main);

/**
 * load_store() - Stores the key-value pair inside the system.
 * @arg1: Load balancer which distributes the work.
 * @arg2: Key represented as a string.
 * @arg3: Value represented as a string.
 * @arg4: This function will RETURN via this parameter
 *        the server ID which stores the object.
 *
 * The load balancer will use Consistent Hashing to distribute the
 * load across the servers. The chosen server ID will be returned
 * using the last parameter.
 *
 * Hint:
 * Search the hashring associated to the load balancer to find the server where the entry
 * should be stored and call the function to store the entry on the respective server.
 *
 */
void loader_store(load_balancer *main, char *key, char *value, int *server_id);

/**
 * load_retrieve() - Gets a value associated with the key.
 * @arg1: Load balancer which distributes the work.
 * @arg2: Key represented as a string.
 * @arg3: This function will RETURN the server ID
          which stores the value via this parameter.
 *
 * The load balancer will search for the server which should posess the
 * value associated to the key. The server will return NULL in case
 * the key does NOT exist in the system.
 *
 * Hint:
 * Search the hashring associated to the load balancer to find the server where the entry
 * should be stored and call the function to store the entry on the respective server.
 */
char *loader_retrieve(load_balancer *main, char *key, int *server_id);

/**
 * load_add_server() - Adds a new server to the system.
 * @arg1: Load balancer which distributes the work.
 * @arg2: ID of the new server.
 *
 * The load balancer will generate 3 replica labels and it will
 * place them inside the hash ring. The neighbor servers will
 * distribute some the objects to the added server.
 *
 * Hint:
 * Resize the servers array to add a new one.
 * Add each label in the hashring in its appropiate position.
 * Do not forget to resize the hashring and redistribute the objects
 * after each label add (the operations will be done 3 times, for each replica).
 */
void loader_add_server(load_balancer *main, int server_id);

/**
 * load_remove_server() - Removes a specific server from the system.
 * @arg1: Load balancer which distributes the work.
 * @arg2: ID of the removed server.
 *
 * The load balancer will distribute ALL objects stored on the
 * removed server and will delete ALL replicas from the hash ring.
 *
 * We will remove the 3 replicas one by one.
 * We will search for the position of every replica, take the objects
 * from it and move them to the clockwise following neighbour server
 * If the server to remove is the last one, we will delete all objects.
 */
void loader_remove_server(load_balancer *main, int server_id);

unsigned int hash_function_servers(void *a);
unsigned int hash_function_key(void *a);

/** 
 * Binary search to determine the position where the server replica
 * should pe placed in the hashring array.
 * At some point, left == right, so we always store position equal to one
 * of them.
 */
int get_replica_position(load_balancer *main, int label, int server_id);

/** 
 * Resizes the hashring to twice the previous capacity or to half of it,
 * thus making it efficient, with <how> indicating if there should be more
 * space or less.
 */
void resize_hashring(server_memory ***hashring, load_balancer *main, int how);

/**
 * Iterates through the objects of the clockwise neighbour and checks
 * if one should move to the new server, based on certain conditions
 * that should happen.
 */
void move_objects_between_servers(load_balancer *main, int neigh_pos,
                                  int label, int position);

/** 
 * Moves the elements of the hashring array from <position> to <main->size> one
 * position forward and places the new server on <position>. Also, checks the
 * clockwise neighbour of the added server for key-value pairs that should be
 * stored on the new server and does that, eliminating them from the old server
 */
void server_put(load_balancer *main, int position, int server_id, int label);

/**
 * Binary search to determine the server where the key-value pair
 * should pe placed.
 * At some point, left == right, so we always store <position> equal to one
 * of them.
 */
int get_server_for_key(load_balancer *main, unsigned int key_hash);

/**
 * Binary search to determine the position in the hashring of the server
 * we want to remove. Unlike the previous two binary searches, this one
 * is a classical one, where the loop stops when the value is found, not
 * when left equals right.
 */
int binary_search_for_remove(load_balancer *main, int label, int server_id);

/**
 * Moves the objects from the outgoing server to its clockwise neighbour,
 * then moves all elements from the hashring starting from <position> to the
 * last one one position backwards.
 */
void server_out(load_balancer *main, int position);

#endif /* LOAD_BALANCER_H_ */
