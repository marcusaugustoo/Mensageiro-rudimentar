#include "logica.h" 
#include "rede.h"   
#include "protocolo.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 

//Estado Global do Servidor 
static User users[MAX_USERS];
static int user_count = 0;

//Funções Auxiliares (static) 

// Encontra um usuário pelo apelido. Retorna o índice ou -1.
static int find_user(const char* apelido) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].apelido, apelido) == 0) {
            return i;
        }
    }
    return -1;
}

//Funções de Lógica (Comandos)
static void do_register(int sock, char* apelido, char* nome) {
    if (find_user(apelido) != -1) {
        send_to_socket(sock, "ERROR NICK_TAKEN\n");
    } else if (user_count >= MAX_USERS) {
        send_to_socket(sock, "ERROR SERVER_FULL\n");
    } else if (strlen(apelido) == 0 || strlen(nome) == 0) {
        send_to_socket(sock, "ERROR BAD_FORMAT\n");
    } else {
        // Adiciona novo usuário
        strcpy(users[user_count].apelido, apelido);
        strcpy(users[user_count].nome, nome);
        users[user_count].socket_fd = -1; 
        users[user_count].queue.count = 0;
        user_count++;
        printf("Usuario registrado: %s (%s)\n", apelido, nome);
        send_to_socket(sock, "OK\n");
    }
}

static void do_login(User** current_user, int sock, char* apelido) {
    if (*current_user) {
        send_to_socket(sock, "ERROR BAD_STATE\n"); // Já logado
        return;
    }
    int user_idx = find_user(apelido);
    if (user_idx == -1) {
        send_to_socket(sock, "ERROR NO_SUCH_USER\n");
    } else if (users[user_idx].socket_fd != -1) {
        send_to_socket(sock, "ERROR ALREADY_ONLINE\n");
    } else {
        //Loga o usuário
        *current_user = &users[user_idx];
        (*current_user)->socket_fd = sock; // Armazena sessão
        printf("Usuario logado: %s (socket_fd=%d)\n", (*current_user)->apelido, sock);
        send_to_socket(sock, "OK\n");

        //Envia mensagens da fila (store-and-forward)
        MessageQueue* q = &(*current_user)->queue;
        char msg_buffer[MAX_BUFFER];
        printf("Enviando %d mensagens pendentes para %s\n", q->count, (*current_user)->apelido);
        for (int i = 0; i < q->count; i++) {
            sprintf(msg_buffer, "DELIVER_MSG %s %s\n", q->messages[i].from, q->messages[i].text);
            send_to_socket(sock, msg_buffer);
        }
        q->count = 0; 
    }
}

static void do_list(int sock) {
    char list_buffer[MAX_BUFFER];
    send_to_socket(sock, "USERS_START\n"); 
    for (int i = 0; i < user_count; i++) {
        // Formato: {nick, online, nome}
        sprintf(list_buffer, "USER %s %s \"%s\"\n", 
            users[i].apelido, 
            (users[i].socket_fd != -1) ? "online" : "offline", 
            users[i].nome);
        send_to_socket(sock, list_buffer);
    }
    send_to_socket(sock, "USERS_END\n");
}

static void do_send_msg(User* current_user, int sock, char* to_apelido, char* text) {
    if (!current_user) {
        send_to_socket(sock, "ERROR UNAUTHORIZED\n");
        return;
    }
    int dest_idx = find_user(to_apelido);
    if (dest_idx == -1) {
        send_to_socket(sock, "ERROR NO_SUCH_USER\n");
    } else {
        User* dest_user = &users[dest_idx];
        if (dest_user->socket_fd != -1) {
            //1. Destinatário está online
            char msg_buffer[MAX_BUFFER];
            sprintf(msg_buffer, "DELIVER_MSG %s %s\n", current_user->apelido, text);
            send_to_socket(dest_user->socket_fd, msg_buffer);
        } else {
            //2. Destinatário offline
            MessageQueue* q = &dest_user->queue;
            if (q->count < MAX_QUEUE) {
                strcpy(q->messages[q->count].from, current_user->apelido);
                strcpy(q->messages[q->count].text, text);
                q->count++;
                printf("Msg de %s para %s (offline) enfileirada.\n", current_user->apelido, dest_user->apelido);
            } else {
                printf("Fila de %s cheia, msg de %s descartada.\n", dest_user->apelido, current_user->apelido);
            }
        }
        send_to_socket(sock, "OK\n");
    }
}

static void do_logout(User** current_user, int sock) {
    if (!*current_user) {
        send_to_socket(sock, "ERROR BAD_STATE\n");
    } else {
        printf("Usuário deslogado: %s\n", (*current_user)->apelido);
        (*current_user)->socket_fd = -1; // Seta como offline
        *current_user = NULL; 
        send_to_socket(sock, "OK\n");
    }
}

static void do_delete(User** current_user, int sock, char* apelido) {
    if (!*current_user) {
        send_to_socket(sock, "ERROR UNAUTHORIZED\n");
    } else if (strcmp((*current_user)->apelido, apelido) != 0) {
        send_to_socket(sock, "ERROR UNAUTHORIZED\n"); //Só pode deletar a si mesmo
    } else {
        int user_idx = find_user(apelido);
        users[user_idx] = users[user_count - 1];
        user_count--;
        printf("Usuario deletado: %s\n", (*current_user)->apelido);
        
        *current_user = NULL; //Força logout 
        send_to_socket(sock, "OK\n");
    }
}


//Função Pública de Conexão
void handle_connection(int client_sock) {
    char buffer[MAX_BUFFER];
    char command[32];
    char arg1[MAX_NAME]; 
    char arg2[MAX_BUFFER]; 
    int bytes_read;
    
    //Ponteiro para o usuário logado nesta sessão
    User* current_user = NULL; 

    printf("Novo cliente conectado, socket_fd=%d\n", client_sock);

    //Loop principal: lê comandos do cliente
    while ((bytes_read = read_line(client_sock, buffer, sizeof(buffer))) > 0) {
        
        //Zera os argumentos
        memset(command, 0, sizeof(command));
        memset(arg1, 0, sizeof(arg1));
        memset(arg2, 0, sizeof(arg2));

        //Tenta processar o comando
        char* p = buffer;
        int n_scanned = sscanf(p, "%s", command); 
        if (n_scanned <= 0) continue; 

        p = strchr(p, ' '); 

        if (p) {
            p++; // Pula o espaço
            if (strcmp(command, "REGISTER") == 0) {
                sscanf(p, "%s \"%[^\"]\"", arg1, arg2);
            } else if (strcmp(command, "LOGIN") == 0) {
                sscanf(p, "%s", arg1);
            } else if (strcmp(command, "DELETE") == 0) {
                sscanf(p, "%s", arg1);
            } else if (strcmp(command, "SEND_MSG") == 0) {
                sscanf(p, "%s", arg1); // <to>
                char* msg_text = strchr(p, ' ');
                if (msg_text && (msg_text + 1) < (p + strlen(p))) {
                    strncpy(arg2, msg_text + 1, sizeof(arg2) - 1);
                    arg2[strcspn(arg2, "\n")] = 0; 
                }
            }
        }

        if (strcmp(command, "REGISTER") == 0) {
            do_register(client_sock, arg1, arg2);
        } 
        else if (strcmp(command, "LOGIN") == 0) {
            do_login(&current_user, client_sock, arg1); //Passa o endereço do ponteiro
        }
        else if (strcmp(command, "LIST\n") == 0) {
            do_list(client_sock);
        }
        else if (strcmp(command, "SEND_MSG") == 0) {
            do_send_msg(current_user, client_sock, arg1, arg2);
        }
        else if (strcmp(command, "LOGOUT\n") == 0) {
            do_logout(&current_user, client_sock); //Passa o endereço do ponteiro
        }
        else if (strcmp(command, "DELETE") == 0) {
            do_delete(&current_user, client_sock, arg1); //Passa o endereço do ponteiro
        }
        else {
            if (strlen(command) > 1 && strcmp(command, "\n") != 0) 
                send_to_socket(client_sock, "ERROR BAD_FORMAT\n");
        }
    }

    //Se o cliente cair, o servidor limpa a sessão
    if (current_user) {
        printf("Cliente %s (socket_fd=%d) desconectou (queda).\n", current_user->apelido, client_sock);
        current_user->socket_fd = -1; //Libera sessão
        current_user = NULL;
    } else {
        printf("Cliente (nao logado) (socket_fd=%d) desconectou.\n", client_sock);
    }
    
    close(client_sock); 
}