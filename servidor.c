#include "logica.h"    
#include "protocolo.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h> //Adicionado para threads

/*
 Função "worker" que roda em uma nova thread.
Ela apenas chama a handle_connection e se encarrega de liberar a memória do argumento.
 */
void* worker_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    pthread_detach(pthread_self());

    //Chama a lógica de conexão
    handle_connection(client_sock);
    
    return NULL;
}


int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid; 

    //Inicializa a lógica 
    inicializar_logica();

    //1. Criar o socket 
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    //Configura SO para reutilizar a porta 
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //2. Configurar o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(SERVER_PORT);

    //3. Vincular o socket à porta
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    //4. Escutar por conexões
    if (listen(server_sock, 10) < 0) { 
        perror("listen");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor Mensageiro (Multithread) escutando na porta %d...\n", SERVER_PORT);

    //5. Loop principal (Aceitar e Lidar com clientes)
    while (1) {
        //accept() bloqueia até um cliente conectar 
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue; 
        }
        
        //Início da Lógica de Thread
        // Precisamos alocar memória para o socket, pois 'client_sock' será sobrescrita no próximo loop.
        int* client_sock_ptr = malloc(sizeof(int));
        if (client_sock_ptr == NULL) {
            perror("malloc");
            close(client_sock);
            continue;
        }
        *client_sock_ptr = client_sock;
        
        //Cria a thread worker
        if (pthread_create(&tid, NULL, worker_thread, client_sock_ptr) != 0) {
            perror("pthread_create");
            free(client_sock_ptr);
            close(client_sock);
        }
        //O loop principal continua para o próximo accept()
    }

    //6. Fechar o socket 
    close(server_sock); 
    return 0;
}