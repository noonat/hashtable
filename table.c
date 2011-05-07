#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"

#define MAX_SIZE_BITS 24

static const table_node_t _dummy_node = {
  0, 0, NULL, 0
};

#define _dummy_node_ptr ((table_node_t *)&_dummy_node)

static int32_t _log2(uint32_t n);
static inline int32_t _ceil_log2(uint32_t n);
static inline uint32_t _table_mod(table_t *table, int32_t i);
static table_node_t *_table_nodes_alloc(int32_t count);
static void _table_nodes_free(table_node_t *nodes);
static int32_t _table_nodes_count(table_t *table);
static void _table_nodes_resize(table_t *table, int32_t additional);
static table_node_t *_table_node_find_inactive(table_t *table);
static table_node_t *_table_node_find_key(table_t *table, hvalue_t key, table_node_t **prev);
static table_node_t *_table_node_insert_key(table_t *table, hvalue_t key);

static int32_t _log2(uint32_t n) {
  static const unsigned char log2_table[256] = {
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int32_t l = -1;
  while (n >= 256) {
    l += 8;
    n >>= 8;
  }
  return l + log2_table[n];
}

static inline int32_t _ceil_log2(uint32_t n) {
  return _log2(n - 1) + 1;
}

static inline uint32_t _table_mod(table_t *table, int32_t i) {
  int32_t size = 1 << table->nodes_size_log;
  assert((size & (size - 1)) == 0);  // make sure the size is a power of 2
  return i & (size - 1);
}

static table_node_t *_table_nodes_alloc(int32_t count) {
  size_t size = sizeof(table_node_t) * count;
  table_node_t *nodes = (table_node_t *)malloc(size);
  if (nodes == NULL) {
    // FIXME: error handling
  }
  memset(nodes, 0, size);
  return nodes;
}

static void _table_nodes_free(table_node_t *nodes) {
  if (nodes != NULL && nodes != _dummy_node_ptr) {
    free(nodes);
  }
}

/**
* Return the number of active (non-free) nodes in the hash table.
*/
static int32_t _table_nodes_count(table_t *table) {
  int32_t count = 0;
  int32_t i = (1 << table->nodes_size_log) - 1;
  while (i >= 0) {
    table_node_t *node = &table->nodes[i--];
    if (node->active) {
      ++count;
    }
  }
  return count;
}

/**
* Resize the hash table. This will use round the desired size up to the next
* power of two. Existing nodes will be redistributed across the new array.
*/
static void _table_nodes_resize(table_t *table, int32_t additional) {
  table_node_t *old_nodes = table->nodes;
  int32_t old_size_log = table->nodes_size_log;
  int32_t old_size = 1 << old_size_log;
  int32_t new_size = _table_nodes_count(table) + additional;
  int32_t i;
  
  // Allocate a new node array for the table
  if (new_size == 0) {
    table->nodes_size_log = 0;
    table->nodes = _dummy_node_ptr;
  } else {
    int32_t new_size_log = _ceil_log2(new_size);
    if (new_size_log > MAX_SIZE_BITS) {
      // FIXME: error handling: table overflow
    }
    new_size = 1 << new_size_log;
    table->nodes_size_log = new_size_log;
    table->nodes = _table_nodes_alloc(new_size);
  }
  table->inactive_node = &table->nodes[new_size];
  
  // Copy the old nodes into the new table
  if (old_nodes != NULL && old_nodes != _dummy_node_ptr) {
    for (i = old_size - 1; i >= 0; --i) {
      table_node_t *old = &old_nodes[i];
      if (old->active) {
        table_set(table, old->key, old->value);
      }
    }
    _table_nodes_free(old_nodes);
  }
}

/**
* Remove a node from the table. This marks the node as inactive, and updates
* linked lists as necesary. If the node was at it's proper index, and the
* next node's proper index matches, the next node will be moved up.
*/
static void _table_node_delete(table_t *table, table_node_t *node, table_node_t *prev) {
  table_node_t *next = node->next;
  if (prev != NULL) {
    prev->next = next;
  } else if (next != NULL) {
    hcode_t next_hash = table->hash_func(table->hash_type, next->key);
    table_node_t *next_node = &table->nodes[_table_mod(table, next_hash)];
    if (next_node == node) {
      // Next node's proper place is the node we're removing, so move it
      node->key = next->key;
      node->value = next->value;
      node->next = next->next;
      node = next;
    }
  }
  node->active = 0;
  node->key = 0;
  node->value = 0;
  node->next = NULL;
  if (table->inactive_node <= node) {
    table->inactive_node = node + 1;
  }
}

/**
* Return a node from the table's free list, or NULL if there are no free
* nodes remaining in the table.
*/
static table_node_t *_table_node_find_inactive(table_t *table) {
  while (table->inactive_node-- > table->nodes) {
    if (!table->inactive_node->active) {
      return table->inactive_node;
    }
  }
  return NULL;
}

/**
* Return the node in the hash table with a matching key, or NULL if no
* matching node is found. If the table->key_func is set, the function will
* be used to compare keys. Otherwise, the key pointers will be compared.
*
* @param prev If defined, a pointer to the previous node will be stored here.
*/
static table_node_t *_table_node_find_key(table_t *table, hvalue_t key, table_node_t **prev) {
  int32_t hash = table->hash_func(table->hash_type, key);
  table_node_t *node = &table->nodes[_table_mod(table, hash)];
  table_node_t *prev_node = NULL;
  if (table->hash_equal_func != NULL) {
    while (node != NULL) {
      if (table->hash_equal_func(table->hash_type, node->key, key)) {
        break;
      }
      prev_node = node;
      node = node->next;
    }
  } else {
    while (node != NULL) {
      if (node->key == key) {
        break;
      }
      prev_node = node;
      node = node->next;
    }
  }
  if (prev != NULL) {
    *prev = prev_node;
  }
  return node;
}

/**
* Insert the key into the hash table, and return the new node. The table will
* be resized if there are no free nodes remaining.
*
* The new key is stored at the correct hash index whenever possible. If there,
* is a collision, colliding keys whose hash indexes do not match their
* current index will be moved to a new free node. Otherwise, the new key will
* itself be stored in a free node.
*/
static table_node_t *_table_node_insert_key(table_t *table, hvalue_t key) {
  int32_t hash = table->hash_func(table->hash_type, key);
  table_node_t *node = &table->nodes[_table_mod(table, hash)];
  if (node->active || node == _dummy_node_ptr) {
    // Collision, there's already a node in the slot
    int32_t other_hash;
    table_node_t *other_node;
    table_node_t *new_node = _table_node_find_inactive(table);
    if (new_node == NULL) {
      // Couldn't find a free node, resize the table and try again
      _table_nodes_resize(table, 1);
      return _table_node_insert_key(table, key);
    }
    assert(new_node != _dummy_node_ptr);
    other_hash = table->hash_func(table->hash_type, node->key);
    other_node = &table->nodes[_table_mod(table, other_hash)];
    if (other_node != node) {
      // Colliding node isn't in its proper place, move it so we can use
      while (other_node->next != node) {
        other_node = other_node->next;
      }
      other_node->next = new_node;
      new_node->active = 1;
      new_node->key = node->key;
      new_node->value = node->value;
      new_node->next = node->next;
      node->next = NULL;
      node->value = 0;
    } else {
      // Colliding node is in the right place, use the free node instead
      new_node->next = node->next;
      node->next = new_node;
      node = new_node;
    }
  }
  node->active = 1;
  node->key = key;
  return node;
}

void table_init(table_t *table, htype_t type, hash_func_t fn, hash_equal_func_t eq_fn) {
  assert(type != H_NULL);
  table->nodes = _dummy_node_ptr;
  table->nodes_size_log = 0;
  table->inactive_node = _dummy_node_ptr;
  table->hash_type = type;
  table->hash_func = fn ? fn : hash_code;
  table->hash_equal_func = eq_fn ? eq_fn : hash_equal;
}

void table_destroy(table_t *table) {
  _table_nodes_free(table->nodes);
  table->nodes = NULL;
}

hvalue_t table_get(table_t *table, hvalue_t key) {
  table_node_t *node = _table_node_find_key(table, key, NULL);
  return node != NULL ? node->value : 0;
}

table_node_t *table_set(table_t *table, hvalue_t key, hvalue_t value) {
  table_node_t *node = _table_node_find_key(table, key, NULL);
  if (node == NULL) {
    node = _table_node_insert_key(table, key);
  }
  node->value = value;
  return node;
}

int32_t table_contains(table_t *table, hvalue_t key) {
  table_node_t *node = _table_node_find_key(table, key, NULL);
  return node != NULL && node->active;
}

void table_delete(table_t *table, hvalue_t key) {
  table_node_t *prev;
  table_node_t *node = _table_node_find_key(table, key, &prev);
  if (node != NULL) {
    _table_node_delete(table, node, prev);
  }
}
