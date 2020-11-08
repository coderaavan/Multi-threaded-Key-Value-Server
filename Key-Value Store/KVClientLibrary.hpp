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

// States codes for GET, PUT and DEL requests as well as for SUCCESS and ERROR responses
enum STATUS_CODES {GET_STATUS_CODE = 1, PUT_STATUS_CODE = 2, DEL_STATUS_CODE = 3, SUCCESS_STATUS_CODE = 200, ERROR_STATUS_CODE = 240};

// Message length (in bytes)
#define MESSAGE_LENGTH 513

// Key/Value maximum length (in bytes)
#define KEY_VALUE_MAX_LENGTH 256

// Will be used to send requests to the server and receive responses from the server
extern struct sockaddr_in addr;
extern int sockFD;

// This function retrieves the corresponding value for a given key (if it exists)
char *GET(char *key);

// This function stores the key, value pair on the server storage (and replaces the value of a key if one exists)
void PUT(char *key, char *value);

// This function is used to remove a key, value entry from the server storage
void DEL(char *key);
