/* コネクション型の簡単なリモートファイル表示サーバ(vc_server.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <sys/wait.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_HOSTNAME_LEN 64
#define DEFAULT_PORT 5000  /* 本サーバが用いるポート番号 */
#define MAX_FILE_NAME 255
#define BUFFER_SIZE 1024
#define SHUTDOWN_HEADER "SHUTDOWN:"
#define SHUTDOWN_HEADER_LEN 9
#define PUT_HEADER "PUT:"
#define PUT_HEADER_LEN 4
#define GET_HEADER "GET:"
#define GET_HEADER_LEN 4
#define EOT "\x04"

int server_fd = -1;

int setup_vc_server(const struct hostent *hostent, uint16_t port);

void handler_client(int sock);

void shutdown_server(int sig) {
    printf("\nShutting down server...\n");
    close(server_fd);
    exit(EXIT_SUCCESS);
}

// 子プロセスが終了したときに呼び出されるハンドラ
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    char s_hostname[MAX_HOSTNAME_LEN];
    /* サーバのホスト名とそのIPアドレス(をメンバに持つhostent構造体)を求める */
    gethostname(s_hostname, sizeof(s_hostname));
    const struct hostent *s_hostent = gethostbyname(s_hostname);

    /* バーチャルサーキットサーバの初期設定 */
    signal(SIGINT, shutdown_server);

    // SIGCHLDシグナル（子プロセス終了）を無視し、ゾンビプロセスを防止
    signal(SIGCHLD, sigchld_handler);

    server_fd = setup_vc_server(s_hostent, DEFAULT_PORT);
    printf("Server listening on port %d\n", DEFAULT_PORT);
    while (1) {
        /* 接続要求の受け入れ */
        struct sockaddr_in c_address;
        socklen_t c_addrlen = sizeof(c_address);
        int client_sock;
        if ((client_sock = accept(server_fd, (struct sockaddr *) &c_address, &c_addrlen)) < 0) {
            perror("accept");
            exit(1);
        }
        printf("new session started\n");
        // フォーク(並行サーバのサービス)
        const pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            // 子プロセス
            close(server_fd);
            // クライアントが要求するファイルの送信
            handler_client(client_sock);
            close(client_sock);
            exit(EXIT_SUCCESS);
        }
        close(client_sock); /* 親プロセス */
    }
}

int setup_vc_server(const struct hostent *hostent, const uint16_t port) {
    /* インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築 */
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in s_address;
    /* アドレス(IPアドレスとポート番号)の作成 */
    bzero((char *) &s_address, sizeof(s_address));
    s_address.sin_family = AF_INET;
    s_address.sin_port = htons(port);
    bcopy(hostent->h_addr, (char *) &s_address.sin_addr, hostent->h_length);

    /* アドレスのソケットへの割り当て */
    if (bind(sock, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* 接続要求待ち行列の長さを5とする */
    if (listen(sock, 5) < 0) {
        perror("listen");
        close(sock);
        exit(EXIT_FAILURE);
    }
    return sock;
}

void handler_client(const int sock) {
    bool is_continue = true;
    while (is_continue) {
        char buffer[BUFFER_SIZE] = {0};
        /* クライアントから送られるファイル名をソケットから読み込む */
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        // 終了命令を受け取った時の処理
        if (strncmp(buffer, SHUTDOWN_HEADER, SHUTDOWN_HEADER_LEN) == 0) {
            printf("session close request received\n");
            is_continue = false;
        } else if (strncmp(buffer, PUT_HEADER, PUT_HEADER_LEN) == 0) {
            char filename[BUFFER_SIZE] = {0};
            sscanf(buffer + PUT_HEADER_LEN, "%1023s", filename);
            FILE *fp = fopen(filename, "w");
            if (fp == NULL) {
                perror("File opening failed");
                while (recv(sock, buffer, BUFFER_SIZE, MSG_DONTWAIT) > 0);
            } else
                while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
                    if (buffer[0] == '\x04') break;
                    fwrite(buffer, sizeof(char), bytes_received, fp);
                    if (bytes_received < BUFFER_SIZE) break;
                }
            printf("File %s receive complete\n", filename);
            fclose(fp);
        } else if (strncmp(buffer, GET_HEADER, GET_HEADER_LEN) == 0) {
            char filenames[BUFFER_SIZE];
            strncpy(filenames, buffer + GET_HEADER_LEN, sizeof(filenames) - 1);
            filenames[sizeof(filenames) - 1] = '\0'; // NULL終端を保証

            // カンマで区切られた複数のファイル名を処理する
            char *filename = strtok(filenames, ",");
            while (filename != NULL) {
                FILE *fp = fopen(filename, "r");
                if (fp == NULL) {
                    perror("File opening failed");
                    const char *error_msg = "ERROR: File not found\n";
                    send(sock, error_msg, strlen(error_msg), 0);
                } else {
                    // ファイル内容を全て送信する
                    while (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
                        size_t bytes_to_send = strlen(buffer);
                        ssize_t bytes_sent = 0;
                        while (bytes_to_send > 0) {
                            bytes_sent = send(sock, buffer, bytes_to_send, 0);
                            if (bytes_sent < 0) {
                                perror("send");
                                fclose(fp);
                                return;
                            }
                            bytes_to_send -= bytes_sent;
                        }
                    }
                    fclose(fp);

                    printf("File %s send complete\n", filename);
                }
                // 次のファイル名に移動
                filename = strtok(NULL, ",");
            }
            // ファイルの送信終了を示すEOTマーカーを送信
            send(sock, EOT, strlen(EOT), 0);
        }
    }
}
