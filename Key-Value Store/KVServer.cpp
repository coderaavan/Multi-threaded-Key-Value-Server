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

using namespace std;

// Defining the name of server configuration file
#define SERVER_CONFIG_FILE "server.config"

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
    char buffer[256]; // For testing
    // Setting up epoll context
    struct epoll_event events[THREAD_QUEUE_SIZE];

    int ePollFD = epoll_create(THREAD_QUEUE_SIZE);

    // Continuously adding new connections to the context (if any) and monitoring the epoll context
    while (1)
    {
        while (!pendingRequestsQueue[positionInQueueVector].empty())
        {
            static struct epoll_event ev;
            printf("Thead %d adding one more...\n", positionInQueueVector);
            // Reading then connection info from the queue
            ClientInfo *tempInfo = pendingRequestsQueue[positionInQueueVector].front();
            ev.data.fd = tempInfo->connectionFD;
            ev.events = EPOLLIN;
            epoll_ctl(ePollFD, EPOLL_CTL_ADD, ev.data.fd, &ev);

            // Removing the entry from pending requests queue
            pendingRequestsQueue[positionInQueueVector].erase(pendingRequestsQueue[positionInQueueVector].begin());
        }

        // Deleting all the entries from pending queue
        pendingRequestsQueue[positionInQueueVector].clear();

        int nfds = epoll_wait(ePollFD, events, THREAD_QUEUE_SIZE, NULL);

        for (int i = 0; i < nfds; i++)
        {
            memset(buffer, 0, 256);
            int n = read(events[i].data.fd, buffer, 256);
            
            if (n == 0)
            {
                // The client has disconnected so we officially close the connection
                close(events[i].data.fd);
                continue;
            }   

            printf("[Thread %d]  [Client %d]\n", positionInQueueVector, events[i].data.fd);
            puts(buffer);
        }
    }
}

int main(int argc, char *argv[])
{
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