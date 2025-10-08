
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
#define MAX_PUBS 5
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define MAX_SUBS 5
#define MAX_CLIENTS (MAX_PUBS + MAX_SUBS)

// Estructura para manejar clientes
typedef struct {
    int fd;
} client_t;

// Arrays para publishers y subscribers
static client_t pubs[MAX_PUBS];
static client_t subs[MAX_SUBS];

// Inicializacion de arrays
static void init_arrays(void){
    for(int i=0;i<MAX_PUBS;i++){ pubs[i].fd = -1;}
    for(int i=0;i<MAX_SUBS;i++){ subs[i].fd = -1;}
}

// Elimina un cliente (publisher o subscriber)
static void remove_client(client_t *c){
    if(c->fd >= 0){
        close(c->fd);
        c->fd = -1;
    }
}

// Agrega un subscriber al array de subscribers
static int add_sub(int fd){
    for(int i=0;i<MAX_SUBS;i++){
        if(subs[i].fd < 0){
            subs[i].fd = fd;
            printf("Cliente registrado como SUBSCRIBER numero %d\n", i+1);
            return 0;
        }
    }
    return -1;
}

static int add_pub(int fd){
    for(int i=0;i<MAX_PUBS;i++){
        if(pubs[i].fd < 0){
            pubs[i].fd = fd;
            printf("Cliente registrado como PUBLISHER numero %d \n", i+1);
            return 0;
        }
    }
    return -1;
}

static void broadcast_to_subs(const char *line, size_t len){
    for(int i=0;i<MAX_SUBS;i++){
        if(subs[i].fd >= 0){
            ssize_t w = send(subs[i].fd, line, len, 0);
            if(w < 0){
                // Si está roto, cerrar, pues se desconecto el subscriber
                printf("Error enviando a subscriber %d, cerrando conexion\n", i+1);
                remove_client(&subs[i]);
            }else{
                printf("Mensaje enviado a subscriber %d\n", i+1);
            }
        }
    }
}



int main(int argc, char const* argv[]) {
    int listen_fd, client_socket;
    struct sockaddr_in address;
    char msg[BUFFER_SIZE];
    socklen_t addrlen = sizeof(address);
    //Socket TCP del broker/servidor
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //linqueamos port al socket
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //Ponemos el socket en modo escucha
    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Broker escuchando en el puerto %d ...\n", PORT);

    // Inicializamos arrays de clientes
    init_arrays();
    fd_set readfds;
    int max_sd, sd, activity, valread;
    char buffer[BUFFER_SIZE];

    client_t unknowns[MAX_PUBS + MAX_SUBS];
    for (int i = 0; i < MAX_PUBS + MAX_SUBS; i++) {unknowns[i].fd = -1;}

    while (1) {
        // Resetear sockets
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        max_sd = listen_fd;

        // Agregar sockets de publishers
        for (int i = 0; i < MAX_PUBS; i++) {
            sd = pubs[i].fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Agregar sockets de subscribers
        for (int i = 0; i < MAX_SUBS; i++) {
            sd = subs[i].fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Esperar accion de un cliente (publisher o subscriber)
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        // Nueva conexion entrante
        if (FD_ISSET(listen_fd, &readfds)) {
            int addrlen = sizeof(address);
            client_socket = accept(listen_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
            printf("Nueva conexión aceptada\n");
            // Agregar a la lista de clientes desconocidos
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (unknowns[i].fd == -1) {
                    printf("Cliente agregado a lista de desconocidos en posicion %d\n", i+1);
                    unknowns[i].fd = client_socket;
                    FD_SET(client_socket, &readfds);
                    if (client_socket > max_sd) max_sd = client_socket;
                    break;
                }
            }
        }

        //Manejar Nuevas Conexiones
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = unknowns[i].fd;
            if (sd < 0) continue;
            if (FD_ISSET(sd, &readfds)) {
                // Leer datos del socket
                printf("Esperando datos de login del cliente %d\n", i+1);
                valread = read(sd, buffer, BUFFER_SIZE-1);
                if (valread == 0) {
                    // El cliente se desconectó
                    printf("Cliente desconectado\n");
                    remove_client(&unknowns[i]);
                } else {
                    // Procesar el mensaje recibido
                    buffer[valread] = '\0';
                    printf("Mensaje recibido: %s\n", buffer);

                    if (strncmp(buffer, "PUBLISHER_LOGIN", 16) == 0) {
                        // Registrar como publisher
                        if (add_pub(sd) != 0) {
                            printf("No se pudo registrar como Publisher (límite alcanzado)\n");
                            sprintf(msg, "ERROR: No se pudo registrar como Publisher (límite alcanzado). Cerrando conexion\n");
                            send(sd, msg, strlen(msg), 0);
                            remove_client(&unknowns[i]);
                        }else{
                            // Enviar confirmación de registro
                            sprintf(msg, "PUBLISH_LOGIN_OK\n");
                            send(sd, msg, strlen(msg), 0);
                        }
                    } else if (strncmp(buffer, "SUBSCRIBER_LOGIN", 16) == 0) {
                        // Registrar como subscriber
                        if (add_sub(sd) != 0) {
                            printf("No se pudo registrar como Subscriber (límite alcanzado)\n");
                            sprintf(msg, "ERROR: No se pudo registrar como Subscriber (límite alcanzado). Cerrando conexion\n");
                            send(sd, msg, strlen(msg), 0);
                            remove_client(&unknowns[i]);
                        }else{
                            // Enviar confirmación de registro
                            sprintf(msg, "SUBSCRIBE_LOGIN_OK\n");
                            send(sd, msg, strlen(msg), 0);
                        }
                    } else {
                        // Aclara que debe registrarse primero en una de las dos categorias
                        printf("Cliente no registrado intentando enviar mensaje. cerrando conexion\n");
                        sprintf(msg, "ERROR: Debe registrarse primero como PUBLISHER o SUBSCRIBER. cerrando conexion\n");
                        send(sd, msg, strlen(msg), 0);
                        remove_client(&unknowns[i]);
                    }
                }
            }
        }

        // Manejar actividad de publishers
        for (int i = 0; i < MAX_PUBS; i++) {
            sd = pubs[i].fd;
            if (sd < 0) continue;

            if (FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, BUFFER_SIZE-1);
                if (valread == 0) {
                    // El publisher se desconectó
                    printf("Publisher %d desconectado\n", i+1);
                    remove_client(&pubs[i]);
                } else {
                    // Mensaje recibido de publisher
                    buffer[valread] = '\0';
                    printf("Mensaje de Publisher %d: %s\n", i+1, buffer);
                    if (strncmp(buffer, "PUBLISH: ", 9) != 0) {
                        // Ignorar si ya está registrado
                        printf("Mensaje no válido de Publisher %d, ignorando\n", i+1);
                        continue;
                    }else{
                        sscanf(buffer, "PUBLISH: %[^,\n]", msg);
                        printf("Mensaje limpio: %s\n", msg);
                        broadcast_to_subs(msg, strlen(msg));
                        printf("Mensaje de Publisher %d enviado a todos los Subscribers\n", i+1);
                    }
                    
                }
            }
        }

        // Manejar actividad de subscribers
        for (int i = 0; i < MAX_SUBS; i++) {
            sd = subs[i].fd;
            if (sd < 0) continue;

            if (FD_ISSET(sd, &readfds)) {
                valread = read(sd, buffer, BUFFER_SIZE-1);
                if (valread == 0) {
                    // El subscriber se desconectó
                    printf("Subscriber desconectado\n");
                    remove_client(&subs[i]);
                } else {
                    // Mensaje recibido de subscriber (no se espera que envíen mensajes)
                    buffer[valread] = '\0';
                    printf("Mensaje de Subscriber %d (no esperado): %s\n", i+1, buffer);
                    if (strncmp(buffer, "FIN", 3) != 0) {
                        // Ignorar si ya está registrado
                        printf("Mensaje no válido de Subscriber %d, ignorando\n", i+1);
                        continue;
                    }else{
                        // Subscriber quiere desconectarse
                        printf("Subscriber %d solicitó desconexión\n", i+1);
                        send(sd, "BYE\n", 4, 0);
                        remove_client(&subs[i]);
                    }
                }
            }
        }
        for (int i = 0; i < MAX_PUBS + MAX_SUBS; i++) {unknowns[i].fd = -1;}
    }
    // Código del main

    for(int i=0;i<MAX_PUBS+MAX_SUBS;i++) if(unknowns[i].fd>=0) close(unknowns[i].fd);
    for(int i=0;i<MAX_PUBS;i++) if(pubs[i].fd>=0) close(pubs[i].fd);
    for(int i=0;i<MAX_SUBS;i++) if(subs[i].fd>=0) close(subs[i].fd);
    if(listen_fd>=0) close(listen_fd);
    return 0;
}