#include <stdio.h>
#if  defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h> // For Windows sockets
    #include <ws2tcpip.h> // For some Winsock functions like getaddrinfo
    #pragma comment(lib, "Ws2_32.lib") // Link with Ws2_32.lib
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h> // For close()
#endif
#define PORT 8080

int main(int argc, char const* argv[]) {
    WSADATA wsaData; // For server
    SOCKET connectSocket = INVALID_SOCKET; // For client
    struct addrinfo *result = NULL, *ptr = NULL, hints;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    

    
    
    WSACleanup();
    return 0;
}