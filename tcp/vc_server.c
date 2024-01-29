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
#define S_TCP_PORT 5000  /* 本サーバが用いるポート番号 */
#define MAX_FILE_NAME 255
#define MAX_BUF_LEN 512
#define CLOSE_HEADER "CLOSE:"
#define PUT_HEADER "PUT:"

const size_t PUT_HEADER_LEN = strlen(PUT_HEADER);

int setup_vc_server(struct hostent *, u_short);

void send_file(int);

int main() {
    char s_hostname[MAX_HOST_NAME];
    /* サーバのホスト名とそのIPアドレス(をメンバに持つhostent構造体)を求める */
    gethostname(s_hostname, sizeof(s_hostname));
    struct hostent *s_hostent = gethostbyname(s_hostname);

    /* バーチャルサーキットサーバの初期設定 */
    int parent_socked = setup_vc_server(s_hostent, S_TCP_PORT);
    int child_socked;
    struct sockaddr_in c_address;
    socklen_t c_addrlen;
    pid_t cp_id;

    while (true) {
        /* 接続要求の受け入れ */
        c_addrlen = sizeof(c_address);
        if ((child_socked = accept(parent_socked, (struct sockaddr *) &c_address, &c_addrlen)) < 0) {
            perror("accept");
            exit(1);
        }
        printf("new session started\n");
        /* フォーク(並行サーバのサービス) */
        if ((cp_id = fork()) < 0) {
            perror("fork");
            exit(1);
        } else if (cp_id == 0) { /* 子プロセス */
            close(parent_socked);
            /* クライアントが要求するファイルの送信 */
            send_file(child_socked);
            close(child_socked);
            exit(0);
        } else close(child_socked);   /* 親プロセス */
    }
}

int setup_vc_server(struct hostent *hostent, u_short port) {
    int socked_id;
    /* インターネットドメインのSOCK_STREAM(TCP)型ソケットの構築 */
    if ((socked_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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
    if (bind(socked_id, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("bind");
        exit(1);
    }

    /* 接続要求待ち行列の長さを5とする */
    if (listen(socked_id, 5) < 0) {
        perror("listen");
        exit(1);
    }
    return socked_id;
}

void send_file(int socked_id) /* クライアントが要求するファイルを読み込みソケットに書き出す */
{
    while (true) {
        char filename[MAX_FILE_NAME + 1];
        /* クライアントから送られるファイル名をソケットから読み込む */
        recv(socked_id, filename, MAX_FILE_NAME + 1, 0);
        /* 終了命令を受け取った時の処理 */
        if (strcmp(filename, CLOSE_HEADER) == 0) {
            printf("session close request received\n");
            break;
        }
        if (strncmp(filename, PUT_HEADER, PUT_HEADER_LEN) == 0) {
            memmove(filename, &filename[PUT_HEADER_LEN], strlen(filename));
            bool ack = true;
            send(socked_id, &ack, 1, 0);
        }
        /* ファイルを読み出し専用にオープンする */
        FILE *fd;
        bool ack;
        if ((fd = fopen(filename, "r")) != NULL) { /* ファイルオープンに成功した場合 */
            /* オープン成功メッセージを送る */
            ack = true;
            send(socked_id, &ack, 1, 0);
            /* ファイルから1行読み込みソケットに書き出すことをEOFを読むまで繰り返す */
            printf("begin sending file %s\n", filename);
            char buf[MAX_BUF_LEN];
            while (fgets(buf, MAX_BUF_LEN, fd)) {
                send(socked_id, buf, strlen(buf), 0);
            }
            buf[0] = EOF;
            buf[1] = '\0';
            send(socked_id, buf, strlen(buf), 0); /* EOFを送りファイル送信が終わったことを伝える */
            printf("sent file %s\n", filename);
            fclose(fd);
            printf("closed file %s\n", filename);
        } else {                                    /* ファイルオープンに失敗した場合 */
            /* オープン失敗メッセージを送る */
            ack = false;
            send(socked_id, &ack, 1, 0);
        }
    }
}
