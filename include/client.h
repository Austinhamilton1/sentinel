#ifndef CLIENT_H
#define CLIENT_H

#include "list.h"

//necessary information to connect to an FTP server
struct ftp_connection {
    int control_conn, control_open; //control connection
    int data_conn, data_open; //data connection
};

/*
 * Look up a host name and convert it into an IP address
 * Return 0 on success, -1 otherwise
 */
int lookup_host(char *buf, char *host, int portno);

/*
 * Connect to an FTP server
 * Return 0 on success or -1 on failure
 */
int ftp_connect(struct ftp_connection *conn, char *host, int portno);

/*
 * Authenticate a user on the server.
 * Should be called immediately after connect
 * Returns -1 on failure, 0 otherwise
 */
int ftp_auth(struct ftp_connection *conn, char *user, char *pass);

/*
 * Initialize the data connection in passive mode
 * Returns 0 on success, -1 otherwise
 */
int ftp_init_data(struct ftp_connection *conn);

/*
 * Print the working directory of an ftp_connection
 * Returns 0 on success or -1 on failure
 */
int ftp_pwd(struct ftp_connection *conn, char *buf);

/*
 * Make a directory on the ftp server
 * Returns 0 on success or -1 on failure
 */
int ftp_mkdir(struct ftp_connection *conn, char *path);

/*
 * Remove a directory from the ftp server
 * Returns 0 on success or -1 on failure
 */
int ftp_rmdir(struct ftp_connection *conn, char *path);

/*
 * List out the files in a directory on the ftp server
 * Returns 0 on success or -1 on failure
 */
int ftp_ls(struct ftp_connection *conn, struct list *list, char *path);

/*
 * Change directories on the server
 * Returns 0 on success or -1 on failure
 */
int ftp_cd(struct ftp_connection *conn, char *path);

/*
 * Remove a file from the server
 * Returns 0 on success or -1 on failure
 */
int ftp_rm(struct ftp_connection *conn, char *path);

/*
 * Copy a file to the server
 * Returns 0 on success, -1 otherwise
 */
int ftp_cp(struct ftp_connection *conn, char *src, char *dest);

/*
 * Check if a file exists on the server
 * Returns 0 if the file exists, -1 otherwise
 */
int ftp_file_exists(struct ftp_connection *conn, char *path);

/*
 * Check if a directory exists on the server
 * Returns 0 if the file exists, -1 otherwise
 */
int ftp_directory_exists(struct ftp_connection *conn, char *path);

/*
 * Close an ftp connection
 */
void ftp_close(struct ftp_connection *conn);

#endif