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
#include <unordered_map>
#include <string>
#include "storageManager.hpp"

using namespace std;

// States codes for GET, PUT and DEL requests as well as for SUCCESS and ERROR responses
enum STATUS_CODES {GET_STATUS_CODE = 1, PUT_STATUS_CODE = 2, DEL_STATUS_CODE = 3, LRU_CACHE = 4, LFU_CACHE = 5, SUCCESS_STATUS_CODE = 200, ERROR_STATUS_CODE = 240};

// Defining the name of server configuration file
#define SERVER_CONFIG_FILE "server.config"

// Message length (in bytes)
#define MESSAGE_LENGTH 513

// Key/Value maximum length (in bytes)
#define KEY_VALUE_MAX_LENGTH 256

// Key-Value Cache
unordered_map<string, KeyValueEntry> keyValueCache;

// Maintains the cache blocks in least recently used 
vector<KeyValueEntry> leastRecentlyUsedTracker;

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

int NUMBER_OF_ENTRIES_IN_CACHE = -1;   // Number of entries that can be present in the cache at a time

int CACHE_REPLACEMENT_POLICY = -1;     // Indicates which cache replacement policy to use while replacing an entry in the cache

// This vector holds the pending accepted connections to be handled by each worker thread.
vector<vector<ClientInfo *> > pendingRequestsQueue;

// This vector holds the number of clients each thread is handling
vector<int> liveClientCounter;

// Reader-Writer Lock to protect LRU entries tracker (leastRecentlyUsedTracker vector) from concurrent execution effects
pthread_rwlock_t lockLRUTracker;

// Defining mutexes for files
vector<pthread_mutex_t> fileLock(256);

// Server ready indicator
int isServerReady = 0;

