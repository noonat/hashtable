#include <string.h>
#include "table.h"
#include "munit.h"

int mu_num_asserts;
int mu_num_failures;
int mu_num_tests;

static hcode_t dummy_hash_func(const htype_t type, const hvalue_t value) {
  return 0;
}

static int32_t dummy_hash_equal_func(const htype_t type, const hvalue_t a, const hvalue_t b) {
  return 0;
}

static void test_table_init() {
  table_t t;
  memset(&t, 0, sizeof(table_t));
  t.nodes_size_log = -1;
  table_init(&t, H_STRING, NULL, NULL);
  mu_assert(t.nodes != 0);
  mu_assert(t.nodes_size_log == 0);
  mu_assert(t.inactive_node != 0);
  mu_assert(t.hash_type == H_STRING);
  mu_assert(t.hash_func == hash_code);
  mu_assert(t.hash_equal_func == hash_equal);
}

// Table should allow custom hash and hash key equality functions.
static void test_table_init_with_funcs() {
  table_t t;
  memset(&t, 0, sizeof(table_t));
  t.nodes_size_log = -1;
  table_init(&t, H_INT32, dummy_hash_func, dummy_hash_equal_func);
  mu_assert(t.nodes != 0);
  mu_assert(t.nodes_size_log == 0);
  mu_assert(t.inactive_node != 0);
  mu_assert(t.hash_type == H_INT32);
  mu_assert(t.hash_func == dummy_hash_func);
  mu_assert(t.hash_equal_func == dummy_hash_equal_func);
}

// New tables should start with table.nodes pointing to the same dummy nodes
// object. Nodes are allocated the first time the table is used.
static void test_table_init_dummy_nodes() {
  table_t t1;
  table_t t2;
  table_init(&t1, H_STRING, NULL, NULL);
  table_init(&t2, H_STRING, NULL, NULL);
  mu_assert(t1.nodes == t2.nodes);
}

static void test_table_set() {
  table_t t;
  table_node_t *n;
  hvalue_t k = (hvalue_t)"foo";
  hvalue_t v = (hvalue_t)1;
  
  table_init(&t, H_STRING, NULL, NULL);
  
  n = t.nodes;
  mu_assert(t.nodes_size_log == 0);
  table_set(&t, k, v);
  mu_assert(t.nodes_size_log == 0);
  mu_assert(t.nodes != n);
  mu_assert(t.inactive_node == t.nodes + 1);
  
  n = t.nodes;
  mu_assert(n->key == k);
  mu_assert(n->value == v);
  mu_assert(n->next == NULL);
  mu_assert(n->active == 1);
}

// Table should be resized by powers of two, as needed.
static void test_table_set_resize() {
  table_t t;
  table_node_t *n;
  
  table_init(&t, H_STRING, NULL, NULL);
  
  n = t.nodes;
  mu_assert(t.nodes_size_log == 0);
  table_set(&t, (hvalue_t)"foo", 1);
  mu_assert(t.nodes_size_log == 0);
  mu_assert(t.nodes != n);
  
  n = t.nodes;
  table_set(&t, (hvalue_t)"bar", 2);
  mu_assert(t.nodes_size_log == 1);
  mu_assert(t.nodes != n);
  
  n = t.nodes;
  table_set(&t, (hvalue_t)"baz", 3);
  mu_assert(t.nodes_size_log == 2);
  mu_assert(t.nodes != n);
  
  n = t.nodes;
  table_set(&t, (hvalue_t)"hoge", 4);
  mu_assert(t.nodes_size_log == 2);
  mu_assert(t.nodes == n);

  n = t.nodes;
  table_set(&t, (hvalue_t)"fuga", 5);
  mu_assert(t.nodes_size_log == 3);
  mu_assert(t.nodes != n);

  n = t.nodes;
  table_set(&t, (hvalue_t)"piyo", 6);
  mu_assert(t.nodes_size_log == 3);
  mu_assert(t.nodes == n);
  
  table_destroy(&t);
}

