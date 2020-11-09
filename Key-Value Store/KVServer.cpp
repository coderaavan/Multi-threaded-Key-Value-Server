#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <vector>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <map>
#include <string>
#include "cacheNstore.hpp"

using namespace std;

bst_t *root = NULL;
int type;
int cache_size;
int rec_file;

// States codes for GET, PUT and DEL requests as well as for SUCCESS and ERROR responses
enum STATUS_CODES {GET_STATUS_CODE = 1, PUT_STATUS_CODE = 2, DEL_STATUS_CODE = 3, SUCCESS_STATUS_CODE = 200, ERROR_STATUS_CODE = 240};

// Defining the name of server configuration file
#define SERVER_CONFIG_FILE "server.config"

// Message length (in bytes)
#define MESSAGE_LENGTH 513

// Key/Value maximum length (in bytes)
#define KEY_VALUE_MAX_LENGTH 256

// Key-Value Storage
map<string, string> keyValueCache;

// This structure defines the information a worker thread needs about a client
typedef struct client_info
{
    int connectionFD;           // Holds the file descriptor returned by accept call
    struct sockaddr_in addr;
    struct sockaddr_in client;
    socklen_t addrlen;
}ClientInfo;

// This structure will be used to pass the necessary arguments on thread creation
struct worker_args
{
    int pendingRequestsQueueIndex;
};

// These values will be replaced by the ones present in the server.config file

int SERVER_LISTENING_PORT = -1;        // Port number on which the server is listening

int INITIAL_THREAD_POOL_SIZE = -1;     // Initial number of threads in the thread pool

int THREAD_QUEUE_SIZE = -1;            // Maximum number of clients a thread can support

int THREAD_POOL_GROWTH_SIZE = -1;      // Number of new threads to be added to the thread pool if it becomes full


// This vector holds the pending accepted connections to be handled by each worker thread.
vector<vector<ClientInfo *> > pendingRequestsQueue;

// This vector holds the number of clients each thread is handling
vector<int> liveClientCounter;

