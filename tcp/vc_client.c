// コネクション型の簡単なリモートファイル表示クライアント(vc_client.c)
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define MAX_HOSTNAME_LEN 64
#define DEFAULT_PORT 5000  // 本サーバが用いるポート番号

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

int setup_vc_client(struct hostent *hostent, u_short port);

void handler_server(int sock);

void receive_files(int sock, char *token);

void send_file(int sock, char *token);

int main() {
    char s_hostname[MAX_HOSTNAME_LEN];
    struct hostent *s_hostent;

    // サーバのホスト名の入力
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

int setup_vc_client(struct hostent *hostent, u_short port) {
    int sock;
    // インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // サーバのアドレス(IPアドレスとポート番号)の作成
    struct sockaddr_in s_address;
    bzero((char *) &s_address, sizeof(s_address));
    s_address.sin_family = AF_INET;
    s_address.sin_port = htons(port);
    bcopy(hostent->h_addr, (char *) &s_address.sin_addr, hostent->h_length);
    // サーバとの接続の確立
    if (connect(sock, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }
    return sock;
}

void handler_server(const int sock) // サーバから受け取ったファイルの内容を表示する
{
    while (1) {
        char buffer[BUFFER_SIZE];
        printf("Enter command (GET:<filename> or PUT:<filename>)"); // コマンドの入力
        fflush(stdout);
        fflush(stdin);
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL || strncmp(buffer, SHUTDOWN_HEADER, SHUTDOWN_HEADER_LEN) == 0) {
            send(sock, SHUTDOWN_HEADER, SHUTDOWN_HEADER_LEN, 0);
            printf("session close request sent\n");
            break;
        }
        if (strncmp(buffer, GET_HEADER, GET_HEADER_LEN) == 0) {
            receive_files(sock, buffer);
        } else if (strncmp(buffer, PUT_HEADER, PUT_HEADER_LEN) == 0) {
            send_file(sock, buffer);
        }
    }
}

void receive_files(const int sock, char *token) {
    char buffer[BUFFER_SIZE] = {0};
    token[strcspn(token, "\n")] = '\0'; // NULL終端を保証
    send(sock, token, strlen(token) + 1, 0); // getコマンドを送信 NULL文字も送信して本文と区別
    while (1) {
        const ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break; // 接続が閉じられたかエラーが発生した場合は終了
        }

        // EOTマーカーが含まれている場合、EOTまでのデータを出力して終了
        const char *eot_pos = memchr(buffer, EOT, bytes_received);
        if (eot_pos != NULL) {
            // EOTの前までのデータを出力
            fwrite(buffer, sizeof(char), eot_pos - buffer, stdout);
            break;
        }

        // EOTが見つからない場合は全データを出力
        fwrite(buffer, sizeof(char), bytes_received, stdout);
    }
}

void send_file(const int sock, char *token) {
    token[strcspn(token, "\n")] = '\0'; // NULL終端を保証
    char filename[BUFFER_SIZE] = {0};
    strncpy(filename, token + PUT_HEADER_LEN, sizeof(filename) - 1); // ファイル名を抽出
    filename[sizeof(filename) - 1] = '\0'; // NULL終端を保証
    FILE *file = fopen(filename, "r");
    char buffer[BUFFER_SIZE];
    if (file == NULL) {
        perror("File opening failed");
        printf("\n");
        return;
    }
    send(sock, token, strlen(token) + 1, 0); // PUTコマンドを送信 NULL文字も送信して本文と区別
    while (fgets(buffer, BUFFER_SIZE, file) != NULL) {
        send(sock, buffer, strlen(buffer), 0);
    }
    fclose(file);
    const ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
    if (buffer[bytes_received - 1] == NAK) {
        printf("\nCouldn't send %s\n", filename);
        return;
    }
    printf("File %s sent to server.\n", filename);
}
