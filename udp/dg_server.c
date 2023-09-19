/* コネクションレスの簡単なデータ検索サーバ(dg_server.c) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h> /* ソケットのための基本的なヘッダファイル      */
#include <netinet/in.h> /* インタネットドメインのためのヘッダファイル  */
#include <netdb.h>      /* gethostbyname()を用いるためのヘッダファイル */
#include <errno.h>
#include <string.h>
#include <unistd.h>
#define  MAXHOSTNAME	64
#define  S_UDP_PORT	(u_short)5000  /* 本サーバが用いるポート番号 */
#define  MAXKEYLEN	128
#define  MAXDATALEN	256
int setup_dgserver(struct hostent*, u_short);
void db_search(int);

int main()
{
	int	socd;
	char	s_hostname[MAXHOSTNAME];
	struct hostent	*s_hostent;

	/* サーバのホスト名とそのInternetアドレス(をメンバに持つhostent構造体)を求める */
	gethostname(s_hostname, sizeof(s_hostname));
	s_hostent = gethostbyname(s_hostname);

	/* データグラムサーバの初期設定 */
	socd = setup_dgserver(s_hostent, S_UDP_PORT);

	/* クライアントからのデータ検索要求の処理 */
	db_search(socd);
}

int setup_dgserver(struct hostent *hostent, u_short port)
{
	int	socd;
	struct sockaddr_in	s_address;

	/* インターネットドメインのSOCK_DGRAM(UDP)型ソケットの構築 */
	if((socd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { perror("socket");exit(1); }

	/* アドレス(Internetアドレスとポート番号)の作成 */
	bzero((char *)&s_address, sizeof(s_address));
	s_address.sin_family = AF_INET;
	s_address.sin_port = htons(port);
	bcopy((char *)hostent->h_addr, (char *)&s_address.sin_addr, hostent->h_length);

	/* アドレスのソケットへの割り当て */
	if(bind(socd, (struct sockaddr *)&s_address, sizeof(s_address)) < 0) { perror("bind");exit(1); }

	return socd;
}

void db_search(int socd) /* クライアントがデータ検索要求を処理する */
{
	struct sockaddr_in	c_address;
	int	c_addrlen;
	char	key[MAXKEYLEN+1], data[MAXDATALEN+1];
	int	keylen, datalen;
	static char *db[] = {"amano-taro","0426-91-9418","ishida-jiro","0426-91-9872",
                             "ueda-saburo","0426-91-9265","ema-shiro","0426-91-7254",
                             "ooishi-goro","0426-91-9618",NULL};
	char	**dbp;

	while(1) {
		/* キーをソケットから読み込む */
		c_addrlen = sizeof(c_address);
		if((keylen = recvfrom(socd, key, MAXKEYLEN, 0, (struct sockaddr *)&c_address, &c_addrlen)) < 0) {
			perror("recvfrom");
			exit(1);
		}
		key[keylen] = '\0';
		printf("Received key> %s\n",key);
		/* キーを用いてデータ検索 */
		dbp = db;
		while(*dbp) {
			if(strcmp(key, *dbp) == 0) {
				strcpy(data, *(++dbp));
				break;
			}
			dbp += 2;
		}
		if(*dbp == NULL) strcpy(data, "No entry");
	
		/* 検索したデータをソケットに書き込む */
		datalen = strlen(data);
		if(sendto(socd, data, datalen, 0, (struct sockaddr *)&c_address, c_addrlen) != datalen) {
			fprintf(stderr, "datagram error\n"); 
			exit(1);
		}
		printf("Sent data> %s\n", data);
	}
}