void *worker_thread(void *workerArgs)
{
    struct worker_args *workerThreadArgs = (struct worker_args *) workerArgs;
    int positionInQueueVector = workerThreadArgs->pendingRequestsQueueIndex;
    char buffer[MESSAGE_LENGTH];

    // Setting up epoll context
    struct epoll_event events[THREAD_QUEUE_SIZE];

    int ePollFD = epoll_create(THREAD_QUEUE_SIZE);

    // Continuously adding new connections to the context (if any) and monitoring the epoll context
    while (1)
    {
        while (!pendingRequestsQueue[positionInQueueVector].empty())
        {
            static struct epoll_event ev;
            // Reading then connection info from the queue
            ClientInfo *tempInfo = pendingRequestsQueue[positionInQueueVector].front();
            ev.data.fd = tempInfo->connectionFD;
            ev.events = EPOLLIN;
            epoll_ctl(ePollFD, EPOLL_CTL_ADD, ev.data.fd, &ev);

            // Removing the entry from pending requests queue
            pendingRequestsQueue[positionInQueueVector].erase(pendingRequestsQueue[positionInQueueVector].begin());
        }

        int nfds = epoll_wait(ePollFD, events, THREAD_QUEUE_SIZE, NULL);

        for (int i = 0; i < nfds; i++)
        {
            memset(buffer, 0, MESSAGE_LENGTH);
            int n = read(events[i].data.fd, buffer, MESSAGE_LENGTH);

            if (n == 0)
            {
                // The client has disconnected so we officially close the connection
                close(events[i].data.fd);
                continue;
            }
            else
            {
                char responseMessage[MESSAGE_LENGTH];

                // This is done to make the padding process easy
                for (int j = 0; j < MESSAGE_LENGTH; j++)
                {
                    responseMessage[j] = '\0';
                }

                // Fetching the status code from the request message
                int statusCode = buffer[0] & 255;

                if (statusCode == GET_STATUS_CODE)
                {
                    // This means that the request is a 'GET' request
                    cout<<"Entered GET request"<<endl;
                    char key[KEY_VALUE_MAX_LENGTH + 1];
                    key[KEY_VALUE_MAX_LENGTH] = '\0';

                    // Fetching key from the request message stored in the buffer
                    for (int j = 0; j < KEY_VALUE_MAX_LENGTH; j++)
                    {
                        key[j] = buffer[j + 1];
                    }

                    string keyString(key);

                    string value = cache_find(keyString);

                    // Checking whether the key exists in the storage or not
                    if (value != "")
                    {
                        cout<<"Value found in cache"<<endl;
                        // This means that the key exists in the storage, so we send the corresponding value
                        responseMessage[0] = SUCCESS_STATUS_CODE;

                        // Fetching the value corresponding to the key from the map
                        //string valueString = keyValueCache[keyString];

                        for (int j = 0; j < value.length(); j++)
                        {
                            responseMessage[KEY_VALUE_MAX_LENGTH + 1 + j] = value.at(j);
                        }

                        // Sending the response to the client
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                    }
                    else if(value == ""){
                      cout<<"Value not found in cache"<<endl;
                      MD5 md5;
                      string keyOrg = keyString;
                      keyString = keyString.append(256-keyString.length(),'#');
                      cout<<md5(keyString);
                      bst_t *meta_node = bst_find(root,md5(keyString));
                      if(meta_node == NULL){
                        cout<<"Value not found in persistent storage too"<<endl;
                        // If the key doesn't exist, then send a response indicating error
                        responseMessage[0] = ERROR_STATUS_CODE;

                        // Sending the response to the client indicating that an error has occurred
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                      }
                      else{
                        cout<<"Value found in persistent storage"<<endl;
                        char val[257], key_value[514];
                        lseek(meta_node->data->file, meta_node->data->offset, SEEK_SET);
                        read(meta_node->data->file,key_value,514);
                        cout<<key_value<<endl;
                        memcpy(val, &key_value[257], 256);
                        val[256] = '\0';
                        for(int i = 0; i<257 ; i++){
                          if(val[i]=='!'){
                            val[i]='\0';
                            break;
                          }
                        }
                        string valString(val);
                        node_t *insert = new node_t;
                        insert->key = keyOrg;
                        insert->value = valString;
                        insert->frequency = 1;
                        cout<<keyOrg<<endl;
                        cache_insert(insert);
                        // This means that the key exists in the storage, so we send the corresponding value
                        responseMessage[0] = SUCCESS_STATUS_CODE;

                        // Fetching the value corresponding to the key from the map
                        //string valueString = keyValueCache[keyString];
                        for (int j = 0; j < valString.length(); j++)
                        {
                            responseMessage[KEY_VALUE_MAX_LENGTH + 1 + j] = valString.at(j);
                        }

                        // Sending the response to the client
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                      }

                    }
                }
                else if (statusCode == PUT_STATUS_CODE)
                {
                    // This means that the request is a 'PUT' request

                    char key[KEY_VALUE_MAX_LENGTH + 1];
                    char value[KEY_VALUE_MAX_LENGTH + 1];
                    key[KEY_VALUE_MAX_LENGTH] = '\0';
                    value[KEY_VALUE_MAX_LENGTH] = '\0';

                    // Fetching key from the request message stored in the buffer
                    for (int j = 0; j < KEY_VALUE_MAX_LENGTH; j++)
                    {
                        key[j] = buffer[j + 1];
                    }

                    // Fetching value from the request message stored in the buffer
                    for (int j = 0; j < KEY_VALUE_MAX_LENGTH; j++)
                    {
                        value[j] = buffer[KEY_VALUE_MAX_LENGTH + 1 + j];
                    }

                    string keyString(key);
                    string valueString(value);

                    // Checking whether the key already exists in the storage or not
                  /*  if (keyValueCache.count(keyString) > 0)
                    {
                        // Key exists in the storage, so just update its corresponding value
                        keyValueCache[keyString] = valueString;
                    }
                    else
                    {
                        // Key doesn't exist in the storage, so store the new key-value pair
                        keyValueCache.insert({keyString, valueString});
                    }
                    */
                    node_t *insert = new node_t;
                    insert->key = keyString;
                    insert->value = valueString;
                    insert->frequency = 1;
                    cout<<keyString<<endl;
                    cache_delete(keyString);
                    cache_insert(insert);
                    // Indicating successful insertion/updation in our response message
                    responseMessage[0] = SUCCESS_STATUS_CODE;

                    // Sending the response to the client
                    int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                }
                else if (statusCode == DEL_STATUS_CODE)
                {
                    // This means that the request is a 'DEL' request
                    cout<<"About to delete"<<endl;
                    char key[KEY_VALUE_MAX_LENGTH + 1];
                    key[KEY_VALUE_MAX_LENGTH] = '\0';

                    // Fetching key from the request message stored in the buffer
                    for (int j = 0; j < KEY_VALUE_MAX_LENGTH; j++)
                    {
                        key[j] = buffer[j + 1];
                    }

                    string keyString(key);
                    bool cache_del = cache_delete(keyString);
                    keyString = keyString.append(256-keyString.length(),'#');
                    MD5 md5;
                    bst_t *del_node = bst_find(root, md5(keyString));
                    cout<<"Successfully executed find"<<endl;
                    if(del_node!=NULL){
                      cout<<"Found in store. Deleting."<<endl;
                      root = bst_delete(root, md5(keyString));
                    }

                    // Checking whether the key exists in the storage or not
                    if (cache_del || del_node)
                    {
                        // Removing the key-value pair from the storage
                        //keyValueCache.erase(keyString);

                        // Indicates that the deletion is successful in response message
                        responseMessage[0] = SUCCESS_STATUS_CODE;

                        // Sending the response to the client
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                    }
                    else
                    {
                        // If the key doesn't exist, then send a response indicating error
                        responseMessage[0] = ERROR_STATUS_CODE;

                        // Sending the response to the client indicating that an error has occurred
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                    }
                }
                else
                {
                    // Invalid request
                    responseMessage[0] = ERROR_STATUS_CODE;

                    // Sending the response to the client indicating that an error has occurred
                    int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // Reading initial parameters from server configuration file
    cout<<"Server ready for connection"<<endl;
    FILE *serverConfigFile = fopen(SERVER_CONFIG_FILE, "r");
    char *temp = NULL;
    size_t len = 0;
    ssize_t numberOfLinesRead;

    while ((numberOfLinesRead = getline(&temp, &len, serverConfigFile)) != -1)
    {
        char *configParameter = strtok(temp, "=");
        char *parameterValue = strtok(NULL, "=");

        if (strcmp(configParameter, "PORT_NUMBER") == 0)
        {
            SERVER_LISTENING_PORT = atoi(parameterValue);
        }
        else if (strcmp(configParameter, "INITIAL_THREAD_POOL_SIZE") == 0)
        {
            INITIAL_THREAD_POOL_SIZE = atoi(parameterValue);
        }
        else if (strcmp(configParameter, "THREAD_QUEUE_SIZE") == 0)
        {
            THREAD_QUEUE_SIZE = atoi(parameterValue);
        }
        else if (strcmp(configParameter, "THREAD_POOL_GROWTH_SIZE") == 0)
        {
            THREAD_POOL_GROWTH_SIZE = atoi(parameterValue);
        }
        else if (strcmp(configParameter, "CACHE_SIZE") == 0)
        {
            cache_size = atoi(parameterValue);
        }
        else if (strcmp(configParameter, "CACHE_REPLACEMENT_POLICY") == 0)
        {
            if(strcmp(parameterValue, "LRU\n")==0){
              type = 1;
            }
            else if(strcmp("LFU\n", parameterValue)==0){
              type = 2;
            }
        }
        else if (strcmp(configParameter, "RECORDS_IN_FILE") == 0)
        {
            rec_file = atoi(parameterValue);
        }
    }

    /*
     * ------------------------------Setting up the thread pool------------------------------
     * This step involves creating the initial number of threads specified in the config file.
    */

    vector<pthread_t> threadPool(INITIAL_THREAD_POOL_SIZE);

    for (int i = 0; i < INITIAL_THREAD_POOL_SIZE; i++)
    {
        vector<ClientInfo *> temp;
        pendingRequestsQueue.push_back(temp);
        liveClientCounter.push_back(0);
        struct worker_args *workerArgs = (struct worker_args *) malloc(sizeof(struct worker_args));
        workerArgs->pendingRequestsQueueIndex = i;
        int threadCreateStatus = pthread_create(&threadPool[i], NULL, worker_thread, (void *) workerArgs);

        if (threadCreateStatus != 0)
        {
            printf("ERROR. Unable to create the thread!\n");
            exit(-1);
        }
    }

    // Setting up server socket and connection handling
    struct sockaddr_in addr;
    int sockFD;

    sockFD = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;

    addr.sin_port = htons(SERVER_LISTENING_PORT);

    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockFD, (struct sockaddr *) &addr, sizeof(addr));

    listen(sockFD, INITIAL_THREAD_POOL_SIZE*THREAD_QUEUE_SIZE);

    // Used to decide which thread will handle a particular connnection request.
    int WORKER_THREAD_TURN = 0;

    while (1)
    {
        ClientInfo *newClient = (ClientInfo *) malloc(sizeof(ClientInfo));
        newClient->addr = addr;
        memset(&newClient->client, 0, sizeof(newClient->client));
        newClient->addrlen = sizeof(newClient->client);
        newClient->connectionFD = accept(sockFD, (struct sockaddr *) &newClient->client, &newClient->addrlen);

        pendingRequestsQueue[WORKER_THREAD_TURN].push_back(newClient);
        liveClientCounter[WORKER_THREAD_TURN] += 1;
        /* Incrementing it since we have to assign connection requests to threads in round-robin
        *  manner.
        */

        WORKER_THREAD_TURN = (WORKER_THREAD_TURN + 1) % pendingRequestsQueue.size();

        // Checking whether the queue of all threads has become full or not
        int threadPoolFull = 1;

        for (int i = 0; i < liveClientCounter.size(); i++)
        {
            if (liveClientCounter[i] < THREAD_QUEUE_SIZE)
            {
                threadPoolFull = 0;
                break;
            }
        }

        // If thread pool has reached its maximum capacity, we add more threads into the pool
        if (threadPoolFull)
        {
            int oldSize = threadPool.size();
            threadPool.resize(oldSize + THREAD_POOL_GROWTH_SIZE);

            for (int i = 0; i < THREAD_POOL_GROWTH_SIZE; i++)
            {
                int positionIndex = oldSize + i;
                vector<ClientInfo *> temp;
                pendingRequestsQueue.push_back(temp);
                liveClientCounter.push_back(0);
                int threadCreateStatus = pthread_create(&threadPool[positionIndex], NULL, worker_thread, (void *) &positionIndex);

                if (threadCreateStatus != 0)
                {
                    printf("ERROR. Unable to create the thread!\n");
                    exit(-1);
                }
            }
        }
    }

    return 0;
}
