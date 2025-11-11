#include "logica.h" 
#include "rede.h"   
#include "protocolo.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <pthread.h> //Adicionado para threads

//Estado Global do Servidor 
static User users[MAX_USERS];
static int user_count = 0;

//Mutex (cadeado) para proteger o array 'users'
static pthread_mutex_t users_mutex;

//Inicializa os componentes da lógica
void inicializar_logica(void) {
    if (pthread_mutex_init(&users_mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
}

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
    //Pede o cadeado
    pthread_mutex_lock(&users_mutex);
    
    if (find_user(apelido) != -1) {
        send_to_socket(sock, "ERROR NICK_TAKEN\n");
    } else if (user_count >= MAX_USERS) {
        send_to_socket(sock, "ERROR SERVER_FULL\n");
    } else if (strlen(apelido) == 0 || strlen(nome) == 0) {
        send_to_socket(sock, "ERROR BAD_FORMAT\n");
    } else {
        //Adiciona novo usuário
        strcpy(users[user_count].apelido, apelido);
        strcpy(users[user_count].nome, nome);
        users[user_count].socket_fd = -1; 
        users[user_count].queue.count = 0;
        user_count++;
        printf("Usuario registrado: %s (%s)\n", apelido, nome);
        send_to_socket(sock, "OK\n");
    }

    //Libera o cadeado
    pthread_mutex_unlock(&users_mutex);
}

static void do_login(User** current_user, int sock, char* apelido) {
    MessageQueue queue_to_send;
    int found = 0;
    
    //Pede o cadeado
    pthread_mutex_lock(&users_mutex);

    if (*current_user) {
        send_to_socket(sock, "ERROR BAD_STATE\n"); //Já logado
    } else {
        int user_idx = find_user(apelido);
        if (user_idx == -1) {
            send_to_socket(sock, "ERROR NO_SUCH_USER\n");
        } else if (users[user_idx].socket_fd != -1) {
            send_to_socket(sock, "ERROR ALREADY_ONLINE\n");
        } else {
            //Loga o usuário
            *current_user = &users[user_idx];
            (*current_user)->socket_fd = sock; //Armazena sessão
            printf("Usuario logado: %s (socket_fd=%d)\n", (*current_user)->apelido, sock);

            //Copia a fila para uma variável local
            memcpy(&queue_to_send, &(*current_user)->queue, sizeof(MessageQueue));
            (*current_user)->queue.count = 0; //Limpa a fila global
            found = 1;
        }
    }
    
    //Libera o cadeado
    pthread_mutex_unlock(&users_mutex);
    
    //Envia mensagens (se houver) FORA do lock
    if (found) {
        //Envia mensagens da fila (store-and-forward)
        char msg_buffer[MAX_BUFFER];
        printf("Enviando %d mensagens pendentes para %s\n", queue_to_send.count, apelido);
        for (int i = 0; i < queue_to_send.count; i++) {
            sprintf(msg_buffer, "DELIVER_MSG %s %s\n", queue_to_send.messages[i].from, queue_to_send.messages[i].text);
            send_to_socket(sock, msg_buffer);
        }
        send_to_socket(sock, "OK\n");
    }
}

static void do_list(int sock) {
    //Aloca um buffer grande o suficiente
    char* list_buffer = malloc(MAX_BUFFER * MAX_USERS);
    if (list_buffer == NULL) {
        perror("malloc");
        return;
    }
    char line_buffer[MAX_BUFFER];
    list_buffer[0] = '\0'; 

    //Pede o cadeado
    pthread_mutex_lock(&users_mutex);
    
    //Monta a string inteira DENTRO do lock
    for (int i = 0; i < user_count; i++) {
        // Formato: {nick, online, nome}
        sprintf(line_buffer, "USER %s %s \"%s\"\n", 
            users[i].apelido, 
            (users[i].socket_fd != -1) ? "online" : "offline", 
            users[i].nome);
        strcat(list_buffer, line_buffer);
    }
    
    //Libera o cadeado
    pthread_mutex_unlock(&users_mutex);

    //Envia os dados FORA do lock
    send_to_socket(sock, "USERS_START\n"); 
    if (strlen(list_buffer) > 0) {
        send_to_socket(sock, list_buffer);
    }
    send_to_socket(sock, "USERS_END\n");
    
    free(list_buffer);
}

static void do_send_msg(User* current_user, int sock, char* to_apelido, char* text) {
    int dest_sock = -1;
    char from_apelido[MAX_NICK];
    
    if (!current_user) {
        send_to_socket(sock, "ERROR UNAUTHORIZED\n");
        return;
    }
    
    //Guarda o 'from' antes do lock
    strcpy(from_apelido, current_user->apelido);

    //Pede o cadeado
    pthread_mutex_lock(&users_mutex);
    
    int dest_idx = find_user(to_apelido);
    if (dest_idx == -1) {
        send_to_socket(sock, "ERROR NO_SUCH_USER\n");
    } else {
        User* dest_user = &users[dest_idx];
        if (dest_user->socket_fd != -1) {
            //1. Destinatário está online
            // Apenas pegamos o socket e saímos do lock
            dest_sock = dest_user->socket_fd;
        } else {
            //2. Destinatário offline
            MessageQueue* q = &dest_user->queue;
            if (q->count < MAX_QUEUE) {
                strcpy(q->messages[q->count].from, from_apelido);
                strcpy(q->messages[q->count].text, text);
                q->count++;
                printf("Msg de %s para %s (offline) enfileirada.\n", from_apelido, dest_user->apelido);
            } else {
                printf("Fila de %s cheia, msg de %s descartada.\n", from_apelido, dest_user->apelido);
            }
        }
        send_to_socket(sock, "OK\n");
    }
    
    //Libera o cadeado
    pthread_mutex_unlock(&users_mutex);
    
    //Envia a msg (se online) FORA do lock
    if (dest_sock != -1) {
        char msg_buffer[MAX_BUFFER];
        sprintf(msg_buffer, "DELIVER_MSG %s %s\n", from_apelido, text);
        send_to_socket(dest_sock, msg_buffer);
    }
}

static void do_logout(User** current_user, int sock) {
    if (!*current_user) {
        send_to_socket(sock, "ERROR BAD_STATE\n");
        return;
    }
    
    //Pede o cadeado
    pthread_mutex_lock(&users_mutex);
    
    printf("Usuario deslogado: %s\n", (*current_user)->apelido);
    (*current_user)->socket_fd = -1; // Seta como offline
    *current_user = NULL; 

    pthread_mutex_unlock(&users_mutex);
    send_to_socket(sock, "OK\n");
}

static void do_delete(User** current_user, int sock, char* apelido) {
    char nome_deletado[MAX_NICK]; //Variável para o bugfix

    if (!*current_user) {
        send_to_socket(sock, "ERROR UNAUTHORIZED\n");
        return; 
    }

    //Pede o cadeado
    pthread_mutex_lock(&users_mutex);

    if (strcmp((*current_user)->apelido, apelido) != 0) {
        send_to_socket(sock, "ERROR UNAUTHORIZED\n"); //Só pode deletar a si mesmo
    } else {
        //Guarda o nome antes de deletar
        strcpy(nome_deletado, (*current_user)->apelido);
        
        int user_idx = find_user(apelido);
        users[user_idx] = users[user_count - 1];
        user_count--;
        printf("Usuario deletado: %s\n", nome_deletado);
        
        *current_user = NULL; //Força logout 
        send_to_socket(sock, "OK\n");
    }
    
    //Libera o cadeado
    pthread_mutex_unlock(&users_mutex);
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

    printf("Novo cliente (thread %ld) conectado, socket_fd=%d\n", (long)pthread_self(), client_sock);

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
            p++; 
            if (strcmp(command, "REGISTER") == 0) {
                sscanf(p, "%s \"%[^\"]\"", arg1, arg2);
            } else if (strcmp(command, "LOGIN") == 0) {
                sscanf(p, "%s", arg1);
            } else if (strcmp(command, "DELETE") == 0) {
                sscanf(p, "%s", arg1);
            } else if (strcmp(command, "SEND_MSG") == 0) {
                sscanf(p, "%s", arg1); 
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
            do_login(&current_user, client_sock, arg1); 
        }
        else if (strcmp(command, "LIST") == 0) {
            do_list(client_sock);
        }
        else if (strcmp(command, "SEND_MSG") == 0) {
            do_send_msg(current_user, client_sock, arg1, arg2);
        }
        else if (strcmp(command, "LOGOUT") == 0) {
            do_logout(&current_user, client_sock); 
        }
        else if (strcmp(command, "DELETE") == 0) {
            do_delete(&current_user, client_sock, arg1); 
        }
        else {
            if (strlen(command) > 1 && strcmp(command, "\n") != 0) 
                send_to_socket(client_sock, "ERROR BAD_FORMAT\n");
        }
    }

    //Se o cliente cair, o servidor limpa a sessão
    if (current_user) {
        //Pede o cadeado para limpar a sessão
        pthread_mutex_lock(&users_mutex);
        
        printf("Cliente %s (socket_fd=%d) desconectou (queda).\n", current_user->apelido, client_sock);
        current_user->socket_fd = -1; //Libera sessão
        current_user = NULL;
        
        //Libera o cadeado
        pthread_mutex_unlock(&users_mutex);
    } else {
        printf("Cliente (nao logado) (socket_fd=%d) desconectou.\n", client_sock);
    }
    
    close(client_sock); 
}