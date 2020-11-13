#include "KVClientLibrary.hpp"

// Name of the server configuration file
#define SERVER_CONFIG_FILE "server.config"

// Port number on which server is listening
int SERVER_LISTENING_PORT = -1; 

// Socket-related variables
struct sockaddr_in addr = {0};
int sockFD;

int main(int argc, char *argv[])
{
    //struct timespec start,finish;
    //double elapsed;

    // Keeps track of the number of total requests processed processed through the file (if any)
    int NUMBER_OF_REQUESTS_PROCESSED = 0;

    // Reading the port number from the server configuration file
    FILE *serverConfigFile = fopen(SERVER_CONFIG_FILE, "r");
    char *temp = NULL;
    size_t len = 0;
    ssize_t numberOfLinesRead;

    //clock_gettime(CLOCK_MONOTONIC, &start);

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

    // Setting up socket structures and connecting to the server

    sockFD = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_LISTENING_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    connect(sockFD, (struct sockaddr *) &addr, sizeof(addr));

    // Opening the testcase file
    FILE *testCaseFile = NULL;
    char *temp_test = NULL;
    size_t len_test = 0;
    ssize_t numberOfLinesRead_test;

    if (argc > 1)
    {
        testCaseFile = fopen(argv[1], "r");
    }

    while (1)
    {
        char requestType[4];
        char keyInput[300];
        char valueInput[300];
        // Asking the client what type of operation it wants to perform
        if (testCaseFile != NULL)
        {
            numberOfLinesRead = getline(&temp_test, &len_test, testCaseFile);

            if (numberOfLinesRead == -1)
            {
                printf("All testcases are over!\n");
                break;
            }
            else
            {
                NUMBER_OF_REQUESTS_PROCESSED += 1;

                char *requestTypeFromFile = strtok(temp_test, "[ \n]");

                for (int i = 0; i < 3; i++)
                {
                    requestType[i] = requestTypeFromFile[i];
                }

                requestType[3] = '\0';
            }
        }
        else
        {
            printf("Enter request type: ");
            scanf("%s", requestType);
        }

        if (strlen(requestType) == 3)
        {
            /* Checking whether the user has made one of the valid requests (i.e. GET, PUT or DEL)
             * and handling each of these requests */
            if (strcmp(requestType, "GET") == 0)
            {
                if (testCaseFile != NULL)
                {
                    char *keyFromFile = strtok(NULL, "[ \n]");

                    for (int i = 0; i < strlen(keyFromFile); i++)
                    {
                        keyInput[i] = keyFromFile[i];
                    }

                    keyInput[strlen(keyFromFile)] = '\0';

                //    printf("Performing GET for Key: %s\n", keyInput);
                }
                else
                {
                    printf("Enter the key: ");
                    scanf("%s", keyInput);
                }
                
                // If the user enters a key longer than 256 characters
                if (strlen(keyInput) > KEY_VALUE_MAX_LENGTH)
                {
                    // Displaying the appropriate error message
                    printf("Key should be less than or equal to 256 characters.\n");
                }
                else
                {
                    // Using the GET function of KVClientLibrary to request the corresponding value
                    char *serverResponse = GET(keyInput);
                    printf("\n%s\n", serverResponse);
                    free(serverResponse);
                }
            }   
            else if (strcmp(requestType, "PUT") == 0)
            {
                if (testCaseFile != NULL)
                {
                    char *keyFromFile = strtok(NULL, "[ \n]");

                    for (int i = 0; i < strlen(keyFromFile); i++)
                    {
                        keyInput[i] = keyFromFile[i];
                    }

                    keyInput[strlen(keyFromFile)] = '\0';

                //   printf("Performing PUT for Key: %s\n", keyInput);
                }
                else
                {
                    printf("Enter the key: ");
                    scanf("%s", keyInput);
                }
                // If the user enters a key longer than 256 characters
                if (strlen(keyInput) > KEY_VALUE_MAX_LENGTH)
                {
                    // Displaying the appropriate error message
                    printf("Key should be less than or equal to 256 characters.\n");
                }
                else
                {
                    if (testCaseFile != NULL)
                    {
                        char *valueFromFile = strtok(NULL, "[ \n]");

                        for (int i = 0; i < strlen(valueFromFile); i++)
                        {
                            valueInput[i] = valueFromFile[i];
                        }

                        valueInput[strlen(valueFromFile)] = '\0';
                    }
                    else
                    {
                        printf("Enter the value: ");
                        scanf("%s", valueInput);
                    }

                    // If the user enters a value longer than 256 characters
                    if (strlen(valueInput) > KEY_VALUE_MAX_LENGTH)
                    {
                        // Displaying the appropriate error message
                        printf("Value should be less than or equal to 256 characters.\n");
                    }
                    else
                    {
                        // Using the PUT function of KVClientLibrary to insert/update the key-value pair
                         PUT(keyInput, valueInput);
                    }
                }
            }         
            else if (strcmp(requestType, "DEL") == 0)
            {
                if (testCaseFile != NULL)
                {
                    char *keyFromFile = strtok(NULL, "[ \n]");

                    for (int i = 0; i < strlen(keyFromFile); i++)
                    {
                        keyInput[i] = keyFromFile[i];
                    }

                    keyInput[strlen(keyFromFile)] = '\0';

                //    printf("Performing DEL for Key: %s\n", keyInput);
                }
                else
                {
                    printf("Enter the key: ");
                    scanf("%s", keyInput);
                }

                // If the user enters a key longer than 256 characters
                if (strlen(keyInput) > KEY_VALUE_MAX_LENGTH)
                {
                    // Displaying the appropriate error message
                    printf("Key should be less than or equal to 256 characters.\n");
                }
                else
                {
                    // Using the DEL function of KVClientLibrary to remove the key-value pair
                    DEL(keyInput);
                }
            }
            else
            {
                // Invalid request type
                printf("Invalid request type!\n");
            }
        }
        else
        {
            // Invalid request type
            printf("Invalid request type!\n");
        }
    }

    /*
    clock_gettime(CLOCK_MONOTONIC, &finish);
    
    // Calculating the total time and throughput
    elapsed = (finish.tv_sec - start.tv_sec);
    elapsed += (finish.tv_nsec - start.tv_nsec)/1000000000.0;
    double throughput = (double) (60000/elapsed);
    printf("\n\nTotal time to process %d requests is %f and throughput is %f\n", NUMBER_OF_REQUESTS_PROCESSED, elapsed, throughput);
    */

    // If the requests were processed through a file, close that file
    if (argc > 1)
    {
        fclose(testCaseFile);    
    }

    // Closing the socket
    close(sockFD);
}
