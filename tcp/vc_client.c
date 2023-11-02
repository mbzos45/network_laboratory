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
#define     S_TCP_PORT    5000
#define  MAX_FILE_NAME    255
#define     MAX_BUF_LEN    512
#define CLOSE_HEADER "CLOSE:"
#define PUT_HEADER "PUT:"

const size_t PUT_HEADER_LEN = strlen(PUT_HEADER);

int setup_vc_client(struct hostent *, u_short);

void receive_file(int);

int main() {
    int socked_id;
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
    socked_id = setup_vc_client(s_hostent, S_TCP_PORT);

    /* サーバにファイルを要求し受信したファイルの内容を標準出力に出力 */
    receive_file(socked_id);
    printf("client will close.\n");
    close(socked_id);
    return 0;
}

int setup_vc_client(struct hostent *hostent, u_short port) {
    int socked_id;
    /* インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築 */
    if ((socked_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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
    if (connect(socked_id, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("connect");
        exit(1);
    }

    return socked_id;
}

void receive_file(int socked_id) /* サーバから受け取ったファイルの内容を表示する */
{
    char inputted_str[MAX_FILE_NAME + 1];
    while (1) {
        /* ファイル名の入力 */
        printf("remote file name?: ");
        if (scanf("%s", inputted_str) == EOF) { /* 終了処理時 ファイル名を終了ヘッダにしてサーバーに終了命令を送る*/
            send(socked_id, CLOSE_HEADER, strlen(CLOSE_HEADER) + 1, 0);
            printf("close signal sent to the server.\n");
            break;
        }
        char *filenames[MAX_FILE_NAME] = {NULL};
        filenames[0] = strtok(inputted_str, ",");
        int file_num = 1;
        for (int i = 1; i < MAX_FILE_NAME; ++i) {
            if ((filenames[i] = strtok(NULL, ",")) == NULL) break;
            ++file_num;
        }
        for (int i = 0; i < file_num; ++i) {
            /* ファイル名をソケットに書き込む */
            send(socked_id, filenames[i], strlen(filenames[i]) + 1, 0);
            /* ファイルオープンに成功したかどうかのメッセージをソケットから読み込む */
            bool ack;
            recv(socked_id, &ack, 1, 0);
            if (ack) {
                char buf[MAX_BUF_LEN] = {0};
                printf("received file %s \n", filenames[i]);
                printf("<- start of file ->\n");
                /* ソケットから読み込み標準出力に書き出す */
                ssize_t length;
                while ((length = recv(socked_id, buf, MAX_BUF_LEN, 0))) {
                    if (buf[length - 1] == EOF) {
                        buf[length - 1] = '\0';
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
}
