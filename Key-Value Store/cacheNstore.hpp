#include<bits/stdc++.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include"md5.h"
using namespace std;

struct node_t{
  string key;
  string value;
  int frequency;
};
struct metadata_t{
  string key_hash;
  int file;
  int offset;
};

struct bst_t{
  metadata_t *data;
  bst_t *left, *right;
};

void swap(node_t &node1, node_t &node2);
int parent(int i);
int left(int i);
int right(int i);
void heapify(int i);
void inc_freq(int i);
string cache_find(string key);
void cache_insert(node_t *node);
bool cache_delete(string key);
metadata_t *md_create(string key, int file, int offset);
bst_t *bst_node(metadata_t *data);
bst_t *bst_insert(bst_t* root, bst_t* node);
bst_t *bst_find(bst_t* root, string key);
bst_t *minValue(bst_t* root);
bst_t *bst_delete(bst_t* root, string key);
void persist(node_t node);
