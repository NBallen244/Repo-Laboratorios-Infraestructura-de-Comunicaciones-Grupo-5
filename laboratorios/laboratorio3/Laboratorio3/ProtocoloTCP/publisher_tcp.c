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
int main(int argc, char const* argv[]) {
    int valread, client_fd, status;
    struct sockaddr_in serv_addr;
    ssize_t bytes_sent;
    char* login = "PUBLISHER_LOGIN";
    // Mensajes a enviar (mocks de eventos deportivos)
    char* msg1 = "PUBLISH: Gol de Equipo A a B\n";
    char* msg2 = "PUBLISH: Gol de Equipo C a D\n";
    char msg[1024];
    char buffer[1024] = { 0 };
    int mensajes_enviados=0;
    int mensajes_fallidos=0;
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
    //Conectamos al broker, indicando si hubo error
    if ((status=connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
        perror("Connection failed");
        return -1;
    }
    printf("Conectado al broker\n");
    // Enviar login
    bytes_sent=send(client_fd, login, strlen(login), 0);
    printf("Login enviado, esperando respuesta...\n");
    if (bytes_sent < 0) {
        perror("Error enviando login");
        if (client_fd >= 0) close(client_fd);
        return -1;
    }
    // Esperar respuesta del broker que confirme el login
    valread = read(client_fd, buffer, 1024-1);
    if (valread<=0){
        printf("No se recibio respuesta del broker. Saliendo\n");
        if (client_fd >= 0) close(client_fd);
        return -1;
    }

    buffer[valread] = '\0';
    //Si se verifica, empezamos a enviar mensajes periodicamente hasta completar 10 mensajes.
    if (strncmp(buffer, "PUBLISH_LOGIN_OK", 16) == 0) {
        printf("Login exitoso. Enviando mensajes periodicamente\n");
        while(mensajes_enviados<10) {
            // Envia el mensaje 1 y 2 con un segundo de espera entre ellos
            //(simulamos eventos deportivos que ocurren en el tiempo, no todos juntos)
            bytes_sent=send(client_fd, msg1, strlen(msg1), 0);
            printf("Mensaje enviado: %s", msg1);
            if (bytes_sent <= 0) {
                perror("Error enviando mensaje 1");
                mensajes_fallidos++;
            }
            sleep(1);
            bytes_sent=send(client_fd, msg2, strlen(msg2), 0);
            printf("Mensaje enviado: %s", msg2);
            if (bytes_sent <= 0) {
                perror("Error enviando mensaje 2");
                mensajes_fallidos++;
            }
            sleep(1);
            mensajes_enviados+=2;
        }
        // Enviar mensaje de fin de publicacion una vez enviados los 10 mensajes.
        printf("Enviados %d mensajes con %d fallidos. Saliendo...\n", mensajes_enviados, mensajes_fallidos);
        send(client_fd, "FIN", 3, 0);
        if (client_fd >= 0) close(client_fd);
        return 0;
    }else{
        printf("Login fallido: %s\n", buffer);
        if (client_fd >= 0) close(client_fd);
        return -1;
    }
    // Cerrar el socket antes de salir
    if (client_fd >= 0) close(client_fd);
    return 0;
}   