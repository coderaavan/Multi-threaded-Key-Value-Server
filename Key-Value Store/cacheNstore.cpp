#include "cacheNstore.hpp"
extern int cache_size;
extern int type;
extern bst_t *root;
extern int rec_file;
int rec_count = 0;
int file_count = 0;
int rec_no = 0;

vector<node_t> cache;
vector<int> fd_arr;

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
  cout<<"Cache type at cacheNstore: "<<type<<endl;
  if(type==1){
    cout<<"LRU"<<endl;
    if(cache.size() == cache_size){
        node_t last = cache[cache.size()-1];
        cache.pop_back();
        umap.erase(last.key);
        string keyString = last.key;
        keyString = keyString.append(256-keyString.length(),'#');
        MD5 md5;
        bst_t *del_node = bst_find(root, md5(keyString));
        cout<<"Successfully executed find for offloading key"<<endl;
        if(del_node!=NULL){
          cout<<"Found in store. Deleting."<<endl;
          root = bst_delete(root, md5(keyString));
        }
        persist(last);
      }
    node->frequency = 1;
    cache.insert(cache.begin(), *node);
    unordered_map<string, int> :: iterator it;
    for(it = umap.begin(); it!=umap.end(); it++){
      (it->second)++;
    }
    umap[node->key]=0;
  //  cout<<"Value inserted\n";
  }
  else if(type==2){
    cout<<"LFU"<<endl;
    if(cache.size() == cache_size){
      persist(cache[0]);
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
    //cout<<"Value inserted\n";
  }
}

bool cache_delete(string key){
  if(umap.find(key) != umap.end()){
    cout<<"Found in cache. Deleting."<<endl;
    int it = umap[key];
    umap.erase(key);
    //cout<<"Succesfully erased from map"<<endl;
    cache.erase(cache.begin() + it);
    //cout<<"Succesfully erased from cache"<<endl;
    return true;
  }
  return false;
}

metadata_t *md_create(string key, int file, int offset){
  metadata_t *md = new metadata_t;
  md->key_hash = key;
  md->file = file;
  md->offset = offset;
  return md;
}

bst_t *bst_node(metadata_t *data){
  bst_t *node = new bst_t;
  node->data = data;
  node->left = NULL;
  node->right = NULL;
  return node;
}

bst_t *bst_insert(bst_t* root, bst_t* node){
  int s = sizeof(node->data->key_hash);
  //cout<<s<<endl;
  char root_key[s+1];
  char node_key[s+1];
  if(root != NULL){
    strcpy(root_key,(root->data->key_hash).c_str());
    strcpy(node_key,(node->data->key_hash).c_str());
    root_key[s]='\0';
    node_key[s]='\0';
  }
  if(root == NULL){
    root = node;
  }
  else if(strcmp(root_key,node_key)>0){
    root->left = bst_insert(root->left,node);
  }
  else{
    root->right = bst_insert(root->right,node);
  }
  return root;
}

bst_t *bst_find(bst_t* root, string key){
  int s = sizeof(key);
  char key_buf[s+1];
  char root_key[s+1];
  strcpy(key_buf,key.c_str());
  key_buf[s] = '\0';
  if(root!=NULL){
    strcpy(root_key,(root->data->key_hash).c_str());
    root_key[s]='\0';
  }
  if(root == NULL || strcmp(root_key,key_buf)==0){
    return root;
  }
  else if(strcmp(root_key,key_buf)>0){
    return bst_find(root->left,key);
  }
  else return bst_find(root->right,key);
}

bst_t *minValue(bst_t* root){
  //cout<<"Entered minValue\n";
  bst_t *current = root;
  while(current && current->left!=NULL)
    current = current->left;

  return current;
}

bst_t *bst_delete(bst_t* root, string key){
  int s = sizeof(key);
  char key_buf[s+1];
  char root_key[s+1];
  strcpy(key_buf,key.c_str());
  key_buf[s] = '\0';
  if(root!=NULL){
    strcpy(root_key,(root->data->key_hash).c_str());
    root_key[s]='\0';
  }
  if(root==NULL) return root;

  if(strcmp(key_buf,root_key)>0){
    root->right = bst_delete(root->right, key);
  }
  else if(strcmp(key_buf,root_key)<0){
    root->left = bst_delete(root->left, key);
  }

  else{
    if(root->left==NULL){
      //cout<<"Entered left == NULL for deleting key: "<<key<<endl;
      bst_t *temp = root->right;
      free(root->data);
      free(root);
      return temp;
    }
    else if(root->right==NULL){
      //cout<<"Entered right == NULL for deleting key: "<<key<<endl;
      bst_t *temp = root->left;
      free(root->data);
      free(root);
      return temp;
    }
  //  cout<<"Entered left != NULL and right != NULL for deleting key: "<<key<<endl;
    bst_t *temp = minValue(root->right);
    root->data = temp->data;
    root->right = bst_delete(root->right,temp->data->key_hash);
  }
  return root;
}

void persist(node_t node){
  //cout<<"New Entry\n";
  string key = node.key;
  string value = node.value;
  if(rec_count==0){
    file_count++;
    string fname = to_string(file_count);
    char name[fname.length()+1];
    strcpy(name,fname.c_str());
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL | O_APPEND, 0666);
    fd_arr.push_back(fd);
  }
  int fd = fd_arr[fd_arr.size() - 1];
  key = key.append(256-key.length(),'#');
  value = value.append(256-value.length(),'!');
  string key_value = key + " " + value + "\n";
  char buf[key_value.length()];
  strcpy(buf,key_value.c_str());
  int write_ret = write(fd,buf,key_value.length());
  int offset = lseek(fd,-514,SEEK_CUR);
  MD5 md5;
  string key_hash = md5(key);
  //cout<<"Hash of key: "<<key<<" is "<<key_hash<<endl;
  root = bst_insert(root, bst_node(md_create(key_hash,fd,offset)));
  rec_count++;
  if(rec_count == rec_file)
    rec_count=0;
}

void close_fd(){
  for(int i = 0; i<fd_arr.size(); i++){
    if(close(fd_arr[i])==-1){
      //cout<<"Error closing file "<<(i+1)<<endl;
    }
  }
}
