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
        if (chdir(newPath.c_str()) < 0) {
            if (errno == ENOENT) {
                return newPath + ": No such file or directory";
            }
            else if (errno == ENOTDIR) {
                return newPath + " is not a directory";
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
    std::string convertPath(const std::string& base) {
        std::string ret = base;
        if (ret != "/" && ret.back() == '/') {
            ret.pop_back();
        }
        return ret;
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
            std::sort(fileList.begin(), fileList.end());
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
    static void cd(const int& fd, const std::string& argu, WorkingDirectory& wd) {
        const std::string nargu = processArgument(argu);
        std::string ret = wd.changeDir(nargu);
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "%s", ret.c_str());
        birdWrite(fd, buffer);
    }
    static void u(const int& fd, const std::string& argu, const WorkingDirectory& wd) {
        const std::string nargu = processArgument(argu);
        char buffer[maxn];
        std::string filename = getFileName(nargu);
        if (wd.getStartupPath().back() == '/') {
            filename = wd.getStartupPath() + "Upload/" + filename;
        }
        else {
            filename = wd.getStartupPath() + "/Upload/" + filename;
        }
        printf("Opening File: %s\n", filename.c_str());
        FILE* fp = fopen(filename.c_str(), "w");
        if (!fp) {
            cleanBuffer(buffer);
            sprintf(buffer, "ERROR_OPEN_FILE");
            birdWrite(fd, buffer);
            return;
        }
        else {
            cleanBuffer(buffer);
            sprintf(buffer, "OK");
            birdWrite(fd, buffer);
        }
        unsigned long fileSize;
        birdRead(fd, buffer);
        sscanf(buffer, "%*s%*s%lu", &fileSize);
        birdReadFile(fd, fp, fileSize);
        fclose(fp);
    }
    static void d(const int& fd, const std::string& argu) {
        const std::string nargu = processArgument(argu);
        char buffer[maxn];
        int chk = isExist(nargu);
        if (chk == -2) {
            cleanBuffer(buffer);
            sprintf(buffer, "UNKNOWN_ERROR");
            birdWrite(fd, buffer);
            return;
        }
        else if (chk == -1) {
            cleanBuffer(buffer);
            sprintf(buffer, "PERMISSION_DENIED");
            birdWrite(fd, buffer);
            return;
        }
        else if (chk == 0) {
            cleanBuffer(buffer);
            sprintf(buffer, "FILE_NOT_EXIST");
            birdWrite(fd, buffer);
            return;
        }
        else if (chk == 2) {
            cleanBuffer(buffer);
            sprintf(buffer, "IS_DIR");
            birdWrite(fd, buffer);
            return;
        }
        else if (chk == 3) {
            cleanBuffer(buffer);
            sprintf(buffer, "NOT_REGULAR_FILE");
            birdWrite(fd, buffer);
            return;
        }
        FILE* fp = fopen(nargu.c_str(), "rb");
        if (!fp) {
            cleanBuffer(buffer);
            sprintf(buffer, "UNKNOWN_ERROR");
            birdWrite(fd, buffer);
            return;
        }
        else {
            cleanBuffer(buffer);
            sprintf(buffer, "FILE_EXISTS");
            birdWrite(fd, buffer);
        }
        cleanBuffer(buffer);
        birdRead(fd, buffer);
        if (std::string(buffer) == "ERROR_OPEN_FILE") {
            return;
        }
        unsigned long fileSize;
        struct stat st;
        stat(nargu.c_str(), &st);
        fileSize = st.st_size;
        cleanBuffer(buffer);
        sprintf(buffer, "filesize = %lu", fileSize);
        birdWrite(fd, buffer);
        birdWriteFile(fd, fp, fileSize);
        fclose(fp);
        return;
    }
    static void undef(const int& fd, const std::string& command) {
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "%s: Command not found", command.c_str());
        birdWrite(fd, buffer);
    }

private:
    // return -2: error, -1: no permission 0: don't exist, 1: regluar file, 2: directory, 3: other
    static int isExist(const std::string& filePath) {
        struct stat st;
        if (lstat(filePath.c_str(), &st) != 0) {
            if (errno == ENOENT) {
                return 0;
            }
            else if (errno == EACCES) {
                return -1;
            }
            else {
                return -2;
            }
        }
        if (S_ISREG(st.st_mode)) {
            return 1;
        }
        else if (S_ISDIR(st.st_mode)) {
            return 2;
        }
        else {
            return 3;
        }
    }
    static std::string getFileName(const std::string& filePath) {
        unsigned long pos = filePath.rfind("/");
        if (pos + 1 >= filePath.length()) {
            return "";
        }
        else if (pos == std::string::npos) {
            return filePath;
        }
        else {
            return filePath.substr(pos + 1);
        }
    }
    static std::string processArgument(const std::string& base) {
        std::string basep = base;
        if (base.front() == '\"' && base.back() == '\"') {
            basep = base.substr(1, base.length() - 2);
            return basep;
        }
        std::string ret = "";
        bool backSlashFlag = false;
        for (unsigned i = 0; i < basep.length(); ++i) {
            if (basep[i] == '\\') {
                backSlashFlag = true;
                continue;
            }
            if (backSlashFlag) {
                backSlashFlag = false;
            }
            ret += basep[i];
        }
        if (backSlashFlag) {
            ret += " ";
        }
        return ret;
    }
    static void cleanBuffer(char *buffer, const int &n = maxn) {
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
    static void birdWriteFile(const int& fd, FILE* fp, const unsigned long& size) {
        char buffer[maxn];
        unsigned byteRead = 0u;
        while (byteRead < size) {
            int n = read(fileno(fp), buffer, maxn);
            if (n < 0) {
                fprintf(stderr, "Error When Reading File\n");
                exit(EXIT_FAILURE);
            }
            byteRead += n;
            int m = birdWrite(fd, buffer, n);
            if (m != n) {
                fprintf(stderr, "Error When Transmitting Data\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    static void birdReadFile(const int& fd, FILE* fp, const unsigned long& size) {
        char buffer[maxn];
        unsigned byteWrite = 0u;
        while (byteWrite < size) {
            int n = read(fd, buffer, maxn);
            if (n < 0) {
                fprintf(stderr, "Error When Receiving Data\n");
                exit(EXIT_FAILURE);
            }
            byteWrite += n;
            int m = write(fileno(fp), buffer, n);
            if (m != n) {
                fprintf(stderr, "Error When Writing File\n");
                exit(EXIT_FAILURE);
            }
        }
    }
};

bool isValidArguments(int argc, char const *argv[]);
int serverInit(const int& port);
void init();
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
    init();
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
            fprintf(stdout, "Connection from %s, port %d\n", clientInfo, clientPort);
            TCPServer(clientfd);
            close(clientfd);
            fprintf(stdout, "Client %s:%d terminated\n", clientInfo, clientPort);
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
    listen(listenId, 256);
    return listenId;
}

void init() {
    if (!WorkingDirectory::isDirExist("./Upload")) {
        mkdir("Upload", 0777);
    }
}

void TCPServer(const int& fd) {
    WorkingDirectory wd;
    while (true) {
        std::string command = ServerFunc::nextCommand(fd);
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
                ServerFunc::u(fd, argu, wd);
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
                ServerFunc::d(fd, argu);
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
