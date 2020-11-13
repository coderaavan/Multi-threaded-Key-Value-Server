CC=g++
FILE_DIR="File_Storage/"

all: KVServer KVClient
KVServer: KVServer.o storageManager.o
	$(CC) KVServer.o storageManager.o -o KVServer -lpthread
	mkdir $(FILE_DIR)
KVServer.o: KVServer.cpp storageManager.cpp storageManager.hpp
	$(CC) -c KVServer.cpp
storageManager.o: storageManager.cpp storageManager.hpp
	$(CC) -c storageManager.cpp
KVClient: KVClient.o KVClientLibrary.o
	$(CC) KVClient.o KVClientLibrary.o -o KVClient
KVClient.o: KVClient.cpp KVClientLibrary.o KVClientLibrary.hpp
	$(CC) -c KVClient.cpp KVClientLibrary.cpp
KVClientLibrary.o: KVClientLibrary.cpp KVClientLibrary.hpp
	$(CC) -c KVClientLibrary.cpp
clean: 
	rm -rf *.o KVServer KVClient
	rm -rf $(FILE_DIR)