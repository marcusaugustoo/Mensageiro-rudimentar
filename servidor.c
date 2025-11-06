#include "logica.h"    
#include "protocolo.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>


int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    //1. Criar o socket 
    //socket() é a chamada de baixo nível 
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
    // bind() é a chamada de baixo nível
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    //4. Escutar por conexões
    // listen() é a chamada de baixo nível 
    if (listen(server_sock, 5) < 0) { 
        perror("listen");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor Mensageiro escutando na porta %d...\n", SERVER_PORT);

    //5. Loop principal (Aceitar e Lidar com clientes)
    while (1) {
        //accept() bloqueia até um cliente conectar 
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue; 
        }
        
        handle_connection(client_sock); 
        
    }

    //6. Fechar o socket 
    close(server_sock); 
    return 0;
}