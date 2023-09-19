/* コネクション型の簡単なリモートファイル表示サーバ(vc_server.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <errno.h>
#include <string.h>

#define  MAXHOSTNAME    64
#define  S_TCP_PORT    (u_short)5000  /* 本サーバが用いるポート番号 */
#define  MAXFILENAME    255
#define  MAXBUFLEN    512
#define  ERR        0 /* ファイルオープン失敗 */
#define     OK        1 /*                 成功 */

int setup_vcserver(struct hostent *, u_short);
void send_file(int);

main() {
    int socd, socd1;
    char s_hostname[MAXHOSTNAME];
    struct hostent *s_hostent;
    struct sockaddr_in c_address;
    int c_addrlen, cpid;

    /* サーバのホスト名とそのIPアドレス(をメンバに持つhostent構造体)を求める */
    gethostname(s_hostname, sizeof(s_hostname));
    s_hostent = gethostbyname(s_hostname);

    /* バーチャルサーキットサーバの初期設定 */
    socd = setup_vcserver(s_hostent, S_TCP_PORT);

    while (1) {
        /* 接続要求の受け入れ */
        c_addrlen = sizeof(c_address);
        if ((socd1 = accept(socd, (struct sockaddr *) &c_address, &c_addrlen)) < 0) {
            perror("accept");
            exit(1);
        }
        /* フォーク(並行サーバのサービス) */
        if ((cpid = fork()) < 0) {
            perror("fork");
            exit(1);
        }
        else if (cpid == 0) { /* 子プロセス */
            close(socd);

            /* クライアントが要求するファイルの送信 */
            send_file(socd1);

            close(socd1);
            exit(0);
        } else close(socd1);   /* 親プロセス */
    }
}

int setup_vcserver(struct hostent *hostent, u_short port) {
    int socd;
    struct sockaddr_in s_address;

    /* インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築 */
    if ((socd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    /* アドレス(IPアドレスとポート番号)の作成 */
    bzero((char *) &s_address, sizeof(s_address));
    s_address.sin_family = AF_INET;
    s_address.sin_port = htons(port);
    bcopy((char *) hostent->h_addr, (char *) &s_address.sin_addr, hostent->h_length);

    /* アドレスのソケットへの割り当て */
    if (bind(socd, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("bind");
        exit(1);
    }

    /* 接続要求待ち行列の長さを5とする */
    if (listen(socd, 5) < 0) {
        perror("listen");
        exit(1);
    }

    return socd;
}

void send_file(int socd) /* クライアントが要求するファイルを読み込みソケットに書き出す */
{
    char filename[MAXFILENAME + 1];
    FILE *fd;
    char ack;
    char buf[MAXBUFLEN];

    /* クライアントから送られるファイル名をソケットから読み込む */
    recv(socd, filename, MAXFILENAME + 1, 0);
    /* ファイルを読み出し専用にオープンする */
    if ((fd = fopen(filename, "r")) != NULL) { /* ファイルオープンに成功した場合 */
        /* オープン成功メッセージを送る */
        ack = OK;
        send(socd, &ack, 1, 0);
        /* ファイルから1行読み込みソケットに書き出すことをEOFを読むまで繰り返す */
        printf("ファイル %s を送信\n", filename);
        while (fgets(buf, MAXBUFLEN, fd)) {
            send(socd, buf, strlen(buf), 0);
        }
        close(fd);
    } else {                                    /* ファイルオープンに失敗した場合 */
        /* オープン失敗メッセージを送る */
        ack = ERR;
        send(socd, &ack, 1, 0);
    }
}
