/* コネクション型の簡単なリモートファイル表示クライアント(vc_client.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define  MAXHOSTNAME    64
#define     S_TCP_PORT    (u_short)5000
#define  MAXFILENAME    255
#define     MAXBUFLEN    512
#define     ERR        0 /* ファイルオープン失敗 */
#define  OK        1 /*                 成功 */

int setup_vcclient(struct hostent *, u_short);

void receive_file(int);

int main() {
    int socd;
    char s_hostname[MAXHOSTNAME];
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

    close(socd);
    exit(0);
}

int setup_vcclient(struct hostent *hostent, u_short port) {
    int socd;
    struct sockaddr_in s_address;

    /* インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築 */
    if ((socd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    /* サーバのアドレス(IPアドレスとポート番号)の作成 */
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
    char filename[MAXFILENAME + 1];
    int filename_len;
    char ack;
    char buf[MAXBUFLEN];
    int length;

    /* ファイル名の入力 */
    printf("remote file name?: ");
    scanf("%s", filename);
    /* ファイル名をソケットに書き込む */
    filename_len = strlen(filename);
    send(socd, filename, filename_len + 1, 0);
    /* ファイルオープンに成功したかどうかのメッセージをソケットから読み込む */
    recv(socd, &ack, 1, 0);
    switch (ack) {
        case OK: /* ファイルオープンに成功した場合 */
            printf("ファイル %s を受信\n", filename);
            /* ソケットから読み込み標準出力に書き出す */
            while (length = recv(socd, buf, MAXBUFLEN, 0)) {
                buf[length] = '\0';
                fputs(buf, stdout);
            }
            break;
        case ERR: /* ファイルオープンに失敗した場合 */
            fprintf(stderr, "File access error\n");
            break;
    }
}
