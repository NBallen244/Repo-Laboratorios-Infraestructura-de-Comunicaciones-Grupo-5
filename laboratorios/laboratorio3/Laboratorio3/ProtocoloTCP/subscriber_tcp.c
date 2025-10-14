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
    //Conectamos al servidor
    if ((status=connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
        perror("Connection failed");
        return -1;
    }
    printf("Conectado al broker\n");
    //Enviamos login
    bytes_sent=send(client_fd, login, strlen(login), 0);
    printf("Login enviado, esperando respuesta...\n");
    if (bytes_sent < 0) {
        perror("Error enviando login");
        if (client_fd >= 0) close(client_fd);
        return -1;
    }
    valread = read(client_fd, buffer, 1024-1);
    // Verificamos si se recibió respuesta del broker confirmando el login
    if (valread > 0) {
        //Al confirmarse, empezamos a esperar mensajes del broker
        buffer[valread] = '\0';
        if (strncmp(buffer, "SUBSCRIBER_LOGIN_OK", 19) == 0) {
            printf("Login exitoso. Esperando mensajes...\n");
            while(1) {
                valread=read(client_fd, buffer, 1024);
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
    //Cerramos el socket
    if (client_fd >= 0) close(client_fd);
    return 0;
}   