/* コネクション型の簡単なリモートファイル表示サーバ(vc_server.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_HOST_NAME 64
#define S_TCP_PORT 7000  /* 本サーバが用いるポート番号 */
#define MAX_FILE_NAME 255
#define MAX_BUF_LEN 512
#define CLOSE_HEADER "shutdown:"

int setup_vcserver(struct hostent *, u_short);

void send_file(int);

int main() {
    int socd, socd1;
    char s_hostname[MAX_HOST_NAME];
    struct hostent *s_hostent;
    struct sockaddr_in c_address;
    pid_t cpid;
    socklen_t c_addrlen;

    /* サーバのホスト名とそのIPアドレス(をメンバに持つhostent構造体)を求める */
    gethostname(s_hostname, sizeof(s_hostname));
    s_hostent = gethostbyname(s_hostname);

    /* バーチャルサーキットサーバの初期設定 */
    socd = setup_vcserver(s_hostent, S_TCP_PORT);

    while (true) {
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
        } else if (cpid == 0) { /* 子プロセス */
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
    /* インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築 */
    if ((socd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in s_address;
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
    while (true) {
        char recv_str[MAX_FILE_NAME + 1];
        /* クライアントから送られるファイル名をソケットから読み込む */
        recv(socd, recv_str, MAX_FILE_NAME + 1, 0);
        /* 終了命令を受け取った時の処理 */
        if (strcmp(recv_str, CLOSE_HEADER) == 0) {
            printf("session close request received\n");
            break;
        }
        /* ファイルを読み出し専用にオープンする */
        FILE *fd;
        bool ack;
        if ((fd = fopen(recv_str, "r")) != NULL) { /* ファイルオープンに成功した場合 */
            /* オープン成功メッセージを送る */
            ack = true;
            send(socd, &ack, 1, 0);
            /* ファイルから1行読み込みソケットに書き出すことをEOFを読むまで繰り返す */
            printf("begin sending file %s\n", recv_str);
            char buf[MAX_BUF_LEN];
            while (fgets(buf, MAX_BUF_LEN, fd)) {
                send(socd, buf, strlen(buf), 0);
            }
            buf[0] = EOF;
            buf[1] = '\0';
            send(socd, buf, strlen(buf), 0); /* EOFを送りファイル送信が終わったことを伝える */
            printf("sent file %s\n", recv_str);
            fclose(fd);
            printf("closed file %s\n", recv_str);
        } else {                                    /* ファイルオープンに失敗した場合 */
            /* オープン失敗メッセージを送る */
            ack = false;
            send(socd, &ack, 1, 0);
        }
    }
}
