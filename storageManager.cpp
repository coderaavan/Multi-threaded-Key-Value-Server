#include "storageManager.hpp"

using namespace std;

// To search the key-value entry in the persistent storage
KeyValueEntry *search_on_storage(string key)
{
    char firstCharacter = key.at(0);
    char keyCharArray[257];

    bzero(keyCharArray, 257);
    for (int i = 0; i < key.length(); i++)
    {
        keyCharArray[i] = key.at(i);
    }

    int asciiValue = firstCharacter;

    // Creating an output string stream
    ostringstream asciiStream;

    // Inserting the filename into the stream
    asciiStream << asciiValue;

    // Converting the stream into string format to get the filename in string form
    string fileName = "./File_Storage/" + asciiStream.str();

    fstream file(fileName, std::fstream::in);

    file.seekg(file.beg);

    KeyValueEntry foundKeyValueEntry;
    KeyValueEntry *returnedEntry = NULL;

    // Fetching key-values entry one-by-one from the file
    while (file.read((char *) &foundKeyValueEntry, sizeof(KeyValueEntry)))
    {
        if ((strcmp(foundKeyValueEntry.key, keyCharArray) == 0) && foundKeyValueEntry.isValid)
        {
            returnedEntry = new KeyValueEntry;
            returnedEntry->frequency = 0;
            bzero(returnedEntry->key, 257);
            bzero(returnedEntry->value, 257);

            strcpy(returnedEntry->key, foundKeyValueEntry.key);
            strcpy(returnedEntry->value, foundKeyValueEntry.value);

            returnedEntry->isValid = 1;
            
            break;
        }
    }

    file.close();
    return returnedEntry;
}

// To insert or update a key-value entry in the persistent storage
void insert_on_storage(KeyValueEntry *keyValueEntry)
{
    string keyOfEntry = keyValueEntry->key;
    char firstCharacter = keyOfEntry.at(0);
    int asciiValue = firstCharacter;

    // Creating an output string stream
    ostringstream asciiStream;

    // Inserting the filename into the stream
    asciiStream << asciiValue;

    // Converting the stream into string format to get the filename in string form
    string fileName = "./File_Storage/" + asciiStream.str();

    // Opening/Creating the file
    fstream file(fileName, fstream::in | fstream::out);

    // Checking whether the record already exists or not
    KeyValueEntry entryToBeInserted;
    KeyValueEntry tempKeyValueEntry;
    int isInserted = 0;

    entryToBeInserted.frequency = 0;
    entryToBeInserted.isValid = 1;
    
    bzero(entryToBeInserted.key, 257);
    bzero(entryToBeInserted.value, 257);

    strcpy(entryToBeInserted.key, keyValueEntry->key);
    strcpy(entryToBeInserted.value, keyValueEntry->value);
    
    // Checking if a record with the key already exists on the persistent storage or not
    
    file.seekg(file.beg);

    while (file.read((char *) &tempKeyValueEntry, sizeof(KeyValueEntry)))
    {
        if (((strcmp(tempKeyValueEntry.key, entryToBeInserted.key) == 0) && (tempKeyValueEntry.isValid == 1)) || ((strcmp(tempKeyValueEntry.key, entryToBeInserted.key) != 0) && (tempKeyValueEntry.isValid == 0)))
        {
            long pos = file.tellg();
            file.seekp(pos - sizeof(KeyValueEntry));
            file.write((char *) &entryToBeInserted, sizeof(KeyValueEntry));
            isInserted = 1;
            break;
        }
    }

    if (!isInserted)
    {
        file.close();
        file.open(fileName, fstream::in | fstream::out | fstream::app);
        file.seekp(file.end);
        file.write((char *) &entryToBeInserted, sizeof(KeyValueEntry)); 
    }
    
    file.close();
}

// To delete a key-value entry from the persistent storage
int delete_from_storage(string key)
{
    char firstCharacter = key.at(0);
    int asciiValue = firstCharacter;
    int deletionStatus = 0;

    // Creating an output string stream
    ostringstream asciiStream;

    // Inserting the filename into the stream
    asciiStream << asciiValue;

    // Converting the stream into string format to get the filename in string form
    string fileName = "./File_Storage/" + asciiStream.str();
    
    // Creating/opening the file
    fstream file(fileName, fstream::in | fstream::out);

    // The read pointer points to the beginning of the file
    file.seekg(file.beg);

    KeyValueEntry tempKeyValueEntry;

    while (file.read((char *) &tempKeyValueEntry, sizeof(KeyValueEntry)))
    {
        string tempKeyValueEntryKeyString(tempKeyValueEntry.key);
        if ((tempKeyValueEntryKeyString == key) && (tempKeyValueEntry.isValid == 1))
        {
            tempKeyValueEntry.isValid = 0;
            // The key-value entry exists, so we update its valid entry to mark it as invalid
            long pos = file.tellg();
            file.seekp(pos - sizeof(KeyValueEntry));
            file.write((char *) &tempKeyValueEntry, sizeof(KeyValueEntry));
            deletionStatus = 1;
            break;
        }
    }

    file.close();
    return deletionStatus;
}