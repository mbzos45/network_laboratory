// コネクション型の簡単なリモートファイル表示サーバ(vc_server.c)
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define MAX_HOSTNAME_LEN 64
#define DEFAULT_PORT 5000  /* 本サーバが用いるポート番号 */
#define BUFFER_SIZE 1024
#define SHUTDOWN_HEADER "SHUTDOWN:"
#define SHUTDOWN_HEADER_LEN 9
#define PUT_HEADER "PUT:"
#define PUT_HEADER_LEN 4
#define GET_HEADER "GET:"
#define GET_HEADER_LEN 4
const char ACK = 0x06;
const char NAK = 0x15;
const char EOT = 0x04;

int server_fd = -1;

int setup_vc_server(const struct hostent *hostent, u_short port);

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
    // サーバのホスト名とそのIPアドレス(をメンバに持つhostent構造体)を求める
    gethostname(s_hostname, sizeof(s_hostname));
    const struct hostent *s_hostent = gethostbyname(s_hostname);

    signal(SIGINT, shutdown_server); // サーバー終了時のコールバック関数の設定
    signal(SIGCHLD, sigchld_handler); // SIGCHLDシグナル（子プロセス終了）を無視し、ゾンビプロセスを防止
    server_fd = setup_vc_server(s_hostent, DEFAULT_PORT); // バーチャルサーキットサーバの初期設定
    printf("Server listening on port %d\n", DEFAULT_PORT);
    while (1) {
        /* 接続要求の受け入れ */
        struct sockaddr_in c_address;
        socklen_t c_addrlen = sizeof(c_address);
        int client_sock;
        if ((client_sock = accept(server_fd, (struct sockaddr *) &c_address, &c_addrlen)) < 0) {
            perror("accept");
            printf("\n");
            exit(1);
        }
        printf("new session started\n");
        // フォーク(並行サーバのサービス)
        const pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            printf("\n");
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

int setup_vc_server(const struct hostent *hostent, const u_short port) {
    // インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        printf("\n");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in s_address;
    // アドレス(IPアドレスとポート番号)の作成
    bzero((char *) &s_address, sizeof(s_address));
    s_address.sin_family = AF_INET; // ipv4を指定
    s_address.sin_port = htons(port);
    bcopy(hostent->h_addr, (char *) &s_address.sin_addr, hostent->h_length);

    // アドレスのソケットへの割り当て
    if (bind(sock, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("bind");
        printf("\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // 接続要求待ち行列の長さを5とする
    if (listen(sock, 5) < 0) {
        perror("listen");
        printf("\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    return sock;
}

void handler_client(const int sock) {
    while (1) {
        char buffer[BUFFER_SIZE] = {0};
        // クライアントから送られるコマンドをソケットから読み込む
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        if (strncmp(buffer, SHUTDOWN_HEADER, SHUTDOWN_HEADER_LEN) == 0) {
            // 終了命令を受け取った時の処理
            printf("session close request received\n");
            break;
        }
        if (strncmp(buffer, PUT_HEADER, PUT_HEADER_LEN) == 0) {
            // アップロード処理
            char filename[BUFFER_SIZE] = {0};
            strncpy(filename, buffer + PUT_HEADER_LEN, sizeof(filename) - 1); // ヘッダーを削除
            filename[sizeof(filename) - 1] = '\0'; // NULL終端を保証
            FILE *fp = fopen(filename, "w");
            if (fp == NULL) {
                // ファイル作成失敗時
                perror("File opening failed");
                printf("\n");
                while (recv(sock, buffer, BUFFER_SIZE, MSG_DONTWAIT) > 0) {
                } // クライアントからの送信は無視
                send(sock, &NAK, 1, 0); //保存に失敗したことをクライアントに伝える
            } else {
                while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
                    fwrite(buffer, sizeof(char), bytes_received, fp);
                    if (bytes_received < BUFFER_SIZE) break;
                }
                send(sock, &ACK, 1, 0); //保存に成功したことをクライアントに伝える
                printf("File %s receive complete\n", filename);
                fclose(fp);
            }
        } else if (strncmp(buffer, GET_HEADER, GET_HEADER_LEN) == 0) {
            char filenames[BUFFER_SIZE] = {0};
            strncpy(filenames, buffer + GET_HEADER_LEN, sizeof(filenames) - 1); // ヘッダーを削除
            filenames[sizeof(filenames) - 1] = '\0'; // NULL終端を保証
            char *filename = strtok(filenames, ","); // カンマで区切られた複数のファイル名を処理する
            while (filename != NULL) {
                FILE *fp = fopen(filename, "r");
                if (fp == NULL) {
                    perror("File opening failed");
                    printf("\n");
                    const char *error_msg = "ERROR: File not found\n";
                    send(sock, error_msg, strlen(error_msg), 0); // エラーメッセージを送信
                } else {
                    const char *start_msg = "<-START FILE->\n";
                    send(sock, start_msg, strlen(start_msg), 0);
                    while (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
                        // ファイル内容を全て送信する
                        size_t bytes_to_send = strlen(buffer); // 未送信データ数
                        ssize_t bytes_sent = 0; // 送信したデータ数
                        while (bytes_to_send > 0) {
                            bytes_sent = send(sock, buffer, bytes_to_send, 0); //実送信数を確認
                            if (bytes_sent < 0) {
                                perror("send");
                                printf("\n");
                                fclose(fp);
                                return;
                            }
                            bytes_to_send -= bytes_sent;
                        }
                    }
                    fclose(fp);
                    const char *end_msg = "<-END FILE->\n";
                    send(sock, end_msg, strlen(end_msg), 0);
                    printf("File %s send complete\n", filename);
                }
                filename = strtok(NULL, ","); // 次のファイル名に移動
            }
            send(sock, &EOT, 1, 0); // ファイルの送信終了を示すEOTマーカーを送信
        }
    }
}
