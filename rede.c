#include "rede.h"
#include <stdio.h>      
#include <string.h>     
#include <sys/socket.h> 
#include <unistd.h>    

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
        //recv() é a chamada de baixo nível 
        n = recv(sock, &c, 1, 0);

        if (n > 0) { 
            //Remove \r 
            if (c == '\r') continue; 
            
            if (c == '\n') {
                buffer[i] = '\n';
                buffer[i + 1] = '\0';
                return i + 1;
            }
            buffer[i] = c;
            i++;
        } else if (n == 0) { //Cliente desconectou
            return 0;
        } else { //Erro
            perror("recv");
            return -1;
        }
    }
    //Linha muito longa, trunca
    buffer[i] = '\n';
    buffer[i + 1] = '\0';
    return i;
}