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
#include <vector>
#include <algorithm>

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
    std::string changeDir(const std::string& newPath) {
        std::string np = processPath(newPath);
        if (chdir(np.c_str()) < 0) {
            if (errno == ENOENT) {
                fprintf(stderr, "%s: No such file or directory\n", np.c_str());
                return np + ": No such file or directory";
            }
            else if (errno == ENOTDIR) {
                fprintf(stderr, "%s is not a directory\n", np.c_str());
                return np + " is not a directory";
            }
        }
        updatePath();
        return "";
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
    std::string processPath(const std::string& base) {
        std::string basep = base;
        if (base.front() == '\"' && base.back() == '\"') {
            basep = base.substr(1, base.length() - 2);
        }
        std::string ret = "";
        for (unsigned i = 0; i < basep.length(); ++i) {
            if (basep[i] == '\\') {
                continue;
            }
            ret += basep[i];
        }
        return ret;
    }
    std::string convertPath(const std::string& base) {
        std::string ret = base;
        if (ret.back() == '/') {
            ret.pop_back();
        }
        return ret;
    }
};

class ServerFunc {
public:
    static std::string nextCommand(const int& fd) {
        char buffer[maxn];
        clearBuffer(buffer);
        birdRead(fd, buffer);
        return std::string(buffer);
    }
    static void pwd(const int& fd, const WorkingDirectory& wd) {
        char buffer[maxn];
        clearBuffer(buffer);
        sprintf(buffer, "%s", wd.getPath().c_str());
        birdWrite(fd, buffer);
    }
    static void ls(const int& fd, const WorkingDirectory& wd) {
        DIR* dir = opendir(wd.getPath().c_str());
        if (!dir) {
            char buffer[maxn];
            clearBuffer(buffer);
            sprintf(buffer, "Open directory Error!");
            birdWrite(fd, buffer);
        }
        else {
            dirent *dirst;
            std::vector<std::string> fileList;
            while ((dirst = readdir(dir))) {
                std::string name(dirst->d_name);
                if (dirst->d_type == DT_DIR) {
                    name += "/";
                }
                fileList.push_back(name);
            }
            std::sort(fileList.begin(), fileList.end());
            char buffer[maxn];
            clearBuffer(buffer);
            sprintf(buffer, "length = %d", static_cast<int>(fileList.size()));
            birdWrite(fd, buffer);
            for (unsigned i = 0; i < fileList.size(); ++i) {
                clearBuffer(buffer);
                sprintf(buffer, "%s", fileList[i].c_str());
                birdWrite(fd, buffer);
            }
            closedir(dir);
        }
    }
    static void cd(const int& fd, const std::string& argu, WorkingDirectory& wd) {
        std::string ret = wd.changeDir(argu);
        char buffer[maxn];
        clearBuffer(buffer);
        sprintf(buffer, "%s", ret.c_str());
        birdWrite(fd, buffer);
    }
    static void u(const int& fd) {
    }
    static void d(const int& fd) {
    }
    static void undef(const int& fd, const std::string& command) {
        char buffer[maxn];
        clearBuffer(buffer);
        sprintf(buffer, "%s: Command not found", command.c_str());
        birdWrite(fd, buffer);
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
int serverInit(const int& port);
void TCPServer(const int& fd);
void trimNewLine(char* str);
std::string trimSpaceLE(const std::string& str);
std::string toLowerString(const std::string& src);
void sigChld(int signo);

int main(int argc, char const *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s SERVER_ADDRESS PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (!isValidArguments(argc, argv)) {
        fprintf(stderr, "Invalid Arguments\n");
        exit(EXIT_FAILURE);
    }
    // server initialize
    int port;
    sscanf(argv[1], "%d", &port);
    int listenId = serverInit(port);
    // signal
    signal(SIGCHLD, sigChld);
    // wait for connection, then fork for per client
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
            TCPServer(clientfd);
            close(clientfd);
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
            fprintf(stderr, "%s is not a number\n", argv[1]);
            return false;
        }
    }
    return true;
}

int serverInit(const int& port) {
    int listenId;
    sockaddr_in serverAddr;
    if ((listenId = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket Error\n");
        exit(EXIT_FAILURE);
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);
    if (bind(listenId, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(sockaddr_in)) < 0) {
        fprintf(stderr, "Bind Error\n");
        exit(EXIT_FAILURE);
    }
    listen(listenId, 5);
    return listenId;
}

void TCPServer(const int& fd) {
    WorkingDirectory wd;
    while (true) {
        std::string command = ServerFunc::nextCommand(fd);
        printf("get command %s\n", command.c_str());
        if (command == "q") {
            break;
        }
        else if (command == "pwd") {
            ServerFunc::pwd(fd, wd);
        }
        else if (command == "ls") {
            ServerFunc::ls(fd, wd);
        }
        else if (command.find("cd") == 0) {
            char op[maxn];
            sscanf(command.c_str(), "%s", op);
            if (strcmp(op, "cd")) {
                ServerFunc::undef(fd, op);
            }
            else {
                std::string argu(command.c_str() + 2);
                argu = trimSpaceLE(argu);
                ServerFunc::cd(fd, argu, wd);
            }
        }
        else if (command.find("u") == 0) {
            char op[maxn];
            sscanf(command.c_str(), "%s", op);
            if (strcmp(op, "u")) {
                ServerFunc::undef(fd, op);
            }
            else {
                std::string argu(command.c_str() + 1);
                argu = trimSpaceLE(argu);
                ServerFunc::u(fd);
            }
        }
        else if (command.find("d") == 0) {
            char op[maxn];
            sscanf(command.c_str(), "%s", op);
            if (strcmp(op, "d")) {
                ServerFunc::undef(fd, op);
            }
            else {
                std::string argu(command.c_str() + 1);
                argu = trimSpaceLE(argu);
                ServerFunc::d(fd);
            }
        }
        else {
            ServerFunc::undef(fd, command);
        }
    }
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

void sigChld(int signo) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WCONTINUED)) != -1) {
        fprintf(stdout, "Child process %d terminated.\n", static_cast<int>(pid));
    }
}
