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
                    if (pos == static_cast<int>(std::string::npos)) {
                        continue;
                    }
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

class ServerFunc {
public:
    static std::string nextCommand(const int& fd) {
        char buffer[maxn];
        cleanBuffer(buffer);
        birdRead(fd, buffer);
        return std::string(buffer);
    }
    static void pwd(const int& fd, const WorkingDirectory& wd) {
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "%s", wd.getPath().c_str());
        birdWrite(fd, buffer);
    }
    static void ls(const int& fd, const WorkingDirectory& wd) {
        DIR* dir = opendir(wd.getPath().c_str());
        if (!dir) {
            char buffer[maxn];
            cleanBuffer(buffer);
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
            char buffer[maxn];
            cleanBuffer(buffer);
            sprintf(buffer, "length = %d", static_cast<int>(fileList.size()));
            birdWrite(fd, buffer);
            for (unsigned i = 0; i < fileList.size(); ++i) {
                cleanBuffer(buffer);
                sprintf(buffer, "%s", fileList[i].c_str());
                birdWrite(fd, buffer);
            }
            closedir(dir);
        }
    }
    static void c(const int& fd, const std::string& argu) {
    }
    static void u(const int& fd, const std::string& argu) {
    }
    static void d(const int& fd, const std::string& argu) {
    }

private:
    static void cleanBuffer(char* buffer, const int& n = maxn) {
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
    listen(listenId, 64);
    return listenId;
}

void TCPServer(const int& fd) {

}

void sigChld(int signo) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WCONTINUED)) != -1) {
        fprintf(stdout, "Child process %d terminated.\n", static_cast<int>(pid));
    }
}
