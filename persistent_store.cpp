#include<bits/stdc++.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include"md5.h"

#define RECORDS_IN_FILE 3

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

bst_t *root = NULL;
int rec_count = 0;
int file_count = 0;
int rec_no = 0;

void printInorder(bst_t* node)
{
    if (node == NULL)
        return;
    rec_no++;
    /* first recur on left child */
    printInorder(node->left);

    /* then print the data of node */
  //  cout << node->data << " ";

    /* now recur on right child */
    printInorder(node->right);
}

vector<int> fd_arr;

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
  cout<<s<<endl;
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
  cout<<"Entered minValue\n";
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
      cout<<"Entered left == NULL for deleting key: "<<key<<endl;
      bst_t *temp = root->right;
      /*char junk[514];
      memset(junk,'#',513);
      junk[513]='\n';
      lseek(root->data->file, root->data->offset, SEEK_SET);
      write(root->data->file,junk,514);*/
      free(root->data);
      free(root);
      return temp;
    }
    else if(root->right==NULL){
      cout<<"Entered right == NULL for deleting key: "<<key<<endl;
      bst_t *temp = root->left;
      /*char junk[514];
      memset(junk,'#',513);
      junk[513]='\n';
      lseek(root->data->file, root->data->offset, SEEK_SET);
      write(root->data->file,junk,514);*/
      free(root->data);
      free(root);
      return temp;
    }
    cout<<"Entered left != NULL and right != NULL for deleting key: "<<key<<endl;
    bst_t *temp = minValue(root->right);
    root->data = temp->data;
    root->right = bst_delete(root->right,temp->data->key_hash);
  }
  return root;
}

void persist(node_t node){
  cout<<"New Entry\n";
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
/*  if(fd_arr.size()==0){
    file_count++;
    string fname = to_string(file_count);
    char name[fname.length()+1];
    strcpy(name,fname.c_str());
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL | O_APPEND, 0666);
    if(fd==-1){
      fd = open(name, O_RDWR | O_APPEND, 0666);
    }
    if(fd==-1){
      cout<<"Error Opening File\n";
    }
    fd_arr.insert(fd_arr.begin(),fd);
  } */
  int fd = fd_arr[fd_arr.size() - 1];
  string key_value = key + " " + value + "\n";
  char buf[key_value.length()];
  strcpy(buf,key_value.c_str());
  int write_ret = write(fd,buf,key_value.length());
  int offset = lseek(fd,-514,SEEK_CUR);
  MD5 md5;
  string key_hash = md5(key);
  root = bst_insert(root, bst_node(md_create(key_hash,fd,offset)));
  rec_count++;
  if(rec_count == RECORDS_IN_FILE)
    rec_count=0;
}

int main(){
  string key[12] = {"1", "4", "3", "7", "8", "9", "22", "14", "19", "21", "16", "17"};
  string values[12] = {"1", "4", "3", "7", "8", "9", "22", "14", "19", "21", "16", "17"};
  for(int i=0;i<12;i++){
    key[i].append(256-key[i].length(),'.');
    values[i].append(256-values[i].length(),';');
  }
  for(int i=0;i<12;i++){
    node_t insert;
    insert.key = key[i];
    insert.value = values[i];
    persist(insert);
  }
  for(int i=0;i<12;i++){
    MD5 md5;
    bst_t *node = bst_find(root,md5(key[i]));
    if(node){
      char key[257], value[257], key_value[514];
      lseek(node->data->file, node->data->offset, SEEK_SET);
      read(node->data->file,key_value,514);
      key_value[513] = '\0';
      memcpy(key, &key_value[0], 256);
      key[256] = '\0';
      memcpy(value, &key_value[257], 256);
      value[256] = '\0';
      cout<<"Key is: "<<key<<endl;
      cout<<"Value is: "<<value<<endl;
    }
    else{
      cout<<"Record not in files\n";
    }
  }
  MD5 md5;
  root = bst_delete(root,md5(key[0]));
  root = bst_delete(root,md5(key[5]));
  root = bst_delete(root,md5(key[8]));
  printInorder(root);
  cout<<"Number of records: "<<rec_no<<endl;
  cout<<"Memory covered by bst nodes: "<<rec_no*sizeof(bst_t)<<endl;
  cout<<"Memory covered in total: "<<rec_no*(sizeof(bst_t) + sizeof(metadata_t))<<endl;
  return 0;
}
