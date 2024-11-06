/* コネクション型の簡単なリモートファイル表示クライアント(vc_client.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */

#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

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

int setup_vc_client(struct hostent *hostent, uint16_t port);

void handler_server(int sock);

void receive_files(int sock);

void send_file(int sock, const char *filename);

int main() {
    char s_hostname[MAX_HOSTNAME_LEN];
    struct hostent *s_hostent;

    /* サーバのホスト名の入力 */
    printf("server host name?: ");
    scanf("%s", s_hostname);
    /* サーバホストのIPアドレス(をメンバに持つhostent構造体)を求める */
    if ((s_hostent = gethostbyname(s_hostname)) == NULL) {
        fprintf(stderr, "server host does not exists\n");
        exit(1);
    }

    /* バーチャルサーキットクライアントの初期設定 */
    const int sock = setup_vc_client(s_hostent, DEFAULT_PORT);

    /* サーバにファイルを要求し受信したファイルの内容を標準出力に出力 */
    handler_server(sock);
    printf("client will close.\n");
    close(sock);
    return 0;
}

int setup_vc_client(struct hostent *hostent, uint16_t port) {
    int sock;
    /* インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築 */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    /* サーバのアドレス(IPアドレスとポート番号)の作成 */
    struct sockaddr_in s_address;
    bzero((char *) &s_address, sizeof(s_address));
    s_address.sin_family = AF_INET;
    s_address.sin_port = htons(port);
    bcopy(hostent->h_addr, (char *) &s_address.sin_addr, hostent->h_length);
    /* サーバとの接続の確立 */
    if (connect(sock, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }

    return sock;
}

void handler_server(const int sock) /* サーバから受け取ったファイルの内容を表示する */
{
    bool is_continue = true;
    while (is_continue) {
        char buffer[BUFFER_SIZE];
        /* ファイル名の入力 */
        printf("Enter command (GET:<filename> or PUT:<filename>)");
        fflush(stdout);
        fflush(stdin);
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL || strncmp(buffer, SHUTDOWN_HEADER, SHUTDOWN_HEADER_LEN) == 0) {
            send(sock, SHUTDOWN_HEADER, SHUTDOWN_HEADER_LEN, 0);
            is_continue = false;
        } else if (strncmp(buffer, GET_HEADER, GET_HEADER_LEN) == 0) {
            buffer[strcspn(buffer, "\n")] = '\0'; // 改行を削除
            send(sock, buffer, strlen(buffer), 0);
            receive_files(sock);
        } else if (strncmp(buffer, PUT_HEADER, PUT_HEADER_LEN) == 0) {
            buffer[strcspn(buffer, "\n")] = '\0'; // 改行を削除
            send(sock, buffer, strlen(buffer), 0);
            send_file(sock, buffer + PUT_HEADER_LEN);
        } else {
            fputs("\nInvalid command.\n", stderr);
        }
    }
}

void receive_files(const int sock) {
    char buffer[BUFFER_SIZE] = {0};
    while (1) {
        const ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        // 受信データの表示
        fputs(buffer, stdout);
        // EOTマーカーが受信された場合はファイルの終わり
        if (buffer[bytes_received - 1] == EOT[0]) {
            printf("\nEnd of file received.\n");
            break;
        }
    }
}

void send_file(const int sock, const char *filename) {
    FILE *file = fopen(filename, "r");
    char buffer[BUFFER_SIZE];
    if (file == NULL) {
        perror("File opening failed");
        send(sock, EOT, 1, 0);
        return;
    }

    while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
        send(sock, buffer, strlen(buffer), 0);
    }
    fclose(file);
    printf("File %s sent to server.\n", filename);
}
