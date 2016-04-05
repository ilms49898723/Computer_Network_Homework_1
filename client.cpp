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
        updatePath();
        startupPath = path;
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
        if (chdir(newPath.c_str()) < 0) {
            if (errno == ENOENT) {
                fprintf(stderr, "%s: No such file or directory\n", newPath.c_str());
            }
            else if (errno == ENOTDIR) {
                fprintf(stderr, "%s is not a directory\n", newPath.c_str());
            }
        }
        updatePath();
    }

private:
    std::string path;
    std::string startupPath;

private:
    void updatePath() {
        char buffer[maxn];
        if (!getcwd(buffer, maxn)) {
            fprintf(stderr, "getcwd Error\n");
            exit(EXIT_FAILURE);
        }
        path = buffer;
    }
    std::string convertPath(const std::string& base) {
        std::string ret = base;
        if (ret.back() == '/') {
            ret.pop_back();
        }
        return ret;
    }
};

class ClientFunc {
public:
    static void q(const int& fd) {
        char buffer[maxn];
        clearBuffer(buffer);
        sprintf(buffer, "q");
        birdWrite(fd, buffer);
    }
    static std::string pwd(const int& fd) {
        char buffer[maxn];
        clearBuffer(buffer);
        sprintf(buffer, "pwd");
        birdWrite(fd, buffer);
        clearBuffer(buffer);
        birdRead(fd, buffer);
        return std::string(buffer);
    }
    static std::string ls(const int& fd) {
        char buffer[maxn];
        clearBuffer(buffer);
        sprintf(buffer, "ls");
        birdWrite(fd, buffer);
        clearBuffer(buffer);
        birdRead(fd, buffer);
        std::string ret = "";
        int msgLen;
        sscanf(buffer, "%*s%*s%d", &msgLen); // format: length = %d
        for (int i = 0; i < msgLen; ++i) {
            birdRead(fd, buffer);
            ret += std::string(buffer) + "\n";
        }
        if (ret.back() == '\n') {
            ret.pop_back();
        }
        return ret;
    }
    static std::string cd(const int& fd, const std::string& argu) {
        char buffer[maxn];
        clearBuffer(buffer);
        sprintf(buffer, "cd %s", argu.c_str());
        birdWrite(fd, buffer);
        clearBuffer(buffer);
        birdRead(fd, buffer);
        return std::string(buffer);
    }
    static std::string u(const int& fd, const std::string& argu) {
        return "";
    }
    static std::string d(const int& fd, const std::string& argu) {
        return "";
    }

private:
    static void clearBuffer(char* buffer, const int& n = maxn) {
        memset(buffer, 0, sizeof(char) * n);
    }
    static int birdRead(const int& fd, char* buffer, const int& n = maxn) {
        int byteRead = read(fd, buffer, n);
        if (byteRead < 0) {
            fprintf(stderr, "Read Error\n");
            exit(EXIT_FAILURE);
        }
        return byteRead;
    }
    static int birdWrite(const int& fd, const char* buffer, const int& n = maxn) {
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
void TCPClient(const int& fd, const char* host);
void printInfo();
void trimNewLine(char* str);
std::string toLowerString(const std::string& src);
std::string trimSpaceLE(const std::string& str);
std::string nextArgument(FILE* fin);

int main(int argc, char const *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s SERVER_ADDRESS PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (!isValidArguments(argc, argv)) {
        fprintf(stderr, "Invalid Arguments\n");
        exit(EXIT_FAILURE);
    }
    init();
    int port;
    sscanf(argv[2], "%d", &port);
    int sockfd = clientInit(argv[1], port);
    TCPClient(sockfd, argv[1]);
    closeClient(sockfd);
    return 0;
}

bool isValidArguments(int argc, char const *argv[]) {
    if (argc != 3) {
        return false;
    }
    sockaddr_in tmp;
    memset(&tmp, 0, sizeof(tmp));
    if (inet_pton(AF_INET, argv[1], &tmp.sin_addr) <= 0) {
        fprintf(stderr, "Inet_pton error for \"%s\"\n", argv[1]);
        return false;
    }
    for (const char* ptr = argv[2]; *ptr; ++ptr) {
        if (!isdigit(*ptr)) {
            fprintf(stderr, "%s is not a number\n", argv[2]);
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
    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) < 0) {
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

void TCPClient(const int& fd, const char* host) {
    std::string serverPath = ClientFunc::pwd(fd);
    WorkingDirectory wd;
    printInfo();
    char userInput[maxn];
    while (true) {
        printf("%s:%s$ ", host, serverPath.c_str());
        if (scanf("%s", userInput) != 1) {
            break;
        }
        std::string command = toLowerString(userInput);
        if (command == "help") {
            printInfo();
        }
        else if (command == "exit") {
            ClientFunc::q(fd);
            break;
        }
        else if (command == "pwd") {
            printf("%s\n", ClientFunc::pwd(fd).c_str());
        }
        else if (command == "ls") {
            printf("%s\n", ClientFunc::ls(fd).c_str());
        }
        else if (command == "cd") {
            std::string argu = nextArgument(stdin);
            serverPath = ClientFunc::cd(fd, argu);
        }
        else if (command == "u") {
            std::string argu = nextArgument(stdin);
            printf("%s\n", ClientFunc::u(fd, argu).c_str());
        }
        else if (command == "d") {
            std::string argu = nextArgument(stdin);
            printf("%s\n", ClientFunc::d(fd, argu).c_str());
        }
        else {
            printf("%s: Command not found\n", command.c_str());
        }
    }
}

void printInfo() {
    puts("");
    puts("pwd: print current working directory (remote)");
    puts("ls: list information about the files (in current directory) (remote)");
    puts("cd <path>: change working directory (remote)");
    puts("u <filepath>: upload file to the working directory (remote)");
    puts("d <filepath>: download file to Download Folder (local)");
    puts("exit: quit");
    puts("help: information");
    puts("");
}

void trimNewLine(char* str) {
    if (str[strlen(str) - 1] == '\n') {
        str[strlen(str) - 1] = '\0';
    }
}

std::string trimSpaceLE(const std::string& str) {
    int len = str.length();
    int startp = 0, endp = str.length() - 1;
    while (startp < len && str.at(startp) == ' ') {
        ++startp;
    }
    while (endp >= 0 && str.at(endp) == ' ') {
        --endp;
    }
    if (startp > endp) {
        return "";
    }
    else {
        return str.substr(startp, endp - startp + 1);
    }
}

std::string toLowerString(const std::string& src) {
    std::string ret;
    for (unsigned i = 0; i < src.length(); ++i) {
        ret += tolower(src[i]);
    }
    return ret;
}

std::string nextArgument(FILE* fin) {
    char argu[maxn];
    if (!fgets(argu, maxn, fin)) {
        fprintf(stderr, "Read Command Error\n");
        exit(EXIT_FAILURE);
    }
    trimNewLine(argu);
    std::string arguStr(argu);
    arguStr = trimSpaceLE(arguStr);
    return arguStr;
}
