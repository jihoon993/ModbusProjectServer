#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <WinSock2.h>

#pragma comment(lib, "WS2_32.LIB")

#define SERVER_PORT 50000

void main() {

	WSADATA wsa;
	SOCKET sock, client_sock;
	struct sockaddr_in sock_addr, client_addr;
	int client_addr_size;

	WSAStartup(MAKEWORD(2, 2), &wsa);
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	memset(&sock_addr, 0, sizeof(sock_addr));

	sock_addr.sin_family = AF_INET; sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_addr.sin_port = htons(SERVER_PORT);

	if (bind(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) == SOCKET_ERROR) {
		closesocket(sock);
		return;
	}

	{
		char local_name[1024]; struct hostent* host_ptr = NULL;
		memset(local_name, 0, 1024); gethostname(local_name, 1024);
		host_ptr = gethostbyname(local_name);
		printf("Server IP: %u.%u.%u.%u\n",
			(unsigned char)host_ptr->h_addr_list[0][0],
			(unsigned char)host_ptr->h_addr_list[0][1],
			(unsigned char)host_ptr->h_addr_list[0][2],
			(unsigned char)host_ptr->h_addr_list[0][3]);
		printf("Port Number is %d\n", (int)SERVER_PORT);
		printf("Wait client connection!\n");
	}

	if (listen(sock, 1) == SOCKET_ERROR) {
		closesocket(sock);
		return;
	}

	client_addr_size = sizeof(client_addr); memset(&client_addr, 0, client_addr_size);
	client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_size);

	{
		char recv_buff[1024];
		char send_buff[1024];
		printf("Client connected. Please send the data.\n");
		memset(recv_buff, 0, 1024);
		recv(client_sock, recv_buff, 1024, 0);
		printf("%s\n", recv_buff);
		memset(send_buff, 0, 1024);
		//sprintf(send_buff, "[server]: %s", &recv_buff[10]); 아래것으로 대체.
		sprintf(send_buff, "[server]: %s", &recv_buff[10]);
		printf("%s\n", send_buff);
		send(client_sock, send_buff, strlen(send_buff), 0);
	}

	closesocket(client_sock);
	closesocket(sock);
	WSACleanup();
	system("pause");
}