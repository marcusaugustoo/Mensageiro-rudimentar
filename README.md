# 💬 Mensageiro Rudimentar  
### Trabalho de Redes de Computadores

Professor: Irineu Sotoma

Autores: Caio K. F. Mendes, Marcus Augusto F. Madureira, Mariana C. Piccini

---

Este é um sistema de **mensageiro cliente/servidor simples (um-a-um)**, desenvolvido em **C** para **Linux**.  
Toda a comunicação é feita via **TCP** utilizando **Sockets POSIX de baixo nível**.

---

## Visão Geral

O projeto implementa um **servidor concorrente (multithread)** capaz de lidar com múltiplos clientes simultaneamente.  
O cliente também é multithread, possuindo uma thread para **enviar comandos** e outra para **receber mensagens em tempo real**.

---

## ⚙️ Funcionalidades

- 📋 **Cadastro (`register`)** e **autenticação (`login`)** de usuários  
- 👥 **Listagem de usuários (`list`)** com status **online/offline**  
- 💌 **Envio de mensagens (`msg`)** individuais (um-a-um)  
- 📨 **Fila de mensagens (Store-and-Forward)**:  
  Mensagens enviadas a usuários offline são armazenadas e entregues no próximo login  

---

## 📁 Estrutura do Projeto

### 🖥️ Servidor
- servidor.c  
- logica.c  
- logica.h  
- rede.c  
- rede.h  
- protocolo.h

### 💻 Cliente
- cliente.c

---

## Como Compilar e Executar

É necessário ter a biblioteca **pthread** (POSIX Threads) instalada no sistema.

### 1️⃣ Compilar o Servidor
No terminal, dentro da pasta do servidor:
```bash
gcc -Wall -g servidor.c logica.c rede.c -o servidor -lpthread
```

### 2️⃣ Compilar o Cliente
No terminal, dentro da pasta do cliente:
```bash
gcc -Wall -g cliente.c -o cliente -lpthread
```

### 3️⃣ Executar o Servidor
Em um terminal:
```bash
./servidor
```
Saída esperada:
```
Servidor Mensageiro (Multithread) escutando na porta 8080...
```

### 4️⃣ Executar o(s) Cliente(s)
Em um ou mais terminais novos:
```bash
./cliente
```
Saída esperada:
```
Conectado ao servidor em 127.0.0.1:8080.
```

---

## 💬 Comandos do Cliente (CLI)

Após conectar com `./cliente`, os seguintes comandos estão disponíveis:

| Comando | Descrição | Exemplo |
|----------|------------|----------|
| register <apelido> "<Nome Completo>" | Cadastra um novo usuário | register marcus "Marcus Madureira" |
| login <apelido> | Faz login no sistema | login marcus |
| list | Lista os usuários online e offline | list |
| msg <apelido_dest> <texto...> | Envia mensagem para outro usuário | msg ana Oi, tudo bem? |
| logout | Encerra a sessão do usuário atual | logout |
| delete <apelido> | Deleta o próprio usuário | delete marcus |
| quit | Fecha o cliente | quit |

---