// Each worker thread will execute this function
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
        if (pendingRequestsQueue[positionInQueueVector].empty() == 0)
        {
            while (pendingRequestsQueue[positionInQueueVector].empty() == 0)
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
        }
        int nfds = epoll_wait(ePollFD, events, THREAD_QUEUE_SIZE, NULL);

        //cout << "Alright " << ePollFD << endl;

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
                    char key[KEY_VALUE_MAX_LENGTH + 1];
                    key[KEY_VALUE_MAX_LENGTH] = '\0';

                    // Fetching key from the request message stored in the buffer
                    for (int j = 0; j < KEY_VALUE_MAX_LENGTH; j++)
                    {
                        key[j] = buffer[j + 1];
                    }
                    
                    string keyString(key);
                    
                    // Acquiring the reader lock
                    pthread_rwlock_rdlock(&lockLRUTracker);
                    
                    // Checking whether the key exists in the cache or not
                    if (keyValueCache.count(keyString) > 0)
                    {
                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        // This means that the key exists in the cache, so we send the corresponding value
                        responseMessage[0] = SUCCESS_STATUS_CODE;      

                        // Acquiring the reader lock
                        pthread_rwlock_rdlock(&lockLRUTracker);
                        // Fetching the entry corresponding to the key from the cache
                        KeyValueEntry keyValueEntry = keyValueCache[keyString];
                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);
                       
                        for (int j = 0; j < strlen(keyValueEntry.value); j++)
                        {
                            responseMessage[KEY_VALUE_MAX_LENGTH + 1 + j] = keyValueEntry.value[j];
                        }

                        // Adjusting the least recently used vector
                        int j;

                        // Acquiring the writer lock
                        pthread_rwlock_wrlock(&lockLRUTracker);

                        // Finding the key-value entry and incrementing its reference count
                        for (int k = 0; k < leastRecentlyUsedTracker.size(); k++)
                        {
                            string tempString(leastRecentlyUsedTracker[k].key);
                            if (tempString == keyString)
                            {
                                j = k;
                                leastRecentlyUsedTracker[k].frequency += 1;
                                break;
                            }
                        }

                        // Based on the reference count, adjusting it's position in the vector
                        for (; j < leastRecentlyUsedTracker.size() - 1; j++)
                        {
                            KeyValueEntry temp = leastRecentlyUsedTracker[j + 1];
                            leastRecentlyUsedTracker[j + 1] = leastRecentlyUsedTracker[j];
                            leastRecentlyUsedTracker[j] = temp;
                        }

                        // Releasing the writer lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        // Sending the response to the client
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                    }
                    else
                    {
                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        // If the key doesn't exist in the cache, then we search for it in the persistent storage
                        int fileNameInt = keyString.at(0);

                        // Acquiring the file lock
                        pthread_mutex_lock(&fileLock[fileNameInt]);
                        
                        // Searching for the key-value entry on the persistent storage
                        KeyValueEntry *entryFromStorage = search_on_storage(keyString);
                        
                        // Releasing the file lock
                        pthread_mutex_unlock(&fileLock[fileNameInt]);

                        if (entryFromStorage == NULL)
                        {
                            // The key doesn't exist on the persistent storage as well, so we send an error message
                            responseMessage[0] = ERROR_STATUS_CODE;
                    
                            // Sending the response to the client indicating that an error has occurred
                            int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                        }
                        else
                        {
                            responseMessage[0] = SUCCESS_STATUS_CODE;
                            // The key exists in the persistent storage, we now insert it into the cache
                            if (leastRecentlyUsedTracker.size() == NUMBER_OF_ENTRIES_IN_CACHE)
                            {
                                // Need to replace a key-value entry in the cache
                                if (CACHE_REPLACEMENT_POLICY == LFU_CACHE)
                                {
                                    // Follow the LFU Replacement Policy (if required)
                                    int minFreqIndex = 0;

                                    // Acquiring the reader lock
                                    pthread_rwlock_rdlock(&lockLRUTracker);
                                    
                                    // Checking for the key-value entry which is referenced least number of times
                                    for (int j = 1; j < leastRecentlyUsedTracker.size(); j++)
                                    {
                                        if (leastRecentlyUsedTracker[j].frequency < leastRecentlyUsedTracker[minFreqIndex].frequency)
                                        {
                                            minFreqIndex = j;
                                        }
                                    }
                                    
                                    string minFreqStr(leastRecentlyUsedTracker[minFreqIndex].key);
                                    
                                    // Releasing the reader lock
                                    pthread_rwlock_unlock(&lockLRUTracker);

                                    // Acquiring the writer lock
                                    pthread_rwlock_wrlock(&lockLRUTracker);

                                    // Removing the entry from the cache
                                    keyValueCache.erase(minFreqStr);
                                    leastRecentlyUsedTracker.erase(leastRecentlyUsedTracker.begin() + minFreqIndex);

                                    // Releasing the writer lock
                                    pthread_rwlock_unlock(&lockLRUTracker);

                                    // Inserting new entry into the cache and vector
                                    KeyValueEntry newEntry;
                                    newEntry.frequency = 1;
                                    newEntry.isValid = 1;
                                    
                                    for (int j = 0; j < 257; j++)
                                    {
                                        newEntry.key[j] = '\0';
                                        newEntry.value[j] = '\0';
                                    }
                                    for (int j = 0; j < 257; j++)
                                    {
                                        newEntry.key[j] = entryFromStorage->key[j];
                                    }
                                    for (int j = 0; j < 257; j++)
                                    {
                                        newEntry.value[j] = entryFromStorage->value[j];
                                    }

                                    delete entryFromStorage;

                                    string newEntryStr(newEntry.key);

                                    // Acquiring the writer lock
                                    pthread_rwlock_wrlock(&lockLRUTracker);

                                    // Adding the key-value entry to the cache
                                    keyValueCache.insert({newEntryStr, newEntry});
                                    leastRecentlyUsedTracker.push_back(newEntry);

                                    // Releasing the writer lock
                                    pthread_rwlock_unlock(&lockLRUTracker);

                                    // Inserting the value corresponding to the key in the response message
                                    for (int j = 0; j < strlen(newEntry.value); j++)
                                    {
                                        responseMessage[KEY_VALUE_MAX_LENGTH + 1 + j] = newEntry.value[j];
                                    }

                                    // Sending the response to the client
                                    int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                                }
                                else 
                                {
                                    // Follow the LRU Replacement Policy

                                    // Acquiring the reader lock
                                    pthread_rwlock_rdlock(&lockLRUTracker);

                                    // The first entry is the least recently used
                                    KeyValueEntry entryToBeRemoved = leastRecentlyUsedTracker[0];

                                    // Releasing the reader lock
                                    pthread_rwlock_unlock(&lockLRUTracker);

                                    string entryToBeRemovedStr(entryToBeRemoved.key);

                                    // Acquiring the writer lock
                                    pthread_rwlock_wrlock(&lockLRUTracker);

                                    // Removing the key-value entry from the cache
                                    keyValueCache.erase(entryToBeRemovedStr);
                                    leastRecentlyUsedTracker.erase(leastRecentlyUsedTracker.begin());

                                    // Releasing the writer lock
                                    pthread_rwlock_unlock(&lockLRUTracker);

                                    // Inserting new entry into the cache and vector
                                    KeyValueEntry newEntry;
                                    newEntry.frequency = 1;
                                    newEntry.isValid = 1;
                                    
                                    for (int j = 0; j < 257; j++)
                                    {
                                        newEntry.key[j] = '\0';
                                        newEntry.value[j] = '\0';
                                    }
                                    for (int j = 0; j < 257; j++)
                                    {
                                        newEntry.key[j] = entryFromStorage->key[j];
                                    }
                                    for (int j = 0; j < 257; j++)
                                    {
                                        newEntry.value[j] = entryFromStorage->value[j];
                                    }

                                    delete entryFromStorage;

                                    string newEntryStr(newEntry.key);

                                    // Acquiring the writer lock
                                    pthread_rwlock_wrlock(&lockLRUTracker);

                                    // Inserting the key-value entry into the cache
                                    keyValueCache.insert({newEntryStr, newEntry});
                                    leastRecentlyUsedTracker.push_back(newEntry);

                                    // Releasing the writer lock
                                    pthread_rwlock_unlock(&lockLRUTracker);

                                    // Inserting the value corresponding to the key in the response message
                                    for (int j = 0; j < strlen(newEntry.value); j++)
                                    {
                                        responseMessage[KEY_VALUE_MAX_LENGTH + 1 + j] = newEntry.value[j];
                                    }

                                    // Sending the response to the client
                                    int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                                }
                            }
                            else
                            {
                                // No need to replace a key-value entry in the cache
                                // Inserting new entry into the cache and vector
                                KeyValueEntry newEntry;
                                newEntry.frequency = 1;
                                newEntry.isValid = 1;
                                    
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.key[j] = '\0';
                                    newEntry.value[j] = '\0';
                                }
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.key[j] = entryFromStorage->key[j];
                                }
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.value[j] = entryFromStorage->value[j];
                                }

                                delete entryFromStorage;

                                string newEntryStr(newEntry.key);
                                 // Acquiring the writer lock
                                pthread_rwlock_wrlock(&lockLRUTracker);

                                // Inserting the key-value entry into the cache
                                keyValueCache.insert({newEntryStr, newEntry});
                                leastRecentlyUsedTracker.push_back(newEntry);

                                // Releasing the writer lock
                                pthread_rwlock_unlock(&lockLRUTracker);

                                // Inserting the value corresponding to the key in the response message
                                for (int j = 0; j < strlen(newEntry.value); j++)
                                {
                                    responseMessage[KEY_VALUE_MAX_LENGTH + 1 + j] = newEntry.value[j];
                                }

                                // Sending the response to the client
                                int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                            }
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
                    int fileNameInt = keyString.at(0);

                    //Acquiring the reader lock
                    pthread_rwlock_rdlock(&lockLRUTracker);

                    // Checking whether the key already exists in the storage or not
                    if (keyValueCache.count(keyString) > 0)
                    {
                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        // Acquiring the writer lock
                        pthread_rwlock_wrlock(&lockLRUTracker);

                        // Key exists in the cache, so update its corresponding value
                        for (int j = 0; j < 257; j++)
                        {
                            keyValueCache[keyString].value[j] = value[j];
                        }

                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        // Updating the value in the persistent storage as well
                        KeyValueEntry *newEntry = new KeyValueEntry;
                        newEntry->frequency = 0;
                        newEntry->isValid = 1;
                        
                        for (int j = 0; j < 257; j++)
                        {
                            newEntry->key[j] = key[j];
                        }
                        for (int j = 0; j < 257; j++)
                        {
                            newEntry->value[j] = value[j];
                        }

                        // Acquiring the file lock
                        pthread_mutex_lock(&fileLock[fileNameInt]);

                        // Updating the key-value entry on the persistent storage
                        insert_on_storage(newEntry);

                        // Releasing the file lock
                        pthread_mutex_unlock(&fileLock[fileNameInt]);

                        // Acquiring the writer lock
                        pthread_rwlock_wrlock(&lockLRUTracker);

                        // Incrementing the key-value entry reference count
                        for (int k = 0; k < leastRecentlyUsedTracker.size(); k++)
                        {
                            string tempString(leastRecentlyUsedTracker[k].key);
                            if (tempString == keyString)
                            {
                                leastRecentlyUsedTracker[k].frequency += 1;
                                break;
                            }
                        }

                        // Releasing the writer lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        delete newEntry;
                   }
                    else
                    {
                        // Key doesn't exist in the cache, so we insert/update the key in persistent storage first

                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        KeyValueEntry *newEntryToBeInserted = new KeyValueEntry;
                        newEntryToBeInserted->frequency = 0;
                        newEntryToBeInserted->isValid = 1;
                            
                        for (int j = 0; j < 257; j++)
                        {
                            newEntryToBeInserted->key[j] = key[j];
                        }
                        for (int j = 0; j < 257; j++)
                        {
                            newEntryToBeInserted->value[j] = value[j];
                        }

                        // Acquiring the file lock
                        pthread_mutex_lock(&fileLock[fileNameInt]);

                        // Inserting the key-value entry into the persistent storage
                        insert_on_storage(newEntryToBeInserted);

                        // Releasing the file lock
                        pthread_mutex_unlock(&fileLock[fileNameInt]);

                        delete newEntryToBeInserted;

                        if (leastRecentlyUsedTracker.size() == NUMBER_OF_ENTRIES_IN_CACHE)
                        {
                            // Need to replace a key-value entry in the cache
                            if (CACHE_REPLACEMENT_POLICY == LFU_CACHE)
                            {
                                // Follow the LFU Replacement Policy
                                int minFreqIndex = 0;

                                // Acquiring the reader lock
                                pthread_rwlock_rdlock(&lockLRUTracker);

                                // Checking the key-value entry which has been referenced least number of times
                                for (int j = 1; j < leastRecentlyUsedTracker.size(); j++)
                                {
                                    if (leastRecentlyUsedTracker[j].frequency < leastRecentlyUsedTracker[minFreqIndex].frequency)
                                    {
                                        minFreqIndex = j;
                                    }
                                }
                                
                                // Releasing the reader lock
                                pthread_rwlock_unlock(&lockLRUTracker);

                                string minFreqStr(leastRecentlyUsedTracker[minFreqIndex].key);

                                // Acquiring the writer lock
                                pthread_rwlock_wrlock(&lockLRUTracker);

                                // Removing the least referenced entry from the cache
                                keyValueCache.erase(minFreqStr);
                                leastRecentlyUsedTracker.erase(leastRecentlyUsedTracker.begin() + minFreqIndex);

                                // Releasing the writer lock
                                pthread_rwlock_unlock(&lockLRUTracker);

                                // Inserting new entry into the cache and vector
                                KeyValueEntry newEntry;
                                newEntry.frequency = 1;
                                newEntry.isValid = 1;
                                    
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.key[j] = '\0';
                                    newEntry.value[j] = '\0';
                                }
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.key[j] = key[j];
                                }
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.value[j] = value[j];
                                }

                                string newEntryStr(newEntry.key);
                                
                                // Acquiring the writer lock
                                pthread_rwlock_wrlock(&lockLRUTracker);

                                // Inserting the key-value pair into the cache
                                keyValueCache.insert({newEntryStr, newEntry});
                                leastRecentlyUsedTracker.push_back(newEntry);

                                // Releasing the writer lock
                                pthread_rwlock_unlock(&lockLRUTracker);
                            }
                            else 
                            {
                                // Follow the LRU Replacement Policy

                                // Acquiring the writer lock
                                pthread_rwlock_wrlock(&lockLRUTracker);

                                // The first entry is the least recently used
                                KeyValueEntry entryToBeRemoved = leastRecentlyUsedTracker[0];

                                string entryToBeRemovedStr(entryToBeRemoved.key);

                                // Removing from the cache
                                keyValueCache.erase(entryToBeRemovedStr);

                                // Removing from the vector
                                leastRecentlyUsedTracker.erase(leastRecentlyUsedTracker.begin());

                                // Releasing the writer lock
                                pthread_rwlock_unlock(&lockLRUTracker);

                                // Inserting new entry into the cache and vector
                                KeyValueEntry newEntry;
                                newEntry.frequency = 1;
                                newEntry.isValid = 1;
                                    
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.key[j] = '\0';
                                    newEntry.value[j] = '\0';
                                }
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.key[j] = key[j];
                                }
                                for (int j = 0; j < 257; j++)
                                {
                                    newEntry.value[j] = value[j];
                                }

                                string newEntryStr(newEntry.key);

                                // Acquiring the writer lock
                                pthread_rwlock_wrlock(&lockLRUTracker);

                                // Inserting the key-value entry into the cache
                                keyValueCache.insert({newEntryStr, newEntry});
                                leastRecentlyUsedTracker.push_back(newEntry);

                                // Releasing the writer lock
                                pthread_rwlock_unlock(&lockLRUTracker);
                            }
                        }
                        else
                        {
                            // No need to replace a key-value entry in the cache
                            // Inserting new entry into the cache and vector
                            KeyValueEntry newEntry;
                            newEntry.frequency = 1;
                            newEntry.isValid = 1;
                                    
                            for (int j = 0; j < 257; j++)
                            {
                                newEntry.key[j] = '\0';
                                newEntry.value[j] = '\0';
                            }
                            for (int j = 0; j < 257; j++)
                            {
                                newEntry.key[j] = key[j];
                            }
                            for (int j = 0; j < 257; j++)
                            {
                                newEntry.value[j] = value[j];
                            }

                            string newEntryStr(newEntry.key);

                            // Acquiring the writer lock
                            pthread_rwlock_wrlock(&lockLRUTracker);
                            
                            keyValueCache.insert({newEntryStr, newEntry});
                            leastRecentlyUsedTracker.push_back(newEntry);
                        
                            // Releasing the writer lock
                            pthread_rwlock_unlock(&lockLRUTracker);
                        }
                    }
                    
                    // Indicating successful insertion/updation in our response message
                    responseMessage[0] = SUCCESS_STATUS_CODE;

                    // Sending the response to the client
                    int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                }
                else if (statusCode == DEL_STATUS_CODE)
                {
                    // This means that the request is a 'DEL' request

                    char key[KEY_VALUE_MAX_LENGTH + 1];
                    key[KEY_VALUE_MAX_LENGTH] = '\0';
                    

                    // Fetching key from the request message stored in the buffer
                    for (int j = 0; j < KEY_VALUE_MAX_LENGTH; j++)
                    {
                        key[j] = buffer[j + 1];
                    }
                    
                    string keyString(key);
                    int fileNameInt = keyString.at(0);
                   
                    // Acquiring the file lock
                    pthread_mutex_lock(&fileLock[fileNameInt]);

                    // Deleting the key-value entry from the persistent storage (if it exists)
                    int deletionStatus = delete_from_storage(keyString);

                    // Releasing the file lock
                    pthread_mutex_unlock(&fileLock[fileNameInt]);

                    // Acquiring the reader lock
                    pthread_rwlock_rdlock(&lockLRUTracker);

                    // Checking whether the key exists in the cache or not
                    if (keyValueCache.count(keyString) > 0)
                    {
                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        int delIndex;

                        // Acquiring the writer lock
                        pthread_rwlock_wrlock(&lockLRUTracker);

                        // Removing the key-value pair from the vector
                        for (int k = 0; k < leastRecentlyUsedTracker.size(); k++)
                        {
                            string tempString(leastRecentlyUsedTracker[k].key);
                            if (tempString == keyString)
                            {
                                delIndex = k;
                                break;
                            }
                        }
                        leastRecentlyUsedTracker.erase(leastRecentlyUsedTracker.begin() + delIndex);

                       // Removing the key-value pair from the cache
                        keyValueCache.erase(keyString);

                        // Releasing the writer lock
                        pthread_rwlock_unlock(&lockLRUTracker);

                        // Indicates that the deletion is successful in response message
                        responseMessage[0] = SUCCESS_STATUS_CODE;      

                        // Sending the response to the client
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                    }
                    else if (deletionStatus == 0)
                    {
                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);
                        // If the key doesn't exist, then send a response indicating error
                        responseMessage[0] = ERROR_STATUS_CODE;

                        // Sending the response to the client indicating that an error has occurred
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                    }
                    else
                    {
                        // Releasing the reader lock
                        pthread_rwlock_unlock(&lockLRUTracker);
                        responseMessage[0] = ERROR_STATUS_CODE;

                        // Sending the response to the client indicating that an error has occurred
                        int m = write(events[i].data.fd, responseMessage, MESSAGE_LENGTH);
                    }
                    
                    //pthread_mutex_unlock(&lockLRUTracker);
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

