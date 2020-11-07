// This contains the implementation of all functions declared in KVClientLibrary
#include "KVClientLibrary.hpp"

char *GET(char *key)
{
    char requestMessage[MESSAGE_LENGTH];
    char responseMessage[MESSAGE_LENGTH];

    // This is done to make the padding process easy
    for (int i = 0; i < MESSAGE_LENGTH; i++)
    {
        requestMessage[i] = '\0';
    }

    // The first byte of the request message will be status code
    requestMessage[0] = GET_STATUS_CODE;

    // This will point to the location where the key value will be inserted in our request message
    char *keyInRequestMessage = &requestMessage[1];

    // Copying the key value to the request message
    strcpy(keyInRequestMessage, key);

    // Sending the GET request to the server
    int n = write(sockFD, requestMessage, MESSAGE_LENGTH);

    // Receiving the response from the server
    int m = read(sockFD, responseMessage, MESSAGE_LENGTH);

    // Decoding the response by first checking the status code
    int statusCode = responseMessage[0] & 255;

    // If the Key is not available in the storage, then response includes the error status code (240)
    if (statusCode == ERROR_STATUS_CODE)
    {
        char errorMessage[256] = "GET Request Error: Key not availabe on storage!";

        char *requestResult = (char *) malloc(sizeof(errorMessage));

        strcpy(requestResult, errorMessage);

        return requestResult; 
    }
    /* If the Key is available in the storage, then response message include the success status code
     * (200) as well as the corresponding value present in the storage */ 
    else if (statusCode == SUCCESS_STATUS_CODE)
    {
        char receivedValue[KEY_VALUE_MAX_LENGTH + 1];
        char *valueInResponseMessage = &responseMessage[KEY_VALUE_MAX_LENGTH + 1];
        for (int i = 0; i < KEY_VALUE_MAX_LENGTH + 1; i++)
        {
            receivedValue[i] = '\0';
        }

        strcpy(receivedValue, valueInResponseMessage);

        char *requestResult = (char *) malloc(sizeof(receivedValue));
        strcpy(requestResult, receivedValue);

        return requestResult;
    }
    else
    {
        return NULL;
    }
    
    return NULL;
}

void PUT(char *key, char *value)
{
    // TODO
}

void DEL(char *key)
{
    // TODO
}