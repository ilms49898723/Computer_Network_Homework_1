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
int serverInit(const int& port);
void tcpServer(int fd);
void sigChld(int signo);

int main(int argc, char const *argv[])
{
    // check arguments
    if (argc != 2 || !isValidArguments(argc, argv)) {
        fprintf(stderr, "usage: %s PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // server initialize
    int port;
    sscanf(argv[1], "%d", &port);
    int listenId = serverInit(port);
    // signal
    signal(SIGCHLD, sigChld);
    // wait for connection, fork()
    while (true) {
        pid_t childPid;
        socklen_t clientLen = sizeof(sockaddr_in);
        sockaddr_in clientAddr;
        int clientfd = accept(listenId, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if ((childPid = fork()) == 0) {
            close(listenId);
            char clientInfo[1024];
            strcpy(clientInfo, inet_ntoa(clientAddr.sin_addr));
            int clientPort = static_cast<int>(clientAddr.sin_port);
            fprintf(stdout, "Connection from %s port %d\n", clientInfo, clientPort);
            tcpServer(clientfd);
            exit(EXIT_SUCCESS);
        }
        else {
            fprintf(stdout, "Start child Process pid = %d\n", static_cast<int>(childPid));
        }
        close(clientfd);
    }
    return 0;
}


bool isValidArguments(int argc, char const *argv[]) {
    if (argc != 2) {
        return false;
    }
    for (const char* ptr = argv[1]; *ptr; ++ptr) {
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

int serverInit(const int& port) {
    int listenId;
    sockaddr_in serverAddr;
    listenId = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);
    if (bind(listenId, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) < 0) {
        fprintf(stderr, "Bind Error\n");
        exit(EXIT_FAILURE);
    }
    listen(listenId, 64);
    return listenId;
}

void tcpServer(int fd) {

}

void sigChld(int signo) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WCONTINUED)) != -1) {
        fprintf(stdout, "Child process %d terminated.\n", static_cast<int>(pid));
    }
}
