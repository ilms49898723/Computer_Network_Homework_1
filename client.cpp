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
#include <limits>

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

class ClientFunc {
public:
    static std::string pwd(const int& fd) {
        char buffer[maxn];
        cleanBuffer(buffer, maxn);
        sprintf(buffer, "pwd");
        birdWrite(fd, buffer, maxn);
        cleanBuffer(buffer, maxn);
        birdRead(fd, buffer, maxn);
        return std::string(buffer);
    }
    static std::string ls(const int& fd) {
        char buffer[maxn];
        cleanBuffer(buffer, maxn);
        sprintf(buffer, "ls");
        birdWrite(fd, buffer, maxn);
        cleanBuffer(buffer, maxn);
        birdRead(fd, buffer, maxn);
        std::string ret;
        int msgLen;
        sscanf(buffer, "%*s%*s%d", &msgLen); // format: length = %d
        int byteRead = 0;
        while ((byteRead += birdRead(fd, buffer, maxn)) < msgLen) {
            ret.append(buffer);
        }
        return ret;
    }
    static std::string c(const int& fd, const std::string& argu) {
        char buffer[maxn];
        cleanBuffer(buffer, maxn);
        sprintf(buffer, "c %s", argu.c_str());
        birdWrite(fd, buffer, maxn);
        cleanBuffer(buffer, maxn);
        birdRead(fd, buffer, maxn);
        return std::string(buffer);
    }
    static std::string u(const int& fd, const std::string& argu) {
        return "";
    }
    static std::string d(const int& fd, const std::string& argu) {
        return "";
    }

private:
    static void cleanBuffer(char* buffer, const int& n) {
        memset(buffer, 0, sizeof(char) * n);
    }
    static int birdRead(const int& fd, char* buffer, const int& n) {
        int byteRead = read(fd, buffer, n);
        if (byteRead < 0) {
            fprintf(stderr, "Read Error\n");
            exit(EXIT_FAILURE);
        }
        return byteRead;
    }
    static int birdWrite(const int& fd, const char* buffer, const int& n) {
        int byteWrite = write(fd, buffer, sizeof(char) * n);
        if (byteWrite < 0) {
            fprintf(stderr, "Write Error\n");
            exit(EXIT_FAILURE);
        }
        return byteWrite;
    }
};

bool isValidArguments(int argc, char const *argv[]);
int clientInit(const char* addr, const int& port);
void closeClient(const int& fd);
void init();
void TCPClient(const int& fd);
void printInfo();
void getCWD(char* path, const int& n);
void trimNewLine(char* str);
std::string toLowerString(const std::string& src);

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
    char userInput[maxn];
    while (scanf("%s ", userInput) == 1) {
        std::string command = toLowerString(userInput);
        if (command == "q") {
            break;
        }
        else if (command == "pwd") {
            printf("%s\n", ClientFunc::pwd(fd).c_str());
        }
        else if (command == "ls") {
            printf("%s\n", ClientFunc::ls(fd).c_str());
        }
        else if (command == "c") {
            char argu[maxn];
            if (!fgets(argu, maxn, stdin)) {
                fprintf(stderr, "Read Command Error\n");
                exit(EXIT_FAILURE);
            }
            trimNewLine(argu);
            printf("%s\n", ClientFunc::c(fd, argu).c_str());
        }
        else if (command == "u") {
            char argu[maxn];
            if (!fgets(argu, maxn, stdin)) {
                fprintf(stderr, "Read Command Error\n");
                exit(EXIT_FAILURE);
            }
            trimNewLine(argu);
            printf("%s\n", ClientFunc::u(fd, argu).c_str());
        }
        else if (command == "d") {
            char argu[maxn];
            if (!fgets(argu, maxn, stdin)) {
                fprintf(stderr, "Read Command Error\n");
                exit(EXIT_FAILURE);
            }
            trimNewLine(argu);
            printf("%s\n", ClientFunc::d(fd, argu).c_str());
        }
        else {
            printf("%s: Command not found\n", command.c_str());
        }
    }
    closeClient(fd);
}

void printInfo() {
    puts("pwd: print current working directory (remote)");
    puts("ls: list information about the files (in current directory) (remote)");
    puts("c <path>: change working directory (remote)");
    puts("u <filepath>: upload file to the working directory (remote)");
    puts("d <filepath>: download file to Download Folder (local)");
    puts("q: quit");
}

void getCWD(char* dst, const int& n) {
    if (!getcwd(dst, n)) {
        fprintf(stderr, "Getcwd Error\n");
        exit(EXIT_FAILURE);
    }
}

void trimNewLine(char* str) {
    if (str[strlen(str) - 1] == '\n') {
        str[strlen(str) - 1] = '\0';
    }
}

std::string toLowerString(const std::string& src) {
    std::string ret;
    for (unsigned i = 0; i < src.length(); ++i) {
        ret += tolower(src[i]);
    }
    return ret;
}
