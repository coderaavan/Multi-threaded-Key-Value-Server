#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdio>

using namespace std;

// Maximum number of entries that can be present in the cache at a time
extern int NUMBER_OF_ENTRIES_IN_CACHE;

// Structure of a cache/storage entry
typedef struct key_value_entry
{
    char key[257];
    char value[257];
    int frequency;
    int isValid;
} KeyValueEntry;

//void displayFile();
KeyValueEntry *search_on_storage(string key);
void insert_on_storage(KeyValueEntry *keyValueEntry);
int delete_from_storage(string key);
