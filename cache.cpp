#include<bits/stdc++.h>
using namespace std;

int cache_size = 5;
int type = 2;

struct node_t{
  string key;
  string value;
  int frequency;
};

vector<node_t> cache;
unordered_map<string, int> umap;

void swap(node_t &node1, node_t &node2){
  node_t temp = node1;
  node1 = node2;
  node2 = temp;
}

int parent(int i){
  return (i-1)/2;
}

int left(int i){
  return 2*i + 1;
}

int right(int i){
  return 2*i + 2;
}

void heapify(int i){
  int l = left(i), r = right(i), min_index;
  if(l < cache.size()){
    min_index = (cache[i].frequency < cache[l].frequency) ? i : l;
  }
  else
    min_index = i;
  if(r < cache.size()){
    min_index = (cache[min_index].frequency < cache[r].frequency) ? min_index : r;
  }
  if(min_index != i){
    umap[cache[min_index].key] = i;
    umap[cache[i].key] = min_index;
    swap(cache[min_index],cache[i]);
    heapify(min_index);
  }
}

void inc_freq(int i){
  (cache[i].frequency)++;
  heapify(i);
}

string cache_find(string key){
  if(umap.find(key) != umap.end()){
    int it = umap[key];
    string value = cache[it].value;
    if(type == 2){
      inc_freq(it);
    }
    return value;
  }
  return "";
}

void cache_insert(node_t *node){
  if(type==1){
    if(cache.size() == cache_size){
        node_t last = cache[cache.size()-1];
        cache.pop_back();
        umap.erase(last.key);
      }

    node->frequency = 1;
    cache.insert(cache.begin(), *node);
    unordered_map<string, int> :: iterator it;
    for(it = umap.begin(); it!=umap.end(); it++){
      (it->second)++;
    }
    umap[node->key]=0;
    cout<<"Value inserted\n";
  }
  else if(type==2){
    if(cache.size() == cache_size){
      umap.erase(cache[0].key);
      cache[0] = cache[cache.size()-1];
      cache.pop_back();
      heapify(0);
    }
    node->frequency = 1;
    cache.insert(cache.begin() + cache.size(),*node);
    umap[node->key] = cache.size()-1;
    int i = cache.size()-1;
    while(i && cache[parent(i)].frequency > cache[i].frequency){
      umap[cache[i].key] = parent(i);
      umap[cache[parent(i)].key] = i;
      swap(cache[i],cache[parent(i)]);
      i = parent(i);
    }
    cout<<"Value inserted\n";
  }
}

int main(){
  string key[5] = {"Prashant", "Dev", "Nishant", "Pooja", "Seema"};
  string values[5] = {"23", "4", "33", "28", "50"};
  for(int j=0;j<3;j++){
    for(int i=0;i<5;i++){
      string value = cache_find(key[i]);
      if(value == ""){
        node_t insert;
        insert.key = key[i];
        insert.value = values[i];
        cache_insert(&insert);
      }
      else{
        cout<<"Key is "<<key[i]<<" value is "<<value<<endl;
      }
    }
  }
  return 0;
}
