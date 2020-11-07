#include "KVClientLibrary.hpp"

// Name of the server configuration file
#define SERVER_CONFIG_FILE "server.config"

// Port number on which server is listening
int SERVER_LISTENING_PORT = -1; 

// Socket-related variables
struct sockaddr_in addr = {0};
int sockFD;

int main()
{
    // Reading the port number from the server configuration file
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
            break;
        } 
    }

    char message[256];

    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_LISTENING_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    connect(sockFD, (struct sockaddr *) &addr, sizeof(addr));

    while (1)
    {
        int sec = (random() % 10) + 1;
        sleep(sec);
        sprintf(message, "Hello Server, I'm client!\n");
        write(sockFD, message, strlen(message));
    }
}