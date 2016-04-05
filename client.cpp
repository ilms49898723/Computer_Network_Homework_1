#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
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
#include <string>

constexpr int maxn = 2048;

class WorkingDirectory {
public:
    static bool isDirExist(const std::string& path) {
        DIR* dir = opendir(path.c_str());
        if (dir) {
            closedir(dir);
            return true;
        }
        return false;
    }

public:
    WorkingDirectory() {
        path = "";
    }
    virtual ~WorkingDirectory() {

    }
    void init(const std::string& initPath) {
        path = convertPath(initPath);
        startupPath = convertPath(initPath);
    }
    std::string getPath() const {
        return path;
    }
    std::string getStartupPath() const {
        return startupPath;
    }
    void changeDir(const std::string& newPath) {
        if (newPath.length() <= 0u) {
            return;
        }
        if (newPath[0] == '/') {
            if (WorkingDirectory::isDirExist(newPath)) {
                path = convertPath(newPath);
            }
            else {
                fprintf(stderr, "%s: No such file or directory\n", newPath.c_str());
            }
        }
        else {
            std::string tmpNewPath = path;
            std::string tmpPath = newPath;
            std::string subPath;
            while ((subPath = nextSubDir(tmpPath)) != "") {
                if (subPath == "." || subPath == "./") {
                    continue;
                }
                else if (subPath == ".." || subPath == "../") {
                    int pos = subPath.rfind("/");
                    tmpNewPath = tmpNewPath.substr(0, pos);
                }
                else {
                    pathCat(tmpNewPath, subPath);
                }
            }
            if (WorkingDirectory::isDirExist(tmpNewPath)) {
                path = tmpNewPath;
            }
            else {
                fprintf(stderr, "%s: No such file or directory\n", tmpNewPath.c_str());
            }
        }
    }

private:
    std::string path;
    std::string startupPath;

private:
    std::string nextSubDir(std::string& path) {
        if (path.find("/") == std::string::npos) {
            return "";
        }
        int pos = path.find("/");
        std::string ret = path.substr(0, pos);
        path = path.substr(pos + 1);
        return ret;
    }
    std::string convertPath(const std::string& base) {
        std::string ret = base;
        if (ret.back() == '/') {
            ret.pop_back();
        }
        return ret;
    }
    void pathCat(std::string& base, const std::string& append) {
        if (base.back() != '/') {
            base.append("/");
        }
        base += append;
        if (base.back() == '/') {
            base.pop_back();
        }
    }
};

bool isValidArguments(int argc, char const *argv[]);
int birdRead(const int& fd, char* buffer, const int& n);
int birdWrite(const int& fd, const char* buffer, const int& n);
int clientInit(const char* addr, const int& port);
void closeClient(const int& fd);
void init();
void TCPClient(const int& fd);
void getCWD(char* path, const int& n);

int main(int argc, char const *argv[])
{
    if (!isValidArguments(argc, argv)) {
        fprintf(stderr, "usage: %s SERVER_ADDRESS PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    init();
    int port;
    sscanf(argv[2], "%d", &port);
    int sockfd = clientInit(argv[1], port);
    TCPClient(sockfd);
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
        fprintf(stderr, "Inet_pton Error for %s\n", addr);
        exit(EXIT_FAILURE);
    }
    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) >= 0) {
        fprintf(stderr, "Connect Error\n");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}


void closeClient(const int& fd) {
    close(fd);
}

void init() {
    if (!WorkingDirectory::isDirExist("./Download")) {
        mkdir("Download", 0777);
    }
}

void TCPClient(const int& fd) {
    WorkingDirectory wd;
    char cwd[maxn];
    getCWD(cwd, maxn);
    wd.init(cwd);
}

void getCWD(char* dst, const int& n) {
    if (!getcwd(dst, n)) {
        fprintf(stderr, "Getcwd Error\n");
        exit(EXIT_FAILURE);
    }
}
