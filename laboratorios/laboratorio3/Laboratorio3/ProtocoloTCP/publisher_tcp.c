#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define PORT 8080
int main(int argc, char const* argv[]) {
    int valread, client_fd, status;
    struct sockaddr_in serv_addr;
    ssize_t bytes_sent;
    char* login = "PUBLISHER_LOGIN";
    char* msg1 = "PUBLISH: Gol de Equipo A a B\n";
    char* msg2 = "PUBLISH: Gol de Equipo C a D\n";
    char msg[1024];
    char buffer[1024] = { 0 };
    //Socket TCP del cliente
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }
    //Definimos la direccion del servidor (local por el momento)
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if ((status=connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
        perror("Connection failed");
        return -1;
    }
    printf("Conectado al broker\n");
    bytes_sent=send(client_fd, login, strlen(login), 0);
    printf("Login enviado, esperando respuesta...\n");
    if (bytes_sent < 0) {
        perror("Error enviando login");
        if (client_fd >= 0) close(client_fd);
        return -1;
    }
    valread = read(client_fd, buffer, 1024-1);

    if (valread > 0) {
        buffer[valread] = '\0';
        if (strncmp(buffer, "PUBLISH_LOGIN_OK", 16) == 0) {
            printf("Login exitoso. Enviando mensajes periodicamente\n");
            while(1 && status==0) {
                bytes_sent=send(client_fd, msg1, strlen(msg1), 0);
                printf("Mensaje enviado: %s", msg1);
                if (bytes_sent < 0) {
                    perror("Error enviando mensaje 1");
                    break;
                }
                sleep(5);
                bytes_sent=send(client_fd, msg2, strlen(msg2), 0);
                printf("Mensaje enviado: %s", msg2);
                if (bytes_sent < 0) {
                    perror("Error enviando mensaje 2");
                    break;
                }
                sleep(5);
            }
            printf("Broker desconectado. Saliendo...\n");
        }else{
            printf("Login fallido: %s\n", buffer);
            if (client_fd >= 0) close(client_fd);
            return -1;
        }
    }else{
        printf("No se recibio respuesta del broker. Saliendo\n");
        if (client_fd >= 0) close(client_fd);
        return -1;
    }
    if (client_fd >= 0) close(client_fd);
    return 0;
}   