#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cctype>

bool isValidArguments(int argc, char const *argv[]);
int birdRead(const int& fd, char* buffer, const int& n);
int birdWrite(const int& fd, const char* buffer, const int& n);
int clientInit(const char* addr, const int& port);
void tcpClient(const int& fd);

int main(int argc, char const *argv[])
{
    if (!isValidArguments(argc, argv)) {
        fprintf(stderr, "usage: %s SERVER_ADDRESS PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port;
    sscanf(argv[2], "%d", &port);
    int sockfd = clientInit(argv[1], port);
    return 0;
}

bool isValidArguments(int argc, char const *argv[]) {
    if (argc != 3) {
        return false;
    }
    sockaddr_in tmp;
    memset(&tmp, 0, sizeof(tmp));
    if (inet_pton(AF_INET, argv[1], &tmp.sin_addr) <= 0) {
        fprintf(stderr, "Inet_pton error for %s\n", argv[1]);
        return false;
    }
    for (const char* ptr = argv[2]; *ptr; ++ptr) {
        if (!isdigit(*ptr)) {
            return false;
        }
    }
    return true;
}

int birdRead(const int& fd, char* buffer, const int& n) {
    int byteRead = read(fd, buffer, n);
    if (byteRead < 0) {
        fprintf(stderr, "Read Error\n");
    }
    return byteRead;
}

int birdWrite(const int& fd, const char* buffer, const int& n) {
    int byteWrite = write(fd, buffer, sizeof(char) * n);
    if (byteWrite < 0) {
        fprintf(stderr, "Write Error\n");
    }
    return byteWrite;
}

int clientInit(const char* addr, const int& port) {
    int sockfd;
    sockaddr_in serverAddr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket Error\n");
        exit(EXIT_FAILURE);
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &serverAddr.sin_addr) <= 0) {
        fprintf(stderr, "Inet_pton error for %s\n", addr);
        exit(EXIT_FAILURE);
    }
    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) >= 0) {
        fprintf(stderr, "Connect Error\n");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void tcpClient(const int& fd) {

}