/* Used for performance analysis

struct timespec start,finish;
double elapsed;

void print_throughput(int sig)
{
    clock_gettime(CLOCK_MONOTONIC, &finish);

    // Calculating the total time and throughput
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec)/1000000000.0;
    double throughput = (double) (120000/elapsed);
    printf("\n\nTotal time to process 60000 requests is %f and throughput is %f\n", elapsed, throughput);
    
    exit(1);
}
*/
int main(int argc, char *argv[])
{
    //signal(SIGINT, print_throughput);
    // Initializing the reader-writer lock for LRU entries tracker (leastRecentlyUsedTracker vector)
    pthread_rwlock_init(&lockLRUTracker, NULL);

    // Initializing the file locks
    for (int i = 0; i < 256; i++)
    {
        pthread_mutex_init(&fileLock[i], NULL);
    }

    // Reading initial parameters from server configuration file
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
        else if (strcmp(configParameter, "NUMBER_OF_ENTRIES_IN_CACHE") == 0)
        {
            NUMBER_OF_ENTRIES_IN_CACHE = atoi(parameterValue);
            cout << "Number of entries: " <<  NUMBER_OF_ENTRIES_IN_CACHE << endl;
        }    
        else if (strcmp(configParameter, "CACHE_REPLACEMENT_POLICY") == 0)
        {
            if (strcmp(parameterValue, "LFU\n") == 0)
            {
                CACHE_REPLACEMENT_POLICY = LFU_CACHE;
                cout << "LFU " << endl;
            }
            else
            {
                CACHE_REPLACEMENT_POLICY = LRU_CACHE;
                cout << "LRU " << endl;
            }
        }
    } 

    // Setting up server socket and connection handling
    struct sockaddr_in addr;
    int sockFD;
    int y = 1;

    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockFD < 0)
    {
        printf("\nError opening socket!\n");
        exit(1);
    }

    if (setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int)) == -1)
    {
        printf("\nError: Setsockopt error. Please restart the server!\n");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    
    addr.sin_family = AF_INET;

    addr.sin_port = htons(SERVER_LISTENING_PORT);
    
    addr.sin_addr.s_addr = INADDR_ANY;

    /* 
     * ------------------------------Setting up the thread pool------------------------------
     * This step involves creating the initial number of threads specified in the config file.
    */

    vector<pthread_t> threadPool(INITIAL_THREAD_POOL_SIZE);
    
    for (int i = 0; i < INITIAL_THREAD_POOL_SIZE; i++)
    {
        vector<ClientInfo *> temp;
        pendingRequestsQueue.push_back(temp);
        pendingRequestsQueue[pendingRequestsQueue.size() - 1].clear();
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

    bind(sockFD, (struct sockaddr *) &addr, sizeof(addr));

    listen(sockFD, INITIAL_THREAD_POOL_SIZE*THREAD_QUEUE_SIZE);

    // Used to decide which thread will handle a particular connnection request.
    int WORKER_THREAD_TURN = 0;
    
    //int firstTime = 0;

    while (1)
    {
        ClientInfo *newClient = (ClientInfo *) malloc(sizeof(ClientInfo));
        newClient->addr = addr;
        memset(&newClient->client, 0, sizeof(newClient->client));
        newClient->addrlen = sizeof(newClient->client);
        newClient->connectionFD = accept(sockFD, (struct sockaddr *) &newClient->client, &newClient->addrlen);  

        
        if (newClient->connectionFD < 0)
        {
            printf("\nServer accept error. Please restart the server!\n");
            exit(1);
        }

        pendingRequestsQueue[WORKER_THREAD_TURN].push_back(newClient);
        liveClientCounter[WORKER_THREAD_TURN] += 1;

        /*
        if (!firstTime)
        {
            clock_gettime(CLOCK_MONOTONIC, &start);
            firstTime = 1;
        }
        */
        /* Incrementing it since we have to assign connection requests to threads in round-robin
         * manner. */ 
        
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