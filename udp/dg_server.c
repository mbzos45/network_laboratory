/* コネクションレスの簡単なデータ検索サーバ(dg_server.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <string.h>
#include <unistd.h>

#define  MAX_HOSTNAME    64
#define  S_UDP_PORT    (u_short)5000  /* 本サーバが用いるポート番号 */
#define  MAX_KEY_LEN    128
#define  MAX_DATA_LEN    256

#define SPLIT_SYMBOL ","
#define GET_HEADER "GET:"
#define POST_HEADER "POST:"

int setup_dgserver(struct hostent *, u_short);

void db_search(int);

int main() {
    int socd;
    char s_hostname[MAX_HOSTNAME];
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

void db_search(int socd) /* クライアントのデータ検索要求を処理する */
{
    struct sockaddr_in c_address;
    int i;
    char keys[MAX_KEY_LEN + 1] = {0}, data[MAX_DATA_LEN + 1] = {0}, removeHeaderKeys[
            MAX_KEY_LEN + 1] = {0}, postTargetName[MAX_KEY_LEN] = {0};
    const int GET_HEADER_LEN = strlen(GET_HEADER), POST_HEADER_LEN = strlen(POST_HEADER);
    size_t keysLen, dataLen, headID = 0;
    socklen_t c_addrlen;
    static char *db[11] = {"amano-taro", "0426-91-9418", "ishida-jiro", "0426-91-9872",
                           "ueda-saburo", "0426-91-9265", "ema-shiro", "0426-91-7254",
                           "ooishi-goro", "0426-91-9618", NULL};
    char **dbp;
    while (1) {
        /* キーをソケットから読み込む */
        c_addrlen = sizeof(c_address);
        if ((keysLen = recvfrom(socd, keys, MAX_KEY_LEN, 0, (struct sockaddr *) &c_address, &c_addrlen)) < 0) {
            perror("recvfrom");
            exit(1);
        }
        keys[keysLen] = '\0';
        printf("Received keys> %s\n", keys);
        if (strncmp(keys, GET_HEADER, GET_HEADER_LEN) == 0) {
            strcat(removeHeaderKeys, &keys[GET_HEADER_LEN + 1]);
            /* キーを用いてデータ検索 */
            for (i = 0; i < keysLen; ++i) {
                if (removeHeaderKeys[i] == ',') {
                    removeHeaderKeys[i] = '\0';
                }
            }
            while (headID < keysLen - GET_HEADER_LEN - 1) {
                dbp = db;
                while (*dbp) {
                    if (strcmp(&removeHeaderKeys[headID], *dbp) == 0) {
                        strcat(data, SPLIT_SYMBOL);
                        strcat(data, *(++dbp));
                        headID = strlen(data) + 1;
                        break;
                    }
                    dbp += 2;
                }
                if (*dbp == NULL) strcpy(data, "No entry");
                break;
            }
        } else if (strncmp(keys, POST_HEADER, POST_HEADER_LEN) == 0) {
            strcat(removeHeaderKeys, &keys[GET_HEADER_LEN + 1]);
            for (i = 0; i < MAX_KEY_LEN; ++i) {
                if (removeHeaderKeys[i] == ',') {
                    postTargetName[i] = '\0';
                    break;
                }
                postTargetName[i] = removeHeaderKeys[i];
            }
            if (i != MAX_KEY_LEN) {
                dbp = db;
                while (*dbp) {
                    if (strcmp(postTargetName, *dbp) == 0) {
                        strcpy(data, *(++dbp));
                        strcpy(*(dbp), &removeHeaderKeys[i + 1]);
                        break;
                    }
                    dbp += 2;
                }
                if (*dbp == NULL) strcpy(data, "No entry");
            }
        } else
            strcpy(data, "Bad Header");
        dataLen = strlen(data);
        if (sendto(socd, data, dataLen, 0, (struct sockaddr *) &c_address, c_addrlen) != dataLen) {
            fprintf(stderr, "datagram error\n");
            exit(1);
        }
        printf("Sent data> %s\n", data);
        memset(data, '\0', sizeof(data) / sizeof(char));
        memset(removeHeaderKeys, '\0', sizeof(removeHeaderKeys) / sizeof(char));
    }
}
