#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>   

#define SERVER_IP "127.0.0.1" //LocalHost
#define SERVER_PORT 8080      
#define MAX_BUFFER 4096

#define MAX_NICK 20
#define MAX_NAME 100


void send_to_socket(int sock, const char* msg) {
    if (send(sock, msg, strlen(msg), 0) < 0) {
        perror("send");
    }
}

/*
Lê uma linha (até \n) de um socket. Bloqueante.
Retorna o número de bytes lidos, 0 no disconnect, -1 no erro.
 */
int read_line(int sock, char* buffer, int size) {
    int i = 0;
    char c = '\0';
    int n;

    while (i < size - 1) {
        n = recv(sock, &c, 1, 0); 

        if (n > 0) { 
            if (c == '\r') continue; 
            
            if (c == '\n') {
                buffer[i] = '\n';
                buffer[i + 1] = '\0';
                return i + 1;
            }
            buffer[i] = c;
            i++;
        } else if (n == 0) { //Servidor desconectou
            return 0;
        } else { // Erro
            perror("recv");
            return -1;
        }
    }
    buffer[i] = '\n';
    buffer[i + 1] = '\0';
    return i;
}


/*
Lê e imprime respostas do servidor até receber um "comando final".
 */
void handle_server_response(int server_sock) {
    char buffer[MAX_BUFFER];
    int bytes_read;

    //Loop para ler respostas (comandos que enviam várias linhas).
    while ((bytes_read = read_line(server_sock, buffer, sizeof(buffer))) > 0) {
        
        buffer[strcspn(buffer, "\n")] = 0;
        printf("SVR: %s\n", buffer);

        //Se for uma dessas, a resposta terminou.
        if (strncmp(buffer, "OK", 2) == 0 || 
            strncmp(buffer, "ERROR", 5) == 0 ||
            strncmp(buffer, "USERS_END", 9) == 0) 
        {
            break; 
        }
    }
    
    if (bytes_read == 0) {
        printf("Servidor desconectou.\n");
        exit(EXIT_SUCCESS);
    }
}

/*
Analisa o comando do usuário e formata para o protocolo do servidor.
 */
int parse_and_send(int server_sock, char* cli_input) {
    char protocol_buffer[MAX_BUFFER]; 
    char command[32];
    char arg_nick[MAX_NICK + 1];
    char arg_name[MAX_NAME + 1];

    memset(protocol_buffer, 0, sizeof(protocol_buffer));
    memset(command, 0, sizeof(command));
    memset(arg_nick, 0, sizeof(arg_nick));
    memset(arg_name, 0, sizeof(arg_name));

    //Pega o primeiro comando
    sscanf(cli_input, "%s", command);

    if (strcmp(command, "quit") == 0) { //
        return 1; 
    }
    
    //Processa comandos baseados no CLI
    if (strcmp(command, "register") == 0) {
        sscanf(cli_input, "%s %s \"%[^\"]\"", command, arg_nick, arg_name);
        sprintf(protocol_buffer, "REGISTER %s \"%s\"\n", arg_nick, arg_name);
        
    } else if (strcmp(command, "login") == 0) {
        sscanf(cli_input, "%s %s", command, arg_nick);
        sprintf(protocol_buffer, "LOGIN %s\n", arg_nick);
        
    } else if (strcmp(command, "list") == 0) { 
        sprintf(protocol_buffer, "LIST\n");
        
    } else if (strcmp(command, "logout") == 0) { 
        sprintf(protocol_buffer, "LOGOUT\n");

    } else if (strcmp(command, "delete") == 0) {
        sscanf(cli_input, "%s %s", command, arg_nick);
        sprintf(protocol_buffer, "DELETE %s\n", arg_nick);

    } else if (strcmp(command, "msg") == 0) {
        sscanf(cli_input, "%s %s", command, arg_nick); //Pega "msg" e "apelido_dest"
        
        //Pega o resto da linha como <texto...>
        char* text_start = strchr(cli_input, ' '); 
        if (text_start) {
            text_start = strchr(text_start + 1, ' '); 
        }
        
        if (text_start) {
            sprintf(protocol_buffer, "SEND_MSG %s %s", arg_nick, text_start + 1);
        } else {
            printf("Formato invalido. Use: msg <apelido> <texto...>\n");
            return 0; 
        }

    } else {
        printf("Comando desconhecido: %s\n", command);
        return 0;
    }

    //Envia o comando formatado para o servidor
    if (strlen(protocol_buffer) > 0) {
        send_to_socket(server_sock, protocol_buffer);
    }
    return 0;
}


//Setup do cliente

int main() {
    int server_sock;
    struct sockaddr_in server_addr;
    char cli_buffer[MAX_BUFFER];

    //1. Criar o socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    //2. Configurar o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    //Converte o IP de string para o formato binário
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    //3. Conectar (Connect) ao servidor
    if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Conectado ao servidor em %s:%d.\n", SERVER_IP, SERVER_PORT);
    printf("Digite 'quit' para sair.\n"); 

    //4. Loop principal do Cliente 
    while (1) {
        printf("cli> ");
        fflush(stdout); 

        if (fgets(cli_buffer, sizeof(cli_buffer), stdin) == NULL) {
            break; 
        }

        int quit = parse_and_send(server_sock, cli_buffer);

        if (quit) {
            break; 
        }

        //Se não for "quit", espera a resposta do servidor
        if (strlen(cli_buffer) > 1 && quit == 0) {
            handle_server_response(server_sock);
        }
    }

    //5. Fechar o socket
    printf("Desconectando...\n");
    close(server_sock);

    return 0;
}