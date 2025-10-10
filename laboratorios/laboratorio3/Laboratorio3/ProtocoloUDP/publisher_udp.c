#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define PORT 8080
#define BUFFER_SIZE 1024
int main(int argc, char const* argv[]) {
    int valread, client_fd, status;
    struct sockaddr_in serv_addr;
    ssize_t bytes_sent;
    char* login = "PUBLISHER_LOGIN";
    char* msg1 = "PUBLISH: Gol de Equipo A a B\n";
    char* msg2 = "PUBLISH: Gol de Equipo C a D\n";
    char msg[BUFFER_SIZE];
    char buffer[BUFFER_SIZE] = { 0 };
    int mensajes_enviados=0;
    int mensajes_fallidos=0;


    
    //Definimos la direccion del servidor (local por el momento)
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
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

    if (valread<0){
        printf("No se recibio respuesta del broker. Saliendo\n");
        if (client_fd >= 0) close(client_fd);
        return -1;
    }

    buffer[valread] = '\0';
    if (strncmp(buffer, "PUBLISH_LOGIN_OK", 16) == 0) {
        printf("Login exitoso. Enviando mensajes periodicamente\n");
        while(mensajes_enviados<10) {
            bytes_sent=sendto(client_fd, msg1, strlen(msg1), 0, (struct sockaddr *)NULL, sizeof(serv_addr));
            printf("Mensaje enviado: %s", msg1);
            if (bytes_sent <= 0) {
                perror("Error enviando mensaje 1");
                mensajes_fallidos++;
            }
            bytes_sent=sendto(client_fd, msg2, strlen(msg2), 0, (struct sockaddr *)NULL, sizeof(serv_addr));
            printf("Mensaje enviado: %s", msg2);
            sleep(1);
            if (bytes_sent <= 0) {
                perror("Error enviando mensaje 2");
                mensajes_fallidos++;
            }
            sleep(1);
            mensajes_enviados+=2;
        }
        printf("Enviados %d mensajes con %d fallidos. Saliendo...\n", mensajes_enviados, mensajes_fallidos);
        sendto(client_fd, "FIN", 3, 0, (struct sockaddr *)NULL, sizeof(serv_addr));
        if (client_fd >= 0) close(client_fd);
        return 0;
    }else{
        printf("Login fallido: %s\n", buffer);
        if (client_fd >= 0) close(client_fd);
        return -1;
    }

    if (client_fd >= 0) close(client_fd);
    return 0;
}   