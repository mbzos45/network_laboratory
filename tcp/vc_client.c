/* コネクション型の簡単なリモートファイル表示クライアント(vc_client.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define  MAX_HOST_NAME    64
#define     S_TCP_PORT    7000
#define  MAX_FILE_NAME    255
#define     MAX_BUF_LEN    512
#define CLOSE_HEADER "shutdown:"
#define PUT_HEADER "PUT:"

const size_t PUT_HEADER_LEN = strlen(PUT_HEADER);

int setup_vcclient(struct hostent *, u_short);

void receive_file(int);

int main() {
    int socd;
    char s_hostname[MAX_HOST_NAME];
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
    socd = setup_vcclient(s_hostent, S_TCP_PORT);

    /* サーバにファイルを要求し受信したファイルの内容を標準出力に出力 */
    receive_file(socd);
    printf("client will close.\n");
    close(socd);
    return 0;
}

int setup_vcclient(struct hostent *hostent, u_short port) {
    int socd;
    /* インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築 */
    if ((socd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    /* サーバのアドレス(IPアドレスとポート番号)の作成 */
    struct sockaddr_in s_address;
    bzero((char *) &s_address, sizeof(s_address));
    s_address.sin_family = AF_INET;
    s_address.sin_port = htons(port);
    bcopy((char *) hostent->h_addr, (char *) &s_address.sin_addr, hostent->h_length);
    /* サーバとの接続の確立 */
    if (connect(socd, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("connect");
        exit(1);
    }

    return socd;
}

void receive_file(int socd) /* サーバから受け取ったファイルの内容を表示する */
{
    char filename[MAX_FILE_NAME + 1];
    size_t filename_len;
    bool ack = false;
    bool is_shutdown = false;
    while (1) {
        /* ファイル名の入力 */
        printf("remote file name?: ");
        if (scanf("%s", filename) == EOF) { /* 終了処理時 ファイル名を終了ヘッダにしてサーバーに終了命令を送る*/
            strcpy(filename, CLOSE_HEADER);
            is_shutdown = true;
        }
        filename_len = strlen(filename);
        if (strncmp(filename, PUT_HEADER, PUT_HEADER_LEN) == 0) {
            printf("未実装\n");
            return;
        }
        /* ファイル名をソケットに書き込む */
        send(socd, filename, filename_len + 1, 0);
        if (is_shutdown) { /* 終了フラグがtrueの時クライアントを終了する。 */
            printf("close signal sent to the server.\n");
            return;
        };
        /* ファイルオープンに成功したかどうかのメッセージをソケットから読み込む */
        recv(socd, &ack, 1, 0);
        if (ack) {
            char buf[MAX_BUF_LEN] = {0};
            printf("received file %s \n", filename);
            printf("<- start of file ->\n");
            /* ソケットから読み込み標準出力に書き出す */
            ssize_t length;
            while ((length = recv(socd, buf, MAX_BUF_LEN, 0))) {
                if (buf[length - 1] == EOF) {
                    buf[0] = '\0';
                    fputs(buf, stdout);
                    break;
                } else {
                    buf[length] = '\0';
                    fputs(buf, stdout);
                }

            }
            printf("<- end of file ->\n");
        } else {
            fprintf(stderr, "File access error\n");
        }
    }
}