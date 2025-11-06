#ifndef REDE_H_
#define REDE_H_

#include "protocolo.h"

//Envia uma string para o socket
void send_to_socket(int sock, const char* msg);

//Lê uma linha (até \n) de um socket. Bloqueante.
int read_line(int sock, char* buffer, int size);

#endif 