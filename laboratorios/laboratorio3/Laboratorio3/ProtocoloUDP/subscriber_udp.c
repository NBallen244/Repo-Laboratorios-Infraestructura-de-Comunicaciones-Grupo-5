#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#define PORT 8080
int main(int argc, char const* argv[]) {
    int valread, client_fd, status;
    struct sockaddr_in serv_addr;
    ssize_t bytes_sent;
    char* login = "SUBSCRIBER_LOGIN";
    char msg[1024];
    char buffer[1024] = { 0 };

    //Definimos la direccion del servidor (local por el momento)
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_addr.s_addr = inet_addr("172.20.65.250");
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_family = AF_INET;

    //Socket UDP del cliente
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    if ((status=connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
        perror("Connection failed");
        return -1;
    }
    printf("Conectado al broker\n");
    bytes_sent=sendto(client_fd, login, strlen(login), 0, (struct sockaddr *)NULL, sizeof(serv_addr));
    printf("Login enviado, esperando respuesta...\n");
    if (bytes_sent < 0) {
        perror("Error enviando login");
        if (client_fd >= 0) close(client_fd);
        return -1;
    }
    valread = recvfrom(client_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)NULL, NULL);


    if (valread > 0) {
        buffer[valread] = '\0';
        if (strncmp(buffer, "SUBSCRIBER_LOGIN_OK", 19) == 0) {
            printf("Login exitoso. Esperando mensajes...\n");
            while(1) {
                valread=recvfrom(client_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)NULL, NULL);
                if (valread<=0){
                    printf("Desconectado del broker");
                    if (client_fd >= 0) close(client_fd);
                    return -1;
                }
                buffer[valread] = '\0';
                printf("Mensaje recibido: %s\n", buffer);
            }
        }
        else{
            printf("Login fallido: %s\n", buffer);
            if (client_fd >= 0) close(client_fd);
            return -1;
        }
    }
    else{
        printf("No se recibio respuesta del broker, cerrando conexion...\n");
        if (client_fd >= 0) close(client_fd);
        return 0;
    }
    if (client_fd >= 0) close(client_fd);
    return 0;
}   