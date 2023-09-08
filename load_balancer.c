/* Copyright 2023 <> */
#include <stdlib.h>
#include <string.h>

#include "load_balancer.h"

struct load_balancer {
    server_memory **hashring;

    // number of elements of the hashring array
    int size;

    // capacity of the hashring array
    int capacity;
};

unsigned int hash_function_servers(void *a) {
    unsigned int uint_a = *((unsigned int *)a);

    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = (uint_a >> 16u) ^ uint_a;
    return uint_a;
}

unsigned int hash_function_key(void *a) {
    unsigned char *puchar_a = (unsigned char *)a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
        hash = ((hash << 5u) + hash) + c;

    return hash;
}

load_balancer *init_load_balancer() {
    load_balancer *balancer = malloc(sizeof(*balancer));
    DIE(!balancer, "malloc failed\n");

    balancer->hashring = malloc(INIT_RING_CAPACITY *
                                sizeof(*balancer->hashring));
    DIE(!balancer->hashring, "malloc failed\n");

    for (int i = 0; i < INIT_RING_CAPACITY; i++)
        balancer->hashring[i] = NULL;

    balancer->size = 0;
    balancer->capacity = INIT_RING_CAPACITY;

    return balancer;
}

int get_replica_position(load_balancer *main, int label, int server_id)
{
    if (main->size == 0)
        return 0;

    // extremities for the binary search
    int left = 0;
    int right = main->size - 1;

    // the position that the function should return
    int position;

    if (right == 0)
        position = 0;

    unsigned int replica_hash = hash_function_servers(&label);

    while (left < right) {
        int middle = (left + right) / 2;

        server_memory *curr = main->hashring[middle];
        unsigned int to_compare = hash_function_servers(&(curr->label));

        if (replica_hash == to_compare) {
            position = middle;
            break;
        }

        if (replica_hash > to_compare) {
            left = middle + 1;
            position = left;
        } else {
            right = middle;
            position = right;
        }
    }

    server_memory *curr = main->hashring[position];
    unsigned int to_compare = hash_function_servers(&(curr->label));

    if (replica_hash < to_compare) {
        return position;
    }

    if (replica_hash == to_compare) {
        if (server_id < curr->id)
            return position;
        else
            return (position + 1);
    }

    // Surely, replica_hash > to_compare
    return (position + 1);
}

void resize_hashring(server_memory ***hashring, load_balancer *main, int how)
{
    if (how == 1)
        (main->capacity) *= 2;
    else
        (main->capacity) /= 2;

    server_memory **tmp = realloc(*hashring,
                           main->capacity * sizeof(*tmp));
    DIE(!tmp, "realloc failed\n");

    *hashring = tmp;
}

void move_objects_between_servers(load_balancer *main, int neigh_pos,
                                  int label, int position)
{
    unsigned int label_hash = hash_function_servers(&label);

    server_memory *neigh = main->hashring[neigh_pos];
    hashtable_t *ht = neigh->objects;

    // we iterate through the buckets of the hashtable stored in the server
    if (neigh_pos != 0 && neigh_pos != 1) {
        for (unsigned int i = 0; i < ht->hmax; i++) {
            // we iterate through the nodes of the linked list of every bucket
            ll_node_t *curr = ht->buckets[i]->head;

            while (curr) {
                info *data = (info *)(curr->data);
                unsigned int to_check_hash = hash_function_key(data->key);

                // check if the key-value pair needs to move to the new server
                if (label_hash > to_check_hash) {
                    server_store(main->hashring[position], data->key,
                                 data->value);

                    // lose the pointer by removing the key from the old server
                    curr = curr->next;
                    server_remove(neigh, data->key);
                    continue;
                }
                curr = curr->next;
            }
        }
    } else if (neigh_pos == 0) {
        for (unsigned int i = 0; i < ht->hmax; i++) {
            // we iterate through the nodes of the linked list of every bucket
            ll_node_t *curr = ht->buckets[i]->head;

            while (curr) {
                info *data = (info *)(curr->data);
                unsigned int to_check_hash = hash_function_key(data->key);

                // check if the key-value pair needs to move to the new server
                if (label_hash > to_check_hash &&
                    to_check_hash > hash_function_servers(&neigh->label)) {
                    server_store(main->hashring[position], data->key,
                                 data->value);

                    curr = curr->next;
                    server_remove(neigh, data->key);
                    continue;
                }
                curr = curr->next;
            }
        }
    } else if (neigh_pos == 1) {
        for (unsigned int i = 0; i < ht->hmax; i++) {
            // we iterate through the nodes of the linked list of every bucket
            ll_node_t *curr = ht->buckets[i]->head;

            while (curr) {
                info *data = (info *)(curr->data);
                unsigned int to_check_hash = hash_function_key(data->key);

                // check if the key-value pair needs to move to the new server
                if (label_hash > to_check_hash ||
                    to_check_hash > hash_function_servers(&neigh->label)) {
                    server_store(main->hashring[position], data->key,
                                 data->value);

                    curr = curr->next;
                    server_remove(neigh, data->key);
                    continue;
                }
                curr = curr->next;
            }
        }
    }
}

void server_put(load_balancer *main, int position, int server_id, int label)
{
    if (main->size + 1 == main->capacity) {
        resize_hashring(&(main->hashring), main, 1);
    }

    if (position == 0 && main->size == 0) {
        main->hashring[position] = init_server_memory();
        main->hashring[position]->id = server_id;
        main->hashring[position]->label = label;
        (main->size)++;
        return;
    }

    // move all elements one position to the right
    // (pointers point to somewhere else)
    for (int i = main->size - 1; i >= position; i--) {
        main->hashring[i + 1] = main->hashring[i];
    }

    main->hashring[position] = init_server_memory();
    main->hashring[position]->id = server_id;
    main->hashring[position]->label = label;

    // we check the clockwise neighbour server
    int neigh_pos;
    if (position == main->size)
        neigh_pos = 0;
    else
        neigh_pos = position + 1;

    move_objects_between_servers(main, neigh_pos, label, position);

    (main->size)++;
}

void loader_add_server(load_balancer *main, int server_id) {
    int replica_label0 = server_id;
    int replica_label1 = REPLICA_FACTOR + server_id;
    int replica_label2 = 2 * REPLICA_FACTOR + server_id;

    int position0 = get_replica_position(main, replica_label0, server_id);
    server_put(main, position0, server_id, replica_label0);

    int position1 = get_replica_position(main, replica_label1, server_id);
    server_put(main, position1, server_id, replica_label1);

    int position2 = get_replica_position(main, replica_label2, server_id);
    server_put(main, position2, server_id, replica_label2);
}

int get_server_for_key(load_balancer *main, unsigned int key_hash)
{
    if (main->size == 0)
        return -1;

    // extremities for the binary search
    int left = 0;
    int right = main->size - 1;

    // the position that the function will return
    int position;

    if (right == 0)
        position = 0;

    while (left < right) {
        int middle = (left + right) / 2;

        server_memory *curr = main->hashring[middle];
        unsigned int to_compare = hash_function_servers(&(curr->label));

        if (key_hash == to_compare) {
            position = middle;
            break;
        }

        if (key_hash > to_compare) {
            left = middle + 1;
            position = left;
        } else {
            right = middle;
            position = right;
        }
    }

    server_memory *curr = main->hashring[position];
    unsigned int to_compare = hash_function_servers(&(curr->label));

    if (key_hash < to_compare) {
        return position;
    }

    // key_hash >= to_compare
    if (position == main->size - 1)
        return 0;
    return (position + 1);
}

void loader_store(load_balancer *main, char *key, char *value, int *server_id)
{
    unsigned int key_hash = hash_function_key(key);

    int position = get_server_for_key(main, key_hash);

    server_store(main->hashring[position], key, value);

    *server_id = main->hashring[position]->id;
}

char *loader_retrieve(load_balancer *main, char *key, int *server_id) {
    unsigned int key_hash = hash_function_key(key);

    int position = get_server_for_key(main, key_hash);

    char *data = server_retrieve(main->hashring[position], key);

    *server_id = main->hashring[position]->id;

    return data;
}

void free_load_balancer(load_balancer *main) {
    for (int i = 0; i < main->size; i++) {
        free_server_memory(main->hashring[i]);
    }
    free(main->hashring);
    free(main);
}

int binary_search_for_remove(load_balancer *main, int label, int server_id)
{
    unsigned int replica_hash = hash_function_servers(&label);

    // extremities for the binary search
    int left = 0;
    int right = main->size - 1;

    while (left <= right) {
        int middle = (left + right) / 2;

        server_memory *curr = main->hashring[middle];
        unsigned int to_compare = hash_function_servers(&(curr->label));

        if (to_compare == replica_hash && server_id == curr->id)
            return middle;

        if (to_compare < replica_hash) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return -1;
}

void server_out(load_balancer *main, int position)
{
    int neigh_pos;
    if (position == main->size - 1)
        neigh_pos = 0;
    else
        neigh_pos = position + 1;

    server_memory *to_remove = main->hashring[position];
    hashtable_t *ht = to_remove->objects;

    // we iterate through the bucket array of the hashtable
    // stored in the server
    for (unsigned int i = 0; i < ht->hmax; i++) {
        // we iterate through the nodes of the linked list of every bucket
        ll_node_t *curr = ht->buckets[i]->head;

        while (curr) {
            info *data = (info *)(curr->data);
            server_store(main->hashring[neigh_pos], data->key, data->value);

            curr = curr->next;
        }
    }

    free_server_memory(main->hashring[position]);

    if (position != main->size - 1) {
        for (int i = position; i < main->size; i++) {
            main->hashring[i] = main->hashring[i + 1];
        }
    }

    (main->size)--;

    // check if a resize is needed
    if (main->size < (main->capacity) / 2)
        resize_hashring(&(main->hashring), main, 0);
}

void loader_remove_server(load_balancer *main, int server_id) {
    // if it is the last server on the ring
    if (main->size == 3) {
        for (int i = 0; i < main->size; i++)
            free_server_memory(main->hashring[i]);
        return;
    }

    int replica_label0 = server_id;
    int replica_label1 = REPLICA_FACTOR + server_id;
    int replica_label2 = 2 * REPLICA_FACTOR + server_id;

    int position0 = binary_search_for_remove(main, replica_label0, server_id);
    server_out(main, position0);

    int position1 = binary_search_for_remove(main, replica_label1, server_id);
    server_out(main, position1);

    int position2 = binary_search_for_remove(main, replica_label2, server_id);
    server_out(main, position2);
}
