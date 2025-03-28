#include <iostream>
#include <netinet/in.h>
#include <sys/socket>
#include <unistd.h>


using namespace std;


int main() {

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address));

    listen(server_socket, 5);

    int client_socket = accept(server_socket, nullptr, nullptr);

    char buffer[1024] = {0};
    recv(client_socket, buffer, sizeof(buffer), 0);

    cout << "Message from client : " << buffer << "\n";

    close(server_socket);

}
