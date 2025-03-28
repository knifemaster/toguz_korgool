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



}
