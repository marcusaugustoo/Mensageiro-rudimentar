#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>   
#include <pthread.h> //Adicionado para threads

#define SERVER_IP "127.0.0.1" 
#define SERVER_PORT 8080      
#define MAX_BUFFER 4096
#define MAX_NICK 20
#define MAX_NAME 100

//Funções de Rede

void send_to_socket(int sock, const char* msg) {
    if (send(sock, msg, strlen(msg), 0) < 0) {
        perror("send");
    }
}

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
        } else if (n == 0) { 
            return 0;
        } else { 
            perror("recv");
            return -1;
        }
    }
    buffer[i] = '\n';
    buffer[i + 1] = '\0';
    return i;
}


//Lógica do Cliente

/*Esta é a função da THREAD RECEPTORA.
Fica em loop lendo do servidor e imprimindo na tela.
 */
void* receiver_thread(void* arg) {
    int server_sock = *(int*)arg;
    char buffer[MAX_BUFFER];
    int bytes_read;

    //Loop infinito para ler respostas
    while ((bytes_read = read_line(server_sock, buffer, sizeof(buffer))) > 0) {
        buffer[strcspn(buffer, "\n")] = 0;
        printf("\nSVR: %s\ncli> ", buffer);
        fflush(stdout);
    }
    
    //Se sair do loop, o servidor caiu
    printf("\nServidor desconectou. Pressione Enter para sair.\ncli> ");
    fflush(stdout);
    close(server_sock); 
    
    return NULL;
}

/*
Analisa o comando do usuário (CLI) e formata para o protocolo do servidor.
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

    sscanf(cli_input, "%s", command);

    if (strcmp(command, "quit") == 0) { 
        return 1; 
    }
    
    if (strcmp(command, "register") == 0) {
        sscanf(cli_input, "%s %20s \"%100[^\"]\"", command, arg_nick, arg_name);
        sprintf(protocol_buffer, "REGISTER %s \"%s\"\n", arg_nick, arg_name);
        
    } else if (strcmp(command, "login") == 0) {
        sscanf(cli_input, "%s %20s", command, arg_nick);
        sprintf(protocol_buffer, "LOGIN %s\n", arg_nick);
        
    } else if (strcmp(command, "list") == 0) { 
        sprintf(protocol_buffer, "LIST\n");
        
    } else if (strcmp(command, "logout") == 0) { 
        sprintf(protocol_buffer, "LOGOUT\n");

    } else if (strcmp(command, "delete") == 0) {
        sscanf(cli_input, "%s %20s", command, arg_nick);
        sprintf(protocol_buffer, "DELETE %s\n", arg_nick);

    } else if (strcmp(command, "msg") == 0) {
        sscanf(cli_input, "%s %20s", command, arg_nick); 
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

    if (strlen(protocol_buffer) > 0) {
        send_to_socket(server_sock, protocol_buffer);
    }
    return 0;
}


//Função Main

int main() {
    int server_sock;
    struct sockaddr_in server_addr;
    char cli_buffer[MAX_BUFFER]; 
    pthread_t tid; 

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
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    //3. Conectar ao servidor
    if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Conectado ao servidor em %s:%d.\n", SERVER_IP, SERVER_PORT);
    printf("Digite 'quit' para sair.\n"); 

    //4. Criar a thread receptora
    if (pthread_create(&tid, NULL, receiver_thread, &server_sock) != 0) {
        perror("pthread_create");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    //5. Loop principal
    while (1) {
        printf("cli> ");
        fflush(stdout); 

        if (fgets(cli_buffer, sizeof(cli_buffer), stdin) == NULL) {
            break; 
        }

        // Se o servidor tiver caído, o read_line na outra thread vai fechar o socket. O send falhará.
        if (send(server_sock, "", 0, 0) == -1) { 
             break;
        }

        int quit = parse_and_send(server_sock, cli_buffer);

        if (quit) {
            break; 
        }
        
    }

    //6. Fechar o socket
    printf("Desconectando...\n");
    close(server_sock); 
    pthread_cancel(tid);

    return 0;
}