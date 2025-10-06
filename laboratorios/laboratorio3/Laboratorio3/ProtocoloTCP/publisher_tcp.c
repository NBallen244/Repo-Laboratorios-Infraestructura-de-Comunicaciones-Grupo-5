#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 8080
int main(int argc, char const* argv[]) {
    int sockfd;
    struct sockaddr_in serv_addr;

    // Crear socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configurar dirección del servidor
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Enlazar socket
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error al enlazar socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones
    if (listen(sockfd, 3) < 0) {
        perror("Error al escuchar");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Esperando conexiones en el puerto %d...\n", PORT);
    return 0;
}   