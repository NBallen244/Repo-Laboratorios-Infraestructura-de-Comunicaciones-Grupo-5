
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
#define MAX_PUBS 5
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define MAX_SUBS 5
#define MAX_CLIENTS (MAX_PUBS + MAX_SUBS)

typedef enum { ROLE_UNKNOWN=0, ROLE_PUBLISHER=1, ROLE_SUBSCRIBER=2 } client_role_t;

// Estructura para manejar clientes
typedef struct {
    int fd;
    client_role_t role;
} client_t;

// Arrays para publishers y subscribers (distinguidos por rol)
static client_t clients[MAX_CLIENTS];
static int pubs_count = 0;
static int subs_count = 0;

// Inicializacion de arrays
static void init_arrays(void){
    for(int i=0;i<MAX_CLIENTS;i++){ clients[i].fd = -1; clients[i].role = ROLE_UNKNOWN;}
}

// Elimina un cliente (publisher o subscriber)
static void remove_client(client_t *c){
    if(c->role == ROLE_PUBLISHER) pubs_count--;
    else if(c->role == ROLE_SUBSCRIBER) subs_count--;
    c->role = ROLE_UNKNOWN;
    if(c->fd >= 0){
        close(c->fd);
        c->fd = -1;
    }
}

// Agrega un subscriber
static int add_sub(int fd){
    for(int i=0;i<MAX_CLIENTS;i++){
        if(clients[i].fd < 0 && subs_count < MAX_SUBS){
            clients[i].fd = fd;
            clients[i].role = ROLE_SUBSCRIBER;
            printf("Cliente registrado como SUBSCRIBER de ID: %d\n", i+1);
            subs_count++;
            return 0;
        }
    }
    return -1;
}
//Agrega un publisher 
static int add_pub(int fd){
    for(int i=0;i<MAX_CLIENTS;i++){
        if(clients[i].fd < 0 && pubs_count < MAX_PUBS){
            clients[i].fd = fd;
            clients[i].role = ROLE_PUBLISHER;
            printf("Cliente registrado como PUBLISHER de ID: %d \n", i+1);
            pubs_count++;
            return 0;
        }
    }
    return -1;
}
//Envia un mensaje a todos los subscribers (asumiendo un único topico)
static void broadcast_to_subs(const char *line, size_t len){
    for(int i=0;i<MAX_CLIENTS;i++){
        if(clients[i].fd >= 0 && clients[i].role == ROLE_SUBSCRIBER){
            ssize_t w = send(clients[i].fd, line, len, 0);
            if(w < 0){
                // Si está roto, cerrar, pues se desconecto el subscriber
                printf("Error enviando a subscriber %d, cerrando conexion\n", i+1);
                remove_client(&clients[i]);
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

    //Definimos la direccion del servidor (local)
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    int opt = 1;
    //reusar el puerto inmediatamente despues de cerrar el programa (no esperar timeout)
    if (setsockopt(listen_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //linqueamos la direccion y el puerto al socket del servidor
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //Ponemos el socket en modo escucha para esperar conexiones entrantes
    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Broker escuchando en el puerto %d ...\n", PORT);

    // Inicializamos arrays de clientes
    init_arrays();
    //Variables de select. Basicamente un poll de sockets
    fd_set readfds;
    int max_sd, sd, activity, valread;
    char buffer[BUFFER_SIZE];

    // Un arreglo de clientes desconocidos para nuevas conexiones (no registrados aun)
    client_t unknowns[MAX_PUBS + MAX_SUBS];
    for (int i = 0; i < MAX_PUBS + MAX_SUBS; i++) {unknowns[i].fd = -1;unknowns[i].role = ROLE_UNKNOWN;}

    while (1) {
        // Reseteamos el poll o set de sockets
        FD_ZERO(&readfds);
        //Agregar el socket del servidor (broker)
        FD_SET(listen_fd, &readfds);
        max_sd = listen_fd;

        //Agregar sockets de los clientes actuales que siguen conectados
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Esperar accion de un cliente (publisher, subscriber, o nuevo)
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        // Nueva conexion entrante (cliente desconocido se conecta al socket del servidor)
        if (FD_ISSET(listen_fd, &readfds)) {
            int addrlen = sizeof(address);
            //definimos el socket del cliente
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
            //se recibe un mensaje de un cliente desconocido
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
                        // Registrar como publisher si hay espacio
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
                        // Registrar como subscriber si hay espacio
                        if (add_sub(sd) != 0) {
                            printf("No se pudo registrar como Subscriber (límite alcanzado)\n");
                            sprintf(msg, "ERROR: No se pudo registrar como Subscriber (límite alcanzado). Cerrando conexion\n");
                            send(sd, msg, strlen(msg), 0);
                            remove_client(&unknowns[i]);
                        }else{
                            // Enviar confirmación de registro
                            sprintf(msg, "SUBSCRIBER_LOGIN_OK\n");
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
        
        //Manejar actividad de clientes registrados
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if (sd < 0) continue;
            //Si se percibe un envio de datos de un cliente registrado
            if (FD_ISSET(sd, &readfds)){
                if(clients[i].role == ROLE_UNKNOWN){
                    printf("Cliente en estado desconocido, cerrando conexion\n");
                    remove_client(&clients[i]);
                }
                // Ya registrado, manejar segun rol
                if(clients[i].role == ROLE_PUBLISHER){
                    valread = read(sd, buffer, BUFFER_SIZE-1);
                    if (valread == 0) {
                        // El publisher se desconectó
                        printf("Publisher ID: %d desconectado\n", i+1);
                        remove_client(&clients[i]);
                    } else {
                        // Mensaje recibido de publisher
                        buffer[valread] = '\0';
                        printf("Mensaje de Publisher ID: %d: %s\n", i+1, buffer);
                        if (strncmp(buffer, "PUBLISH: ", 9) == 0) {
                            // Enviar mensaje a todos los subscribers
                            sscanf(buffer, "PUBLISH: %[^,\n]", msg);
                            printf("Mensaje limpio: %s\n", msg);
                            broadcast_to_subs(msg, strlen(msg));
                            printf("Mensaje de Publisher ID: %d enviado a todos los Subscribers\n", i+1);
                        }else if (strncmp(buffer, "FIN", 3) == 0) {
                            // Publisher quiere desconectarse
                            printf("Publisher ID: %d solicitó desconexión por enviar todos sus mensajes. Retirandolo\n", i+1);
                            remove_client(&clients[i]);
                        }
                        else{
                            // Ignorar mensaje errado
                            printf("Mensaje no válido de Publisher ID: %d, ignorando\n", i+1);
                        }
                        
                    }
                }
                //Seccion que hubiera servido para manejar mensajes de subscribers, pero no es necesario al no enviar mensajes
                //Sin mencionar que su lectura bloqueaba el flujo de mensajes (al no recibir nada)
                /**else if(clients[i].role == ROLE_SUBSCRIBER){
                    valread = read(sd, buffer, BUFFER_SIZE-1);
                    if (valread == 0) {
                        // El subscriber se desconectó
                        printf("Subscriber desconectado\n");
                        remove_client(&clients[i]);
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
                            remove_client(&clients[i]);
                        }
                    }
                }**/
            }
        }
        // Limpiar array de desconocidos (ya registrados o desconectados)
        for (int i = 0; i < MAX_PUBS + MAX_SUBS; i++) {unknowns[i].fd = -1;}
    }
    // Código del main
    // Cerrar todos los sockets abiertos antes de salir
    for(int i=0;i<MAX_PUBS+MAX_SUBS;i++) if(unknowns[i].fd>=0) close(unknowns[i].fd);
    for(int i=0;i<MAX_CLIENTS;i++) if(clients[i].fd>=0) close(clients[i].fd);
    // Cerrar socket de escucha
    if(listen_fd>=0) close(listen_fd);
    return 0;
}