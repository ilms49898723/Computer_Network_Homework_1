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
        struct stat st;
        if (lstat(path.c_str(), &st) != 0) {
            if (errno == ENOENT) {
                return false;
            }
            else if (errno == EACCES) {
                return false;
            }
            else {
                return false;
            }
        }
        if (S_ISREG(st.st_mode)) {
            return false;
        }
        else if (S_ISDIR(st.st_mode)) {
            return true;
        }
        else {
            return false;
        }
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
            else if (errno == EACCES) {
                return newPath + ": Permission denied";
            }
            else {
                return newPath + ": Unexpected error";
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
            fprintf(stderr, "getcwd Error\nProgram Terminated!\n");
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

class ClientFunc {
public:
    static void q(const int& fd) {
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "q");
        birdWrite(fd, buffer);
    }
    static std::string pwd(const int& fd) {
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "pwd");
        birdWrite(fd, buffer);
        cleanBuffer(buffer);
        birdRead(fd, buffer);
        return std::string(buffer);
    }
    static std::string ls(const int& fd) {
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "ls");
        birdWrite(fd, buffer);
        cleanBuffer(buffer);
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
    static void cd(const int& fd, const std::string& argu) {
        const std::string nargu = argu;
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "cd %s", nargu.c_str());
        birdWrite(fd, buffer);
        cleanBuffer(buffer);
        birdRead(fd, buffer);
        if (std::string(buffer) != "") {
            printf("%s\n", buffer);
        }
    }
    static bool u(const int& fd, const std::string& argu) {
        const std::string nargu = processArgument(argu);
        int chk = isExist(nargu);
        if (chk == -2) {
            fprintf(stderr, "%s: Unexpected Error\n", nargu.c_str());
            return false;
        }
        else if (chk == -1) {
            fprintf(stderr, "%s: Permission denied\n", nargu.c_str());
            return false;
        }
        else if (chk == 0) {
            fprintf(stderr, "%s: No such file or directory\n", nargu.c_str());
            return false;
        }
        else if (chk == 2) {
            fprintf(stderr, "%s is a directory\n", nargu.c_str());
            return false;
        }
        else if (chk == 3) {
            fprintf(stderr, "%s is not a regular file\n", nargu.c_str());
            return false;
        }
        FILE* fp = fopen(nargu.c_str(), "rb");
        if (!fp) {
            fprintf(stderr, "%s: Unexpected Error\n", nargu.c_str());
            return false;
        }
        unsigned long fileSize;
        struct stat st;
        stat(nargu.c_str(), &st);
        fileSize = st.st_size;
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "u %s", argu.c_str());
        birdWrite(fd, buffer);
        cleanBuffer(buffer);
        birdRead(fd, buffer);
        if (std::string(buffer) == "ERROR_OPEN_FILE") {
            fprintf(stderr, "Cannot open file \"%s\" on Remote Server\n", getFileName(argu.c_str()).c_str());
            fclose(fp);
            return false;
        }
        printf("Upload File \"%s\"\n", getFileName(nargu).c_str());
        cleanBuffer(buffer);
        sprintf(buffer, "filesize = %lu", fileSize);
        birdWrite(fd, buffer);
        printf("File size: %lu bytes\n", fileSize);
        birdWriteFile(fd, fp, fileSize);
        printf("Upload File \"%s\" Completed\n", getFileName(nargu).c_str());
        fclose(fp);
        return true;
    }
    static bool d(const int& fd, const std::string& argu, const WorkingDirectory& wd) {
        const std::string nargu = processArgument(argu);
        char buffer[maxn];
        cleanBuffer(buffer);
        sprintf(buffer, "d %s", argu.c_str());
        birdWrite(fd, buffer);
        cleanBuffer(buffer);
        birdRead(fd, buffer);
        if (std::string(buffer) == "UNEXPECTED_ERROR") {
            fprintf(stderr, "Unexpected Error\n");
            return false;
        }
        else if (std::string(buffer) == "PERMISSION_DENIED") {
            fprintf(stderr, "%s: Permission denied\n", nargu.c_str());
            return false;
        }
        else if (std::string(buffer) == "FILE_NOT_EXIST") {
            fprintf(stderr, "%s: No such file or directory\n", nargu.c_str());
            return false;
        }
        else if (std::string(buffer) == "IS_DIR") {
            fprintf(stderr, "%s is a directory\n", nargu.c_str());
            return false;
        }
        else if (std::string(buffer) == "NOT_REGULAR_FILE") {
            fprintf(stderr, "%s is not a regular file\n", nargu.c_str());
            return false;
        }
        std::string filename = getFileName(nargu);
        if (wd.getStartupPath().back() == '/') {
            filename = wd.getStartupPath() + "Download/" + filename;
        }
        else {
            filename = wd.getStartupPath() + "/Download/" + filename;
        }
        FILE* fp = fopen(filename.c_str(), "wb");
        if (!fp) {
            fprintf(stderr, "%s: File Open Error\n", filename.c_str());
            cleanBuffer(buffer);
            sprintf(buffer, "ERROR_OPEN_FILE");
            birdWrite(fd, buffer);
            return false;
        }
        cleanBuffer(buffer);
        sprintf(buffer, "OK");
        birdWrite(fd, buffer);
        printf("Download File \"%s\"\n", getFileName(nargu).c_str());
        unsigned long fileSize;
        birdRead(fd, buffer);
        sscanf(buffer, "%*s%*s%lu", &fileSize);
        printf("File size: %lu bytes\n", fileSize);
        birdReadFile(fd, fp, fileSize);
        printf("Download File \"%s\" Completed\n", getFileName(nargu).c_str());
        fclose(fp);
        return true;
    }

private:
    // return -2: error, -1: no permission 0: don't exist, 1: regular file, 2: directory, 3: other
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
            fprintf(stderr, "read() Error\n");
            exit(EXIT_FAILURE);
        }
        return byteRead;
    }
    static int birdWrite(const int& fd, const char* buffer, const int& n = maxn) {
        int byteWrite = write(fd, buffer, sizeof(char) * n);
        if (byteWrite < 0) {
            fprintf(stderr, "write() Error\n");
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
                fprintf(stderr, "Error When Writing to File\n");
                exit(EXIT_FAILURE);
            }
        }
    }
};

bool isValidArguments(int argc, char const *argv[]);
bool isAllSpace(const char* str);
int clientInit(const char* addr, const int& port);
void closeClient(const int& fd);
void init();
void TCPClient(const int& fd, const char* host);
void printInfo();
void trimNewLine(char* str);
std::string toLowerString(const std::string& src);
std::string trimSpaceLE(const std::string& str);
std::string nextArgument(std::string& base);

int main(int argc, char const *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <server address> <port>\n", argv[0]);
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
    printf("\n\nNetwork Programming Homework 1\n\nConnected to %s:%s\n", argv[1], argv[2]);
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
        fprintf(stderr, "%s: Inet_pton error\n", argv[1]);
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

bool isAllSpace(const char* str) {
    for (int i = 0; str[i]; ++i) {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n') {
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
        fprintf(stderr, "%s: Inet_pton Error\n", addr);
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
        if (mkdir("Download", 0777) < 0) {
            fprintf(stderr, "Error: mkdir %s: %s\n", "Download", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

void TCPClient(const int& fd, const char* host) {
    std::string serverPath = ClientFunc::pwd(fd);
    WorkingDirectory wd;
    printInfo();
    while (true) {
        printf("%s:%s$ ", host, serverPath.c_str());
        char userInputCStr[maxn];
        if (!fgets(userInputCStr, maxn, stdin)) {
            break;
        }
        if (!strcmp(userInputCStr, "\n") || isAllSpace(userInputCStr)) {
            continue;
        }
        std::string userInput = userInputCStr;
        std::string command = nextArgument(userInput);
        if (command == "help") {
            std::string argu = nextArgument(userInput);
            if (argu != "") {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: help\n");
                    printf("Print help information.\n");
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                printInfo();
            }
        }
        else if (command == "lpwd") {
            std::string argu = nextArgument(userInput);
            if (argu != "") {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: lpwd\n");
                    printf("Print local current working directory.\n");
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                printf("%s\n", wd.getPath().c_str());
            }
        }
        else if (command == "lls") {
            std::string argu = nextArgument(userInput);
            if (argu != "") {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: lpwd\n");
                    printf("List information about the files in the local current directory.\n");
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                DIR* dir = opendir(wd.getPath().c_str());
                if (!dir) {
                    fprintf(stderr, "%s: Cannot open the directory\n", wd.getPath().c_str());
                    continue;
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
                    for (const auto& i : fileList) {
                        printf("%s\n", i.c_str());
                    }
                    closedir(dir);
                }
            }
        }
        else if (command == "exit") {
            std::string argu = nextArgument(userInput);
            if (argu != "") {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: exit\n");
                    printf("Terminate the connection.\n");
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                ClientFunc::q(fd);
                printf("\nConnection Terminated\n\n");
                break;
            }
        }
        else if (command == "pwd") {
            std::string argu = nextArgument(userInput);
            if (argu != "") {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: pwd\n");
                    printf("Print current working directory on Remote Server.\n");
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                printf("%s\n", ClientFunc::pwd(fd).c_str());
            }
        }
        else if (command == "ls") {
            std::string argu = nextArgument(userInput);
            if (argu != "") {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: ls\n");
                    printf("List information about the files in the current directory on Remote Server.\n");
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                printf("%s\n", ClientFunc::ls(fd).c_str());
            }
        }
        else if (command == "cd") {
            std::string argu = nextArgument(userInput);
            if (argu == "" || argu[0] == '-') {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: cd <path>\n");
                    printf("Change working directory to <path> on Remote Server.\n");
                    printf("ex:\n");
                    printf("    cd \"Network Programming\"\n");
                    printf("    cd ../Download\n");
                }
                else if (argu == "") {
                    continue;
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                ClientFunc::cd(fd, argu);
                serverPath = ClientFunc::pwd(fd);
            }
        }
        else if (command == "u") {
            std::string argu = nextArgument(userInput);
            if (argu == "" || argu[0] == '-') {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: u <file>\n");
                    printf("Upload file(path related to local working directory) to Remote Server.\n");
                    printf("ex:\n");
                    printf("    u hw1.tar\n");
                    printf("    u ../client.cpp\n");
                }
                else if (argu == "") {
                    printf("usage: u <file>\nu --help for more information\n");
                    continue;
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                ClientFunc::u(fd, argu);
            }
        }
        else if (command == "d") {
            std::string argu = nextArgument(userInput);
            if (argu == "" || argu[0] == '-') {
                if (argu == "-h" || argu == "-help" || argu == "--help") {
                    printf("usage: d <file>\n");
                    printf("Download file(path related to working directory on server) to Download.\n");
                    printf("ex:\n");
                    printf("    d hw1.tar\n");
                    printf("    d ../server.cpp\n");
                }
                else if (argu == "") {
                    printf("usage: d <file>\nd --help for more information\n");
                    continue;
                }
                else {
                    fprintf(stderr, "Unrecognized Argument %s\n", argu.c_str());
                }
            }
            else {
                ClientFunc::d(fd, argu, wd);
            }
        }
        else {
            fprintf(stderr, "%s: Command not found\n", command.c_str());
        }
    }
}

void printInfo() {
    puts("");
    puts("Avaliable Commands:");
    puts("");
    puts("    lpwd: print current working directory(local)");
    puts("    lls: list information about the files in current directory(local)");
    puts("");
    puts("    pwd: print current working directory on remote server");
    puts("    ls: list information about the files in current directory on remote server");
    puts("    cd <path>: change working directory on remote server");
    puts("    u <file>: upload file to remote server");
    puts("    d <file>: download file from server");
    puts("    exit: terminate connection");
    puts("");
    puts("    help: print information");
    puts("");
    puts("use <command> -h or <command> --help for more information");
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

std::string nextArgument(std::string& base) {
    std::string ret = "";
    while (base.length() > 0) {
        char arguCStr[maxn];
        memset(arguCStr, 0, sizeof(char) * maxn);
        sscanf(base.c_str(), "%s", arguCStr);
        if (strlen(arguCStr) == 0) {
            base = "";
            break;
        }
        std::string argu(arguCStr);
        if (argu.front() == '\"') {
            unsigned startp = base.find("\"");
            unsigned endp = startp + 1;
            while (endp < base.length() && base[endp] != '\"') {
                ++endp;
            }
            if (endp < base.length() && base[endp] == '\"') {
                ++endp;
            }
            ret.append(base.substr(startp, endp - startp));
            if (endp >= base.length()) {
                base = "";
            }
            else {
                base = base.substr(endp);
            }
            break;
        }
        else {
            ret.append(argu);
            base = base.substr(base.find(argu) + argu.length());
            if (ret.back() == '\\') {
                ret += " ";
            }
            else {
                break;
            }
        }
    }
    ret = trimSpaceLE(ret);
    return ret;
}
