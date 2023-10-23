/* コネクションレスの簡単なデータ検索クライアント(dg_client.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <string.h>
#include <unistd.h>

#define  MAX_HOSTNAME    64
#define     S_UDP_PORT    (u_short)5000
#define  MAX_KEY_LEN    128
#define     MAX_DATA_LEN    256
#define SHUTDOWN_MESSAGE "the server will shutdown now!"

int setup_dgclient(struct hostent *, u_short, struct sockaddr_in *, socklen_t *);

void remote_dbsearch(int, struct sockaddr_in *, socklen_t);

int main() {
    int socd;
    char s_hostname[MAX_HOSTNAME];
    struct hostent *s_hostent;
    struct sockaddr_in s_address;
    socklen_t s_addrlen;

    /* サーバのホスト名の入力 */
    printf("server host name?: ");
    scanf("%s", s_hostname);
    /* サーバホストのInternetアドレス(をメンバに持つhostent構造体)を求める */
    if ((s_hostent = gethostbyname(s_hostname)) == NULL) {
        fprintf(stderr, "server host does not exists\n");
        exit(1);
    }

    /* データグラムクライアントの初期設定 */
    socd = setup_dgclient(s_hostent, S_UDP_PORT, &s_address, &s_addrlen);

    /* リモートデータベース検索 */
    remote_dbsearch(socd, &s_address, s_addrlen);

    close(socd);
    exit(0);
}

int setup_dgclient(struct hostent *hostent, u_short port, struct sockaddr_in *s_addressp, socklen_t *s_addrlenp) {
    int socd;
    struct sockaddr_in c_address;

    /* インターネットドメインのSOCK_DGRAM(UDP)型ソケットの構築 */
    if ((socd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    /* サーバのアドレス(Internetアドレスとポート番号)の作成 */
    bzero((char *) s_addressp, sizeof(*s_addressp));
    s_addressp->sin_family = AF_INET;
    s_addressp->sin_port = htons(port);
    bcopy((char *) hostent->h_addr, (char *) &s_addressp->sin_addr, hostent->h_length);
    *s_addrlenp = sizeof(*s_addressp);

    /* クライアントのアドレス(Internetアドレスとポート番号)の作成 */
    bzero((char *) &c_address, sizeof(c_address));
    c_address.sin_family = AF_INET;
    c_address.sin_port = htons(0);               /* ポート番号の自動割り当て */
    c_address.sin_addr.s_addr = htonl(INADDR_ANY); /* Internetアドレスの自動割り当て */

    /* クライアントアドレスのソケットへの割り当て */
    if (bind(socd, (struct sockaddr *) &c_address, sizeof(c_address)) < 0) {
        perror("bind");
        exit(1);
    }

    return socd;
}

void remote_dbsearch(int socd, struct sockaddr_in *s_addressp, socklen_t s_addrlen) /* サーバにキーを送り検索結果(データ)を受け取る */
{
    char key[MAX_KEY_LEN + 1], data[MAX_DATA_LEN + 1];
    ssize_t datalen;
    size_t keylen;
    do {
        /* キーを標準入力から入力 */
        printf("key?: ");
        if (scanf("%s", key) == EOF) {
            return;
        }
        /* キーをソケットに書き込む */
        keylen = strlen(key);
        if (sendto(socd, key, keylen, 0, (struct sockaddr *) s_addressp, s_addrlen) != keylen) {
            fprintf(stderr, "datagram error\n");
            exit(1);
        }
        /* 検索データをソケットから読み込む */
        if ((datalen = recvfrom(socd, data, MAX_DATA_LEN, 0, NULL, &s_addrlen)) < 0) {
            perror("recvfrom");
            exit(1);
        }
        /* データを標準出力に出力 */
        data[datalen] = '\0';
        fputs("data: ", stdout);
        puts(data);
        printf("\n");
        if (strcmp(data, SHUTDOWN_MESSAGE) == 0) break;
    } while (1);
}
