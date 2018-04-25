#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include "dir.h"
#include <stdbool.h>
#include <dirent.h>
#include "CSftp.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>

bool loggedIn = false;
bool inPasvMode = false;
int pasvSocket;

char ftpServerDir[1024] = {0};
char currentPath[1024] = {0};
char currentType[1] = {'A'};
char currentStructure[1] = {'F'};
char currentMode[1] = {'S'};

void main(int argc, char **argv) {
    int serverSocket, newSocket;
    struct sockaddr_in address;
    int addressLength = sizeof(address);

    // Get current directory
    getcwd(ftpServerDir, sizeof(ftpServerDir));
    getcwd(currentPath, sizeof(currentPath));

    // Socket creation
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set the attributes (IP type, port, and make it be the local address) of the address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((uint16_t) atoi(argv[1]));

    // Bind to the socket
    if (bind(serverSocket, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Start to listen
        if (listen(serverSocket, 3) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        // Accept connection request
        if ((newSocket = accept(serverSocket, (struct sockaddr *) &address, (socklen_t *) &addressLength)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Responds with a 220 to initiate a login sequence.
        if (send(newSocket, "220 CSftp server ready.\r\n", strlen("220 CSftp server ready.\r\n"), 0) < 0) {
            // send error message
        }

        // Read and execute command from ftp client.
        bool connected = true;
        while (connected) {
            char buffer[1024] = {0};
            if (read(newSocket, buffer, 1024) > 0) {
                connected = runCommand(newSocket, buffer);
            }
        }
    }
}

/**
 * Runs an FTP command.
 * @param cmd The message from the ftp client. Requires cmd to be a valid FTP command.
 */
bool runCommand(int socket, char cmd[]) {
    // Set command to uppercase
    int i = 0;
    while (cmd[i] != ' ' && cmd[i] != '\r' && cmd[i] != '\n') {
        if (cmd[i] != '\r' && cmd[i] != '\n') {
            cmd[i] = toupper(cmd[i]);
        }
        i++;
    }

    // Determine type of command.
    bool isUserCmd = strncmp(cmd, "USER", 4) == 0;
    bool isQuitCmd = strncmp(cmd, "QUIT", 4) == 0;
    bool isCwdCmd = strncmp(cmd, "CWD", 3) == 0;
    bool isCdupCmd = strncmp(cmd, "CDUP", 4) == 0;
    bool isTypeCmd = strncmp(cmd, "TYPE", 4) == 0;
    bool isModeCmd = strncmp(cmd, "MODE", 4) == 0;
    bool isStruCmd = strncmp(cmd, "STRU", 4) == 0;
    bool isRetrCmd = strncmp(cmd, "RETR", 4) == 0;
    bool isPasvCmd = strncmp(cmd, "PASV", 4) == 0;
    bool isNlstCmd = strncmp(cmd, "NLST", 4) == 0;
    bool isNewLine = strncmp(cmd, "\r\n", 2) == 0;

    // Clean up end of line characters
    for (i = 0; i < strlen(cmd); i++) {
        if (cmd[i] == '\r' || cmd[i] == '\n') {
            cmd[i] = '\0';
        }
    }

    // Check for correct number of arguments in cmd.
    bool invalidArgs = false;

    if (isUserCmd || isTypeCmd || isModeCmd || isStruCmd || isRetrCmd || isCwdCmd) {
        if (!checkArguments(&cmd[4], 1)) {
            invalidArgs = true;
        }
    }
    if (isQuitCmd || isCdupCmd || isNlstCmd) {
        if (!checkArguments(&cmd[4], 0)) {
            invalidArgs = true;
        }
    }
    if (isPasvCmd) {
        if (!checkArguments(&cmd[3], 1)) {
            invalidArgs = true;
        }
    }

    // Incorrect number of command arguments detected.
    if (invalidArgs) {
        send(socket, "501 Incorrect number of arguments.\r\n", strlen("501 Incorrect number of arguments.\r\n"), 0);
        return true;
    }

    // Execute the command.
    if (isUserCmd) {
        runUserCmd(&cmd[5], socket);
    } else if (isQuitCmd) {
        loggedIn = false;
        return false;
    } else if (isCwdCmd) {
        runCwdCmd(&cmd[4], socket);
    } else if (isCdupCmd) {
        runCDUPCmd(socket);
    } else if (isTypeCmd) {
        runTypeCmd(&cmd[5], socket);
    } else if (isModeCmd) {
        runModeCmd(&cmd[5], socket);
    } else if (isStruCmd) {
        runStruCmd(&cmd[5], socket);
    } else if (isRetrCmd) {
        runRetrCmd(&cmd[5], socket);
    } else if (isPasvCmd) {
        runPasvCmd(socket);
    } else if (isNlstCmd) {
        runNlstCmd(socket);
    } else if (isNewLine) {
    } else {
        // Invalid command.
        send(socket, "500 Invalid command.\r\n", strlen("500 Invalid command.\r\n"), 0);
    }
    return true;
}

/**
 * Returns true if a given string has the correct number of arguments, and false otherwise.
 * @param arguments The string of arguments.
 * @param numArgs The number of arguments the string should equal to.
 */
bool checkArguments(char *arguments, int numArgs) {
    int realNumArgs = 0;
    int i;
    for (i = 0; i < strlen(arguments); i++) {
        if (arguments[i] != ' ' && arguments[i] != '\000') {
            realNumArgs++;
            while (arguments[i] != ' ' && arguments[i] != '\000') {
                i++;
            }
            i--;
        }
    }
    return realNumArgs == numArgs;
}

/**
 * Runs the USER command.
 * @param username The username specified by the ftp client.
 */
void runUserCmd(char *username, int socket) {
    // Check if user is already logged in
    if (loggedIn) {
        send(socket, "230 User cs317 already logged in.\r\n", strlen("230 User cs317 already logged in.\r\n"), 0);
        return;
    }
    // Check username
    if (strcmp(username, "cs317") == 0) {
        send(socket, "230 User logged in, proceed.\r\n", strlen("230 User logged in, proceed.\r\n"), 0);
        loggedIn = true;
    } else {
        send(socket, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
    }
}

/**
 * Runs the CWD command.
 * Change working directory.
 * @param pathname The pathname specified by the ftp client
 */
void runCwdCmd(char *pathname, int socket) {
    // User not logged in
    if (!loggedIn) {
        send(socket, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
        return;
    }

    // Check if pathname contains './' or '../'
    if (strstr(pathname, "./") || strstr(pathname, "../")) {
        send(socket, "550 Failed to change directory.\r\n", strlen("550 Failed to change directory.\r\n"), 0);
        return;
    }

    // Get pathname of current directory
    char cwd[1024] = {0};
    strcpy(cwd, currentPath);

    // Get pathname of directory to go into
    strcat(cwd, "/");
    strcat(cwd, pathname);
    if (opendir(cwd) == NULL) {
        send(socket, "550 Failed to change directory.\r\n", strlen("550 Failed to change directory.\r\n"), 0);
        return;
    }

    // Change current directory to the directory to go into
    int pathLen = strlen(cwd);
    int i;
    for (i = 0; i < pathLen; i++) {
        currentPath[i] = cwd[i];
    }
    send(socket, "250 Directory successfully changed.\r\n", strlen("250 Directory successfully changed.\r\n"), 0);
}


/**
 * Runs the CDUP command.
 * Change remote working directory to parent directory.
 */
void runCDUPCmd(int socket) {
    if (!loggedIn) {
        send(socket, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
        return;
    }

    // Check if directory to go to is parent directory of ftp server
    if (strcmp(currentPath, ftpServerDir) == 0) {
        send(socket, "550 Failed to change directory.\r\n", strlen("550 Failed to change directory.\r\n"), 0);
        return;
    }

    // Get pathname of current directory
    char cwd[1024] = {0};
    strcpy(cwd, currentPath);

    // Get name of parent directory
    int i = strlen(cwd);

    while (cwd[i] != '/') {
        cwd[i] = '\000';
        i--;
    }
    cwd[i] = '\000';

    // Get pathname of parent directory
    if (opendir(cwd) == NULL) {
        send(socket, "550 Failed to change directory.\r\n", strlen("550 Failed to change directory.\r\n"), 0);
        return;
    }

    // Change current directory to parent directory
    int pathLen = strlen(currentPath);
    for (i = 0; i < pathLen; i++) {
        currentPath[i] = cwd[i];
    }

    send(socket, "200 Directory successfully changed.\r\n", strlen("200 Directory successfully changed.\r\n"), 0);
}

/**
 * Runs the TYPE command.
 * Set file transfer type.
 * @param typeCode The type code specified by the ftp client.
 */
void runTypeCmd(char *typeCode, int socket) {
    if (!loggedIn) {
        send(socket, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
        return;
    }

    // Convert type code to uppercase
    typeCode[0] = toupper(typeCode[0]);
    if (strcmp(typeCode, "A") == 0) {
        strcpy(currentType, "A");
        send(socket, "200 Type set to ASCII.\r\n", strlen("200 Type set to ASCII.\r\n"), 0);
        return;
    }
    if (strcmp(typeCode, "I") == 0) {
        strcpy(currentType, "I");
        send(socket, "200 Type set to Image.\r\n", strlen("200 Type set to Image.\r\n"), 0);
        return;
    }
    if (strcmp(typeCode, "E") == 0 || strcmp(typeCode, "N") == 0 || strcmp(typeCode, "T") == 0 ||
        strcmp(typeCode, "C") == 0 || strcmp(typeCode, "L") == 0) {
        send(socket, "504 Command not implemented for that parameter.\r\n",
             strlen("504 Command not implemented for that parameter.\r\n"), 0);
        return;

    }
    send(socket, "501 Syntax error in parameters or arguments.\r\n",
         strlen("501 Syntax error in parameters or arguments.\r\n"), 0);
}

/**
 * Runs the MODE command.
 * Set file transfer mode.
 * @param modeCode The mode code specified by the ftp client.
 */
void runModeCmd(char *modeCode, int socket) {
    if (!loggedIn) {
        send(socket, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
        return;
    }

    // Convert mode code to uppercase
    modeCode[0] = toupper(modeCode[0]);

    // Change mode
    if (strcmp(modeCode, "S") == 0) {
        send(socket, "200 Stream Mode set.\r\n", strlen("200 Stream Mode set.\r\n"), 0);
        strcpy(currentMode, "S");
        return;

    }
    if (strcmp(modeCode, "B") == 0 || strcmp(modeCode, "C") == 0) {
        send(socket, "504 Command not implemented for that parameter.\r\n",
             strlen("504 Command not implemented for that parameter.\r\n"), 0);
        return;
    }

    // Invalid parameter
    send(socket, "501 Syntax error in parameters or arguments.\r\n",
         strlen("501 Syntax error in parameters or arguments.\r\n"), 0);
}

/**
 * Runs the STRU command.
 * Set file transfer structure.
 * @param structureCode The structure code specified by the ftp client.
 */
void runStruCmd(char *structureCode, int socket) {
    if (!loggedIn) {
        send(socket, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
        return;
    }

    // Convert structure code to uppercase
    structureCode[0] = toupper(structureCode[0]);

    // Change structure
    if (strcmp(structureCode, "F") == 0) {
        send(socket, "200 File-structure set.\r\n", strlen("200 File-structure set.\r\n"), 0);
        strcpy(currentStructure, "F");
        return;
    }
    if (strcmp(structureCode, "R") == 0 || strcmp(structureCode, "P") == 0) {
        send(socket, "504 Command not implemented for that parameter.\r\n",
             strlen("504 Command not implemented for that parameter.\r\n"), 0);
        return;
    }

    // Invalid parameter
    send(socket, "501 Syntax error in parameters or arguments.\r\n",
         strlen("501 Syntax error in parameters or arguments.\r\n"), 0);
}

/**
 * Runs the RETR command.
 * Retrieve a copy of the file.
 * @param pathname The pathname specified by the ftp client.
 */
void runRetrCmd(char *filename, int socket) {
    if (!loggedIn) {
        send(socket, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
        return;
    }

    if (!inPasvMode) {
        send(socket, "425 Use PASV first.\r\n", strlen("425 Use PASV first.\r\n"), 0);
        return;
    }

    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        send(socket, "550 File unavailable.\r\n", strlen("550 File unavailable.\r\n"), 0);
        return;
    }

    if (strcmp(currentType, "A") == 0) {
        send(socket, "150 File status okay; about to open data connection.\r\n",
             strlen("150 File status okay; about to open data connection.\r\n"), 0);
        char buf[10000] = {0};

        while (fgets(buf, 10000, fp) != NULL) {
            send(pasvSocket, buf, strlen(buf), 0);
        }

        send(socket, "226 Transfer complete.\r\n", strlen("226 Transfer complete.\r\n"), 0);
    } else {
        fseek(fp, 0, SEEK_END);
        long lSize = ftell(fp);
        rewind(fp);
        char *buf = (char *) malloc(sizeof(char) * lSize);

        if (buf == NULL) {
            send(socket, "451 Requested action aborted: local error in processing.\r\n",
                 strlen("426 Connection closed; transfer aborted.\r\n"), 0);
            return;
        }

        size_t result = fread(buf, 1, (size_t) lSize, fp);

        if (result != lSize) {
            send(socket, "451 Requested action aborted: local error in processing.\r\n",
                 strlen("426 Connection closed; transfer aborted.\r\n"), 0);
            return;
        }

        send(socket, "150 File status okay; about to open data connection.\r\n",
             strlen("150 File status okay; about to open data connection.\r\n"), 0);
        send(pasvSocket, buf, (size_t) lSize, 0);
        send(socket, "226 Transfer complete.\r\n", strlen("226 Transfer complete.\r\n"), 0);
        free(buf);
    }

    fclose(fp);
    shutdown(pasvSocket, 2);
}

/**
 * Runs the PASV command.
 * Enter passive mode.
 */
void runPasvCmd(int skt) {
    if (!loggedIn) {
        send(skt, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
        return;
    }

    // Create new socket for data connection
    if ((pasvSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        send(skt, "421 Could not create socket.\r\n", strlen("421 Could not create socket.\r\n"), 0);
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Initialize address for passive socket to bind to.
    struct sockaddr_in init_addr;
    init_addr.sin_family = AF_INET;
    init_addr.sin_addr.s_addr = INADDR_ANY;
    init_addr.sin_port = 0; // Find an available port

    // Bind passive socket to a port
    if (bind(pasvSocket, (struct sockaddr *) &init_addr, sizeof(init_addr)) < 0) {
        send(skt, "421 Could not bind socket.\r\n", strlen("421 Could not bind socket.\r\n"), 0);
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Passive socket listens and accepts connection from client.
    // Start to listen
    if (listen(pasvSocket, 3) < 0) {
        send(skt, "421 Socket error.\r\n", strlen("421 Socket error.\r\n"), 0);
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Obtain the port number the passive socket bound to.
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    if (getsockname(pasvSocket, (struct sockaddr *) &addr, &addr_len) < 0) {
        send(skt, "421 Socket error.\r\n", strlen("421 Socket error.\r\n"), 0);
        perror("failed to retrieve socket name");
        exit(EXIT_FAILURE);
    }

    uint16_t portNumber = ntohs(addr.sin_port);

    // Get IP address of the server host
    char hostBuffer[1024];
    struct hostent *host_entry;
    if (gethostname(hostBuffer, 1024) < 0) {
        send(skt, "421 Unable to obtain host name.\r\n", strlen("421 Unable to obtain host name.\r\n"), 0);
        perror("error");
        exit(EXIT_FAILURE);
    }
    if ((host_entry = gethostbyname(hostBuffer)) == NULL) {
        send(skt, "421 Unable to obtain host name.\r\n", strlen("421 Unable to obtain host name.\r\n"), 0);
        perror("error");
        exit(EXIT_FAILURE);
    }
    char *hostIPAddr = inet_ntoa(*((struct in_addr *) host_entry->h_addr_list[0]));
    int i;
    for (i = 0; i < strlen(hostIPAddr); i++) {
        if (hostIPAddr[i] == '.') {
            hostIPAddr[i] = ',';
        }
    }

    // Format port number into bytes.
    int pnByte1 = (portNumber & 0xff);
    int pnByte2 = (portNumber & 0xffff) >> 8;

    // Convert port number to string
    char pn1Buffer[4] = {'\0', '\0', '\0', '\0'};
    char pn2Buffer[4] = {'\0', '\0', '\0', '\0'};
    sprintf(pn1Buffer, "%d", pnByte1);
    sprintf(pn2Buffer, "%d", pnByte2);

    // Create string containing host ip and port number to send to client
    char *pasvMsg = "227 Entering Passive Mode ";
    int len = 7 + strlen(pasvMsg) + strlen(hostIPAddr) + strlen(pn2Buffer) + strlen(pn1Buffer);
    char toSend[len];
    strcpy(toSend, pasvMsg);
    strcat(toSend, "(");
    strcat(toSend, hostIPAddr);
    strcat(toSend, ",");
    strcat(toSend, pn2Buffer);
    strcat(toSend, ",");
    strcat(toSend, pn1Buffer);
    strcat(toSend, ").");
    strcat(toSend, "\r\n");

    send(skt, toSend, strlen(toSend), 0);

    fd_set rfds;
    struct timeval timeout;
    int retval;

    timeout.tv_sec = 20;
    timeout.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(pasvSocket, &rfds);

    retval = select(pasvSocket + 1, &rfds, NULL, NULL, &timeout);

    if (retval == -1) {
        perror("select()");
        send(skt, "426 Socket timeout.\r\n",
             strlen("426 Socket timeout.\r\n"), 0);
        return;
    } else if (retval) {
        printf("Data is available now.\n");
    } else {
        send(skt, "426 Socket timeout.\r\n",
             strlen("426 Socket timeout.\r\n"), 0);
        return;
    }

    // Accept connection request
    if ((pasvSocket = accept(pasvSocket, (struct sockaddr *) &addr, (socklen_t *) &addr)) < 0) {
        send(skt, "425 Can't open data connection.\r\n", strlen("425 Can't open data connection.\r\n"), 0);
        return;
    }

    inPasvMode = true;
}

/**
 * Runs the NLST command.
 * Returns a list of file names in a specified directory.
 * @param pathname The pathname specified by the ftp client.
 */
void runNlstCmd(int socket) {
    if (!loggedIn) {
        send(socket, "530 Not logged in.\r\n", strlen("530 Not logged in.\r\n"), 0);
        return;
    }
    if (!inPasvMode) {
        send(socket, "425 Use PASV first.\r\n", strlen("425 Use PASV first.\r\n"), 0);
        return;
    }
    char cwd[1024] = {0};
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        send(socket, "451 Requested action aborted: local error in processing.\r\n",
             strlen("451 Requested action aborted: local error in processing.\r\n"), 0);
        perror("getcwd() error");
    }

    send(socket, "125 Here comes the directory listing.\r\n", strlen("125 Here comes the directory listing.\r\n"), 0);

    if (listFiles(pasvSocket, cwd) == -1) {
        send(socket, "450 Directory listing error.\r\n", strlen("450 Directory listing error.\r\n"), 0);
    }

    send(socket, "226 Dictionary send OK.\r\n", strlen("226 Dictionary send OK.\r\n"), 0);

    shutdown(pasvSocket, 2);
}