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

bool isValidArguments(int argc, char const *argv[]);

int main(int argc, char const *argv[])
{
    if (argc != 2 || !isValidArguments(argc, argv)) {
        fprintf(stderr, "usage: %s PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    return 0;
}


bool isValidArguments(int argc, char const *argv[]) {
    if (argc != 2) {
        return false;
    }
    int port;
    if (sscanf(argv[1], "%d", &port) != 1) {
        return false;
    }
    return true;
}
