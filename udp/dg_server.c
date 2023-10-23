/* コネクションレスの簡単なデータ検索サーバ(dg_server.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define  MAXHOSTNAME    64
#define  S_UDP_PORT    (u_short)5000  /* 本サーバが用いるポート番号 */
#define  MAXKEYLEN    128
#define  MAXDATALEN    256

#define GET_HEADER "GET:"
#define POST_HEADER "POST:"
#define SHUTDOWN_HEADER "shutdown"
#define SHUTDOWN_MESSAGE "the server will shutdown now!"

int setup_dgserver(struct hostent *, u_short);

void db_search(int);

int main() {
    int socd;
    char s_hostname[MAXHOSTNAME];
    struct hostent *s_hostent;

    /* サーバのホスト名とそのInternetアドレス(をメンバに持つhostent構造体)を求める */
    gethostname(s_hostname, sizeof(s_hostname));
    s_hostent = gethostbyname(s_hostname);

    /* データグラムサーバの初期設定 */
    socd = setup_dgserver(s_hostent, S_UDP_PORT);

    /* クライアントからのデータ検索要求の処理 */
    db_search(socd);
}

int setup_dgserver(struct hostent *hostent, u_short port) {
    int socd;
    struct sockaddr_in s_address;

    /* インターネットドメインのSOCK_DGRAM(UDP)型ソケットの構築 */
    if ((socd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    /* アドレス(Internetアドレスとポート番号)の作成 */
    bzero((char *) &s_address, sizeof(s_address));
    s_address.sin_family = AF_INET;
    s_address.sin_port = htons(port);
    bcopy((char *) hostent->h_addr, (char *) &s_address.sin_addr, hostent->h_length);

    /* アドレスのソケットへの割り当て */
    if (bind(socd, (struct sockaddr *) &s_address, sizeof(s_address)) < 0) {
        perror("bind");
        exit(1);
    }

    return socd;
}

void db_search(int socd) /* クライアントがデータ検索要求を処理する */
{
    struct sockaddr_in c_address;
    socklen_t c_addrlen;
    char key[MAXKEYLEN + 1] = {0}, data[MAXDATALEN + 1] = {0}, *token, *newNumber;
    ssize_t keyLen;
    size_t dataLen;
    int i, error_num = 0, is_shutdown = 0;
    const size_t GET_HEADER_LEN = strlen(GET_HEADER), POST_HEADER_LEN = strlen(POST_HEADER);
    static char *db[11];
    for (i = 0; i <= 9; i++) db[i] = (char *) malloc(32);
    strcpy(db[0], "amano-taro");
    strcpy(db[1], "0426-91-9418");
    strcpy(db[2], "ishida-jiro");
    strcpy(db[3], "0426-91-9872");
    strcpy(db[4], "ueda-saburo");
    strcpy(db[5], "0426-91-9265");
    strcpy(db[6], "ema-shiro");
    strcpy(db[7], "0426-91-7254");
    strcpy(db[8], "ooishi-goro");
    strcpy(db[9], "0426-91-9618");
    db[10] = NULL;
    char **dbp;

    while (1) {
        /* キーをソケットから読み込む */
        c_addrlen = sizeof(c_address);
        if ((keyLen = recvfrom(socd, key, MAXKEYLEN, 0, (struct sockaddr *) &c_address, &c_addrlen)) < 0) {
            perror("recvfrom");
            error_num = EPERM;
            break;
        }
        key[keyLen] = '\0';
        printf("Received key> %s\n", key);
        if (strncmp(key, GET_HEADER, GET_HEADER_LEN) == 0) { /* GET:から始まる時 */
            memmove(key, key + GET_HEADER_LEN, keyLen); /* GET_HEADERを消去する */
            token = strtok(key, ",");
            do {
                dbp = db;
                while (*dbp) {
                    if (strcmp(token, *dbp) == 0) {
                        strcat(data, *(++dbp));
                        strcat(data, ",");
                        break; /* dataに値と , を追加する */
                    }
                    dbp += 2;
                }
                if (*dbp == NULL) strcpy(data, "No entry"); /* 文末の,を消去 */
            } while ((token = strtok(NULL, ",")));
            if (data[strlen(data) - 1] == ',') data[strlen(data) - 1] = '\0';
        } else if (strncmp(key, POST_HEADER, POST_HEADER_LEN) == 0) { /* POSTの時 */
            memmove(key, key + POST_HEADER_LEN, keyLen); /* POST_HEADER消去*/
            token = strtok(key, ",");
            newNumber = strtok(NULL, ",");
            dbp = db;
            while (*dbp) {
                if (strcmp(token, *dbp) == 0) {
                    strcpy(data, *(++dbp));
                    strcpy(*dbp, newNumber);
                    break;
                }
                dbp += 2;
            }
            if (*dbp == NULL) strcpy(data, "No entry");
        } else if (strcmp(key, SHUTDOWN_HEADER) == 0) { /* SHUTDOWN_HEADERの時 */
            is_shutdown = 1;
            strcpy(data, SHUTDOWN_MESSAGE);
        } else
            strcpy(data, "Bad Header");
        /* 検索したデータをソケットに書き込む */
        dataLen = strlen(data);
        if (sendto(socd, data, dataLen, 0, (struct sockaddr *) &c_address, c_addrlen) != dataLen) {
            fprintf(stderr, "datagram error\n");
            error_num = EPERM;
            break;
        }
        printf("Sent data> %s\n", data);
        if (is_shutdown) break; /* シャットダウンするとき無限ループを抜けてメモリ解放を行う */
        data[0] = '\0';
    }
    for (i = 0; i < 11; ++i) free(db[i]);
    exit(error_num);
}