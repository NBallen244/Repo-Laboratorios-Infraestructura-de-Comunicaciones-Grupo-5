
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
    struct sockaddr_in addr;
    client_role_t role;
    int in_use;
} udp_client_t;

// Arrays para publishers y subscribers
static udp_client_t clients[MAX_CLIENTS];
static int pubs_count = 0;
static int subs_count = 0;

// Inicializacion de arrays
static void init_arrays(void){
    for(int i=0;i<MAX_CLIENTS;i++){ clients[i].addr = (struct sockaddr_in){0}; clients[i].role = ROLE_UNKNOWN; clients[i].in_use = 0;}
}

//Comparar direcciones de clientes para saber si son
static int same_peer(const struct sockaddr_in* a, const struct sockaddr_in* b){
    return a->sin_family==b->sin_family &&
    a->sin_addr.s_addr==b->sin_addr.s_addr &&
    a->sin_port==b->sin_port;
}

//Encontrar cliente por direccion
static int find_client(const struct sockaddr_in* addr){
    for(int i=0;i<MAX_CLIENTS;i++) if(clients[i].in_use && same_peer(&clients[i].addr, addr)) return i;
    return -1;
}

// Agrega un cliente (publisher o subscriber)
static int add_client(const struct sockaddr_in* addr, client_role_t role){
    for(int i=0;i<MAX_CLIENTS;i++) if(!clients[i].in_use){
        clients[i].in_use=1; clients[i].role=role; clients[i].addr=*addr; return i;
    }
    return -1;
}

// Elimina un cliente (publisher o subscriber)
static void remove_client(udp_client_t *c){
    for(int i=0;i<MAX_CLIENTS;i++){
        if(same_peer(&clients[i].addr, &c->addr)){
            clients[i].in_use = 0;
            clients[i].role = ROLE_UNKNOWN;
            clients[i].addr = (struct sockaddr_in){0};
            return;
        }
    }
}

// Agrega un subscriber
static int add_sub(const struct sockaddr_in* addr){
    if (subs_count >= MAX_SUBS) return -1;
    subs_count++;
    return add_client(addr, ROLE_SUBSCRIBER);
}
// Agrega un publisher
static int add_pub(const struct sockaddr_in* addr){
    if (pubs_count >= MAX_PUBS) return -1;
    pubs_count++;
    return add_client(addr, ROLE_PUBLISHER);
}


static void broadcast_to_subs(const char *line, size_t len, int socket){
    for(int i=0;i<MAX_CLIENTS;i++){
        if(clients[i].in_use && clients[i].role == ROLE_SUBSCRIBER){
            ssize_t w = sendto(socket, line, len, 0, (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
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
    //Socket UDP del broker/servidor
    bzero(&address, sizeof(address));
    listen_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    int opt = 1;
    //reusar el puerto inmediatamente despues de cerrar el programa
    if (setsockopt(listen_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //linqueamos port al socket
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Broker escuchando en el puerto %d ...\n", PORT);

    // Inicializamos arrays de clientes
    init_arrays();
    fd_set readfds;
    int max_sd, sd, activity, valread;
    char buffer[BUFFER_SIZE]={0};


    while (1) {
        // Resetear sockets
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        max_sd = listen_fd;

        // Esperar accion de un cliente (publisher o subscriber)
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        // Nueva conexion entrante
        if (FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in cli; socklen_t clen = sizeof(cli);
            valread = recvfrom(listen_fd, buffer, BUFFER_SIZE-1, 0, (struct sockaddr *)&cli, &clen);
            if (valread < 0) {
                perror("recvfrom");
                int descarte=find_client(&cli);
                if(descarte>=0) {
                    if (clients[descarte].role == ROLE_PUBLISHER) {pubs_count--; printf("Publisher ID: %d desconectado\n", descarte+1);}
                    else if (clients[descarte].role == ROLE_SUBSCRIBER) {subs_count--; printf("Subscriber ID: %d desconectado\n", descarte+1);}
                    remove_client(&clients[descarte]);
                }
                continue;
            }
            // Agregar a la lista de clientes desconocidos
            buffer[valread] = '\0';
            int index=find_client(&cli);
            if(index<0){
                printf("Nuevo cliente conectado, esperando login\n");
                printf("Mensaje recibido: %s\n", buffer);
                // Cliente no registrado, esperar login
                if(strncmp(buffer, "PUBLISHER_LOGIN", 16)==0){
                    printf("Login de PUBLISHER recibido\n");
                    // Registrar como publisher
                    index = add_pub(&cli);
                    if(index<0){
                        printf("No se pudo registrar como Publisher (límite alcanzado)\n");
                        sprintf(msg, "ERROR: No se pudo registrar como Publisher (límite alcanzado). Cerrando conexion\n");
                        sendto(listen_fd, msg, strlen(msg), 0, (struct sockaddr *)&cli, clen);
                    }else{
                        // Enviar confirmación de registro
                        printf("Publisher registrado correctamente bajo ID: %d\n", index+1);
                        sprintf(msg, "PUBLISH_LOGIN_OK\n");
                        sendto(listen_fd, msg, strlen(msg), 0, (struct sockaddr *)&cli, clen);
                    }
                }
                else if (strncmp(buffer, "SUBSCRIBER_LOGIN", 16)==0){
                    printf("Login de SUBSCRIBER recibido\n");
                    // Registrar como subscriber
                    index = add_sub(&cli);
                    if(index<0){
                        printf("No se pudo registrar como Subscriber (límite alcanzado)\n");
                        sprintf(msg, "ERROR: No se pudo registrar como Subscriber (límite alcanzado). Cerrando conexion\n");
                        sendto(listen_fd, msg, strlen(msg), 0, (struct sockaddr *)&cli, clen);
                    }else{
                        // Enviar confirmación de registro
                        printf("Subscriber registrado correctamente bajo ID: %d\n", index+1);
                        sprintf(msg, "SUBSCRIBER_LOGIN_OK\n");
                        sendto(listen_fd, msg, strlen(msg), 0, (struct sockaddr *)&cli, clen);
                    }
                }
                else{
                    // Aclara que debe registrarse primero en una de las dos categorias
                    printf("Cliente no registrado intentando enviar mensaje. cerrando conexion\n");
                    sprintf(msg, "ERROR: Debe registrarse primero como PUBLISHER o SUBSCRIBER. cerrando conexion\n");
                    sendto(listen_fd, msg, strlen(msg), 0, (struct sockaddr *)&cli, clen);
                }
            }
            else{
                // Cliente ya registrado, procesar mensaje
                if(clients[index].role == ROLE_PUBLISHER){
                    printf("Mensaje recibido de Publisher %d: %s\n", index+1, buffer);

                    if (strncmp(buffer, "PUBLISH: ", 9) == 0) {
                        // Enviar mensaje a todos los subscribers
                        sscanf(buffer, "PUBLISH: %[^,\n]", msg);
                        printf("Mensaje limpio: %s\n", msg);
                        broadcast_to_subs(msg, strlen(msg), listen_fd);
                        printf("Mensaje de Publisher ID: %d enviado a todos los Subscribers\n", index+1);
                    }else if (strncmp(buffer, "FIN", 3) == 0) {
                        // Publisher quiere desconectarse
                        printf("Publisher ID: %d solicitó desconexión por enviar todos sus mensajes. Retirandolo\n", index+1);
                        remove_client(&clients[index]);
                    }
                    else{
                        // Ignorar mensaje errado
                        printf("Mensaje no válido o incompleto de Publisher ID: %d, ignorando\n", index+1);
                    }
                }else if(clients[index].role == ROLE_SUBSCRIBER){
                    printf("Mensaje recibido de Subscriber %d (ignorado): %s\n", index+1, buffer);
                }else{
                    printf("Cliente en estado desconocido intentando enviar mensaje. cerrando conexion\n");
                    sprintf(msg, "ERROR: Estado desconocido. cerrando conexion\n");
                    sendto(listen_fd, msg, strlen(msg), 0, (struct sockaddr *)&cli, clen);
                }
            }
        }        
    }
    // Código del main
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use) {
            if (clients[i].role == ROLE_PUBLISHER) {
                pubs_count--;
            } else if (clients[i].role == ROLE_SUBSCRIBER) {
                subs_count--;
            }
            clients[i].in_use = 0;
            clients[i].role = ROLE_UNKNOWN;
            clients[i].addr = (struct sockaddr_in){0};
        }
    }
    if(listen_fd>=0) close(listen_fd);
    return 0;
}