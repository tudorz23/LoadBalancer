/* Copyright 2023 <> */
#include <stdlib.h>
#include <string.h>

#include "server.h"

server_memory *init_server_memory()
{
	server_memory *server = malloc(sizeof(*server));
	DIE(!server, "malloc failed\n");

	server->id = -1;
	server->label = -1;

	server->objects = ht_create(HMAX, hash_function_string,
					   compare_function_strings, key_val_free_function);

	return server;
}

void server_store(server_memory *server, char *key, char *value) {
	ht_put(server->objects, key, strlen(key) + 1, value, strlen(value) + 1);
}

char *server_retrieve(server_memory *server, char *key) {
	char *data = (char *)(ht_get(server->objects, key));

	return data;
}

void server_remove(server_memory *server, char *key) {
	ht_remove_entry(server->objects, key);
}

void free_server_memory(server_memory *server) {
	ht_free(server->objects);
	free(server);
}