static void test_table_get() {
  table_t t;
  
  table_init(&t, H_STRING, NULL, NULL);

  mu_assert(0 == table_get(&t, (hvalue_t)"foo"));
  table_set(&t, (hvalue_t)"foo", 1);
  mu_assert(1 == table_get(&t, (hvalue_t)"foo"));
  
  mu_assert(0 == table_get(&t, (hvalue_t)"bar"));
  table_set(&t, (hvalue_t)"bar", 2);
  mu_assert(1 == table_get(&t, (hvalue_t)"foo"));
  mu_assert(2 == table_get(&t, (hvalue_t)"bar"));
  
  table_set(&t, (hvalue_t)"baz", 3);
  table_set(&t, (hvalue_t)"hoge", 4);
  table_set(&t, (hvalue_t)"fuga", 5);
  table_set(&t, (hvalue_t)"piyo", 6);
  mu_assert(1 == table_get(&t, (hvalue_t)"foo"));
  mu_assert(2 == table_get(&t, (hvalue_t)"bar"));
  mu_assert(3 == table_get(&t, (hvalue_t)"baz"));
  mu_assert(4 == table_get(&t, (hvalue_t)"hoge"));
  mu_assert(5 == table_get(&t, (hvalue_t)"fuga"));
  mu_assert(6 == table_get(&t, (hvalue_t)"piyo"));
  
  table_destroy(&t);
}

static void test_table_contains() {
  table_t t;
  table_init(&t, H_STRING, NULL, NULL);
  mu_assert(0 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(0 == table_contains(&t, (hvalue_t)"bar"));
  table_set(&t, (hvalue_t)"foo", 1);
  mu_assert(1 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(0 == table_contains(&t, (hvalue_t)"bar"));
  table_set(&t, (hvalue_t)"bar", 2);
  mu_assert(1 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(1 == table_contains(&t, (hvalue_t)"bar"));
  table_destroy(&t);
}

static void test_table_delete() {
  table_t t;
  table_init(&t, H_STRING, NULL, NULL);
  mu_assert(0 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(0 == table_contains(&t, (hvalue_t)"bar"));
  table_set(&t, (hvalue_t)"foo", 1);
  mu_assert(1 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(0 == table_contains(&t, (hvalue_t)"bar"));
  table_set(&t, (hvalue_t)"bar", 2);
  mu_assert(1 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(1 == table_contains(&t, (hvalue_t)"bar"));
  table_delete(&t, (hvalue_t)"foo");
  mu_assert(0 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(1 == table_contains(&t, (hvalue_t)"bar"));
  table_set(&t, (hvalue_t)"foo", 1);
  mu_assert(1 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(1 == table_contains(&t, (hvalue_t)"bar"));
  table_delete(&t, (hvalue_t)"bar");
  mu_assert(1 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(0 == table_contains(&t, (hvalue_t)"bar"));
  table_delete(&t, (hvalue_t)"foo");
  mu_assert(0 == table_contains(&t, (hvalue_t)"foo"));
  mu_assert(0 == table_contains(&t, (hvalue_t)"bar"));
  table_destroy(&t);
}

static void test_table_error_string() {
  mu_assert(0 == strcmp(table_error_string(TABLE_ERROR_NONE), "none"));
  mu_assert(0 == strcmp(table_error_string(TABLE_ERROR_MEMORY), "out of memory"));
  mu_assert(0 == strcmp(table_error_string(TABLE_ERROR_OVERFLOW), "table size too large"));
  mu_assert(0 == strcmp(table_error_string(MAX_TABLE_ERROR), "unknown error"));
}

int main(int argc, char **argv) {
  mu_test(test_table_init);
  mu_test(test_table_init_with_funcs);
  mu_test(test_table_init_dummy_nodes);
  mu_test(test_table_set);
  mu_test(test_table_set_resize);
  mu_test(test_table_get);
  mu_test(test_table_contains);
  mu_test(test_table_delete);
  mu_test(test_table_error_string);
  mu_print_results();
  return mu_num_failures != 0;
}
