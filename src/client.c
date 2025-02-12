#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>

#include "client.h"
#include "utils.h"

int lookup_host(char *buf, char *host, int portno) {
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN], service[256];

    //initialize lookup
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    sprintf(service, "%d", portno);

    //dns lookup
    if((status = getaddrinfo(host, service, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    //find the first IPv4 address
    for(p = res; p != 0; p = p->ai_next) {
        void *addr = 0;
        if(p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
            memcpy(buf, ipstr, sizeof(ipstr));
            break;
        }
    }

    //cleanup
    freeaddrinfo(res);
    return 0;
}

/*
 * Check the status for a command response
 * Returns 0 if the status matches target, otherwise it returns -1
 */
static int check_status(char **rest, int target) {
    int result = -1;
    char *token = strtok_r(*rest, " ", rest);
    if(token != 0) {
        sscanf(token, "%d", &result);
        if(result == target)
            return 0;
    }
    return -1;
}

int ftp_connect(struct ftp_connection *conn, char *host, int portno) {
    int control_fd;
    struct sockaddr_in control_addr;

    char buf[1024], ipaddr[INET6_ADDRSTRLEN];
    memset(buf, 0, sizeof(buf));

    //try to create socket
    if((control_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Could not create control socket.\n");
        return -1;
    }

    //configuration values
    control_addr.sin_family = AF_INET;
    control_addr.sin_port = htons(portno);

    //DNS lookup
    if(lookup_host(ipaddr, host, portno) < 0) {
        fprintf(stderr, "Could not resolve host: %s\n", host);
        close(control_fd);
        return -1;
    }

    //convert string IP address to bytes IP address
    if(inet_pton(AF_INET, ipaddr, &control_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s.\n", ipaddr);
        close(control_fd);
        return -1;
    }

    //connect to the server
    if(connect(control_fd, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0) {
        fprintf(stderr, "Could not connect to remote address.\n");
        close(control_fd);
        return -1;
    }

    //read the response from the server
    if(read(control_fd, buf, sizeof(buf) -1) < 0) {
        fprintf(stderr, "No response from server.\n");
        close(control_fd);
        return -1;
    }

    //set the connection configs of the ftp_connection
    conn->control_conn = control_fd;
    conn->data_conn = -1;
    conn->control_open = 1;
    conn->data_open = 0;

    return 0;
}

int ftp_auth(struct ftp_connection *conn, char *user, char *pass) {
    //check the control connection
    if(conn->control_open == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //declare memory for sending authentication commands
    char userbuf[512], passbuf[512], response[1024];
    memset(userbuf, 0, sizeof(userbuf));
    memset(passbuf, 0, sizeof(passbuf));
    memset(response, 0, sizeof(response));

    //create the USER and PASS commands
    snprintf(userbuf, sizeof(userbuf) - 1, "USER %s\r\n", user);
    snprintf(passbuf, sizeof(passbuf) - 1, "PASS %s\r\n", pass);

    //send the username and make sure the server responds
    send(conn->control_conn, userbuf, strlen(userbuf), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Failed to read user authentication response.\n");
        return -1;
    }

    //reset the response buffer
    memset(response, 0, sizeof(response));

    //send the password and make sure the server responds
    send(conn->control_conn, passbuf, strlen(passbuf), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Failed to read password authentication response.\n");
        return -1;
    }

    //make sure the authentication succeeded
    char *rest = response;
    if(check_status(&rest, 230) < 0) {
        fprintf(stderr, "Failed to authenticate.\n");
        return -1;
    }

    return 0;
}

int ftp_init_data(struct ftp_connection *conn) {
    //check the control connection
    if(conn->control_open == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //create the passive command
    char *command = "PASV\r\n";
    char response[1024];
    memset(response, 0, sizeof(response));

    //send the passive command and check for a response
    send(conn->control_conn, command, strlen(command), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Failed to read passive handshake response.\n");
        return -1;
    }

    //needed for moving forward
    char *rest = response;
    int h1, h2, h3, h4, p1, p2;

    if(check_status(&rest, 227) < 0) {
        fprintf(stderr, "Failed to enter passive mode.\n");
        return -1;
    }

    //grab the values needed to initialize the data connection
    sscanf(rest, "Entering Passive Mode (%d,%d,%d,%d,%d,%d) \r\n", &h1, &h2, &h3, &h4, &p1, &p2);
    
    //create the IP address and port number to initialize the data connection
    char ipaddr[INET6_ADDRSTRLEN];
    memset(ipaddr, 0, sizeof(ipaddr));
    snprintf(ipaddr, sizeof(ipaddr) - 1, "%d.%d.%d.%d", h1, h2, h3, h4);
    int portno = p1 * 256 + p2;

    //for the data socket
    int data_fd;
    struct sockaddr_in data_addr;

    //try to create socket
    if((data_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Could not create data socket.\n");
        return -1;
    }

    //configuration values
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(portno);

    //convert string IP address to bytes IP address
    if(inet_pton(AF_INET, ipaddr, &data_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s.\n", ipaddr);
        close(data_fd);
        return -1;
    }

    //connect to the server
    if(connect(data_fd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
        fprintf(stderr, "Could not connect to remote address.\n");
        close(data_fd);
        return -1;
    }

    //update the data config values in the ftp connection
    conn->data_conn = data_fd;
    conn->data_open = 1;

    return 0;
}

int ftp_pwd(struct ftp_connection *conn, char *buf) {
    //check the control connection
    if(conn->control_open == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //create the command and buffer for the response
    char *command = "PWD\r\n";
    char response[1024];
    memset(response, 0, sizeof(response));

    //send the command and try to read the response
    send(conn->control_conn, command, strlen(command), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Could not read PWD response.\n");
        return -1;
    }

    //check the status of the command
    char *rest = response;
    if(check_status(&rest, 257) < 0) {
        fprintf(stderr, "Failed to retrieve CWD.\n");
        return -1;
    }

    sscanf(rest, "\"%[^\"]\" is the current directory\r\n", buf);

    return 0;
}

int ftp_mkdir(struct ftp_connection *conn, char *path) {
    //check if the control connection is valid
    if(conn->control_conn == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //initialize values for creating the command
    char commandbuf[4096], response[1024];
    memset(commandbuf, 0, sizeof(commandbuf));
    memset(response, 0, sizeof(response));

    //create the command
    snprintf(commandbuf, sizeof(commandbuf) - 1, "MKD %s\r\n", path);

    //send the command and receive the response
    send(conn->control_conn, commandbuf, strlen(commandbuf), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Could not read MKD command response.\n");
        return -1;
    }

    //check the status of the response
    char *rest = response;
    if(check_status(&rest, 257) < 0) {
        fprintf(stderr, "Failed to make directory: %s.\n", path);
        return -1;
    }

    return 0;
}

int ftp_rmdir(struct ftp_connection *conn, char *path) {
    //check the control connection
    if(conn->control_open == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //initialize values for creating the command
    char commandbuf[4096], response[1024];
    memset(commandbuf, 0, sizeof(commandbuf));
    memset(response, 0, sizeof(response));

    //create the command
    snprintf(commandbuf, sizeof(commandbuf) - 1, "RMD %s\r\n", path);

    //send the command and receive the response
    send(conn->control_conn, commandbuf, strlen(commandbuf), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Could not read RMD command response.\n");
        return -1;
    }

    //check the status of the response
    char *rest = response;
    if(check_status(&rest, 250) < 0) {
        fprintf(stderr, "Failed to remove directory: %s.\n", path);
        return -1;
    }

    return 0;
}

int ftp_cd(struct ftp_connection *conn, char *path) {
    //check control connection
    if(conn->control_open == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //initialize data for command
    char commandbuf[4096], response[1024];
    memset(commandbuf, 0, sizeof(commandbuf));
    memset(response, 0, sizeof(response));

    //create command
    snprintf(commandbuf, sizeof(commandbuf) - 1, "CWD %s\r\n", path);

    //send the command and check the results
    send(conn->control_conn, commandbuf, strlen(commandbuf), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Could not read response of CWD command.\n");
        return -1;
    }

    //check the results of the command
    char *rest = response;
    if(check_status(&rest, 250) < 0) {
        return -1;
    }

    return 0;
}

int ftp_rm(struct ftp_connection *conn, char *path) {
    //check control connection
    if(conn->control_open == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //initialize data for command
    char commandbuf[4096], response[1024];
    memset(commandbuf, 0, sizeof(commandbuf));
    memset(response, 0, sizeof(response));

    //create command
    snprintf(commandbuf, sizeof(commandbuf) - 1, "DELE %s\r\n", path);

    //send command and receive result
    send(conn->control_conn, commandbuf, strlen(commandbuf), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Could not read DELE result.\n");
        return -1;
    }

    //check the result of the command
    char *rest = response;
    if(check_status(&rest, 250) < 0) {
        fprintf(stderr, "Could not delete file: %s.\n", path);
        return -1;
    }

    return 0;
}

int ftp_cp(struct ftp_connection *conn, char *src, char *dest) {
    //check control connection
    if(conn->control_open == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //get the relative filename of the src file
    char relative_filename[256];
    memset(relative_filename, 0, sizeof(relative_filename));
    filename(relative_filename, src, sizeof(relative_filename) - 1);

    //change directories to the destination folder
    if(ftp_cd(conn, dest) < 0) {
        fprintf(stderr, "Could not change directories to directory: %s.\n", dest);
        return -1;
    }

    //initialize data for command
    char commandbuf[4096], response[1024];
    memset(commandbuf, 0, sizeof(commandbuf));
    memset(response, 0, sizeof(response));

    //create the command
    snprintf(commandbuf, sizeof(commandbuf) - 1, "STOR %s\r\n", relative_filename);

    //send the command and receive the response
    send(conn->control_conn, commandbuf, strlen(commandbuf), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Could not read response of STOR command.\n");
        return -1;
    }

    //check the response
    char *rest = response;
    if(check_status(&rest, 150) < 0) {
        fprintf(stderr, "Failed to intiate copy file: %s.\n", relative_filename);
        return -1;
    }

    //check the data connection
    if(conn->data_open == 0) {
        fprintf(stderr, "Invalid connection. Data connection is closed.\n");
        return -1;
    }

    //open the file on the client machine
    int fd;
    if((fd = open(src, O_RDONLY)) < 0) {
        fprintf(stderr, "Could not open file: %s.\n", src);
        return -1;
    }

    //copy the file to the server
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    while(read(fd, buf, sizeof(buf) - 1) > 0) {
        send(conn->data_conn, buf, strlen(buf), 0);
        memset(buf, 0, sizeof(buf));
    }

    close(conn->data_conn);
    conn->data_open = 0;

    //get the response of the server
    memset(response, 0, sizeof(response));
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Failed to read response of STOR command\n");
        return -1;
    }

    rest = response;
    if(check_status(&rest, 226) < 0) {
        fprintf(stderr, "Failed to copy file: %s\n", relative_filename);
        return -1;
    }

    return 0;
}

int ftp_file_exists(struct ftp_connection *conn, char *path) {
    //check the control connection
    if(conn->control_open == 0) {
        fprintf(stderr, "Invalid connection. Control connection is closed.\n");
        return -1;
    }

    //initialize command data
    char commandbuf[4096], response[1024];
    memset(commandbuf, 0, 4096);
    memset(response, 0, 1024);

    //create command
    snprintf(commandbuf, sizeof(commandbuf) - 1, "SIZE %s\r\n", path);

    send(conn->control_conn, commandbuf, strlen(commandbuf), 0);
    if(read(conn->control_conn, response, sizeof(response) - 1) < 0) {
        fprintf(stderr, "Could not read response from SIZE command.\n");
        return -1;
    }

    //check if the file exists
    char *rest = response;
    if(check_status(&rest, 550) == 0) {
        return -1;
    }

    return 0;
}

int ftp_directory_exists(struct ftp_connection *conn, char *path) {
    //save the state of the system
    char cwd[4096];
    memset(cwd, 0, sizeof(cwd));
    ftp_pwd(conn, cwd);
    
    //try to change the directory to the path
    if(ftp_cd(conn, path) < 0) {
        return -1;
    }

    //restore state
    ftp_cd(conn, cwd);

    return 0;
}

void ftp_close(struct ftp_connection *conn) {
    //if the control connection is open, close it
    if(conn->control_open == 1) {
        conn->control_open = 0;
        close(conn->control_conn);
    }

    //if the data connection is open, close it
    if(conn->data_open == 1) {
        conn->data_open = 0;
        close(conn->data_conn);
    }
}