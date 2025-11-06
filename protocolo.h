#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#define SERVER_PORT 8080 //Porta configurável
#define MAX_BUFFER 4096  //Buffer máximo para mensagens
#define MAX_USERS 50
#define MAX_NICK 20
#define MAX_NAME 100
#define MAX_MSG 256
#define MAX_QUEUE 10     //Máximo de mensagens na fila offline

//Estrutura para uma mensagem na fila
typedef struct {
    char from[MAX_NICK];
    char text[MAX_MSG];
} Message;

//Fila de mensagens 
typedef struct {
    Message messages[MAX_QUEUE];
    int count;
} MessageQueue;

//Estrutura do Usuário
typedef struct {
    char apelido[MAX_NICK];
    char nome[MAX_NAME];
    int socket_fd; //ID do socket, -1 se offline
    MessageQueue queue; //Fila de mensagens pendentes
} User;


#endif 