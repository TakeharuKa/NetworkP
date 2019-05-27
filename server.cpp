#include <iostream>
#include <cstring>
#include <vector>

#include <dirent.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <netinet/tcp.h>

#define VERSION 100

using namespace std;

typedef void Sigfunc(int);

const int SERV_PORT = 9877;
const int MAX_SIZE = 4096;
const int BACKLOG = 20;
const int DATA_SIZE = 1000;

const int PACKAGE_SIZE = 1024;
const int LISTENQ = 1024;
//const uint32_t MAX_FILE_SIZE = 10000000;
//
const int CONTENT = 18;

struct Request {
	char version;
	char type;
	char content[CONTENT];

	uint32_t packetCount;
	uint32_t checkSum;
}ERRORT, ERRORF, ERRORI, SUSSESS;

struct dataPacket {
	char data[DATA_SIZE];

	uint32_t order;
	uint32_t checkSum;
};

int readn(int fd, void *vptr, int n)
{
    char* ptr = (char*) vptr;

    int now = 0, nread;
    while (now < n){
        if ( (nread = read(fd, ptr + now, n - now)) < 0){

            if (errno == EINTR)
                nread = 0;
            else
                return -1;

        } else if (nread == 0)
            break;

		now += nread;
    }
    return now;
}

int writen(int fd, void *vptr, int n)
{
	char* ptr = (char*)vptr;

	int now = 0, nwrite;
	while (now < n) {
		if ((nwrite = write(fd, ptr + now, n - now)) <= 0) {
			if (nwrite < 0 && errno == EINTR)
				nwrite = 0;
			else
				return -1;
		}
		now += nwrite;
	}

	return now;
}

void Writen(int fd, void *vptr, int n)
{
	if (writen(fd, vptr, n) == -1) {
		cerr << "Write error" << endl;
		close(fd);
		pthread_exit (NULL);
	}
}

int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr) 
{
	int n;

	bool flag;
	do
	{
		flag = 0;
		if ((n = accept(fd, sa, salenptr)) < 0) {

#ifdef	EPROTO
			if (errno == EPROTO || errno == ECONNABORTED)
#else
			if (errno == ECONNABORTED)
#endif
			flag = 1;

			else {
				cerr << "accept error" << endl;

				return -1;
			}
		}
	} while(flag);

	return n;
}

void sig_chld(int signo) {
	int stat;
	pid_t pid;
	pid = wait(&stat);

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
		cerr << "child " << pid << "terminated." << endl;

	return;
}

Sigfunc* Signal(int signo, Sigfunc *func) {
	Sigfunc	*sigfunc;

	if ((sigfunc = signal(signo, func)) == SIG_ERR) {
		printf("signal error\n");
		exit(1);
	}
	return(sigfunc);
}

uint32_t calcCheckSum(char* data, int length)
{
	uint32_t ret = 0;
	for(int i = 0; i < length; i ++) {
		ret += *(data + i);
	}
	return ret;
}

void *doit(void *_nowfd)
{
	cerr << "Thread Created." << endl;

	pthread_detach(pthread_self());

	int nowfd = *((int *)_nowfd);

	struct Request request;

	int nread;


	if ( (nread = readn(nowfd, (void *)&request, sizeof(request))) <= 0 || 
			ntohl(request.checkSum) != calcCheckSum((char *)&request, sizeof(request) - 4) ||
			request.version != VERSION || (request.type != 'U' && request.type != 'D')) {

		cerr << request.type << " " << request.version << " " << request.content << " " << " " << request.packetCount << " " << request.checkSum << endl;
		cerr << "Request invalid" << endl;
		Writen(nowfd, (void*) &ERRORI, sizeof(ERRORI));
		close(nowfd);
		pthread_exit (NULL);
	}

	request.checkSum = ntohl(request.checkSum);
	request.packetCount = ntohl(request.packetCount);

	cerr << request.type << " " << request.version << " " << request.content << " " << " " << request.packetCount << " " << request.checkSum << endl;

	char pathbuf[100];
	char dir[10] = "/files/";

	Request ret;
	string filePath;

	FILE *fp;
	dataPacket data;

	char content_local[CONTENT + 1];
	switch(request.type)
	{
		case 'U':

			cerr << "Receiving File" << endl;

			for(int i = 0; i < CONTENT; i ++)
				content_local[i] = request.content[i];

			content_local[CONTENT] = '\0';
			getcwd(pathbuf, sizeof(pathbuf));
			strcat(pathbuf, dir);
			strcat(pathbuf, content_local);

			cerr << pathbuf << endl;

			if( (access( pathbuf, 0 )) != -1 )
			{
				cerr << "File exists" << endl;
				Writen(nowfd, (void*) &ERRORF, sizeof(ERRORF));

				close(nowfd);
				pthread_exit (NULL);
			}

			fp = fopen(pathbuf, "ab");

			for (uint32_t i = 0; i < request.packetCount; i ++){
				bzero((void *)&data.data, sizeof(data.data));
				if ( readn(nowfd, (void *)&data, sizeof(data)) <= 0) {
					cerr << "Read data error" << endl;
					Writen(nowfd, (void*) &ERRORI, sizeof(ERRORI));

					fclose(fp);
					close(nowfd);
					pthread_exit (NULL);
				}

				data.order = ntohl(data.order);
				data.checkSum = ntohl(data.checkSum);

				if (data.checkSum != calcCheckSum((char *)&data, sizeof(data) - 4) ||
						data.order != i) {

					cerr << "Request invalid" << endl;
					Writen(nowfd, (void*) &ERRORI, sizeof(ERRORI));

					close(nowfd);
					pthread_exit (NULL);
				}

				fwrite(data.data, sizeof(char), DATA_SIZE, fp);
			}

			cerr << "Received: " << filePath << " successfully." << endl;

			Writen(nowfd, (void*) &SUSSESS, sizeof(SUSSESS));

			fclose(fp);
			close(nowfd);
			break;

		case 'D':

			cerr << "Sending File" << endl;

			for(int i = 0; i < CONTENT; i ++)
				content_local[i] = request.content[i];
			content_local[CONTENT] = '\0';
			getcwd(pathbuf, sizeof(pathbuf));
			strcat(pathbuf, dir);
			strcat(pathbuf, content_local);

			if( (access( pathbuf, 0 )) != 0 || access ( pathbuf, 4 ) != 0 )
			{
				cerr << "File not exist" << endl;
				Writen(nowfd, (void*) &ERRORF, sizeof(ERRORF));

				close(nowfd);
				pthread_exit (NULL);
			}

			fp = fopen(pathbuf, "rb");

			fseek(fp, 0, SEEK_END);
			uint32_t size=ftell(fp);
			uint32_t size_packet = (size - 1) / DATA_SIZE + 1;
			fseek(fp, 0, SEEK_SET);

			ret.type = 'R';
			ret.version = 'd';
			bzero((void *)&ret.content, sizeof(ret.content));
			ret.packetCount = htonl(size_packet);
			ret.checkSum = calcCheckSum((char *)&ret, sizeof(ret) - 4);

			Writen(nowfd, (void *) &ret, sizeof(ret));

			for (uint32_t i = 0; i < size_packet; i ++){
				data.order = i;
				fread(data.data, DATA_SIZE, 1, fp);

				data.order = ntohl(data.order);
				data.checkSum = ntohl(calcCheckSum((char *)&data, sizeof(data) - 4));

				Writen(nowfd, (void*) &data, sizeof(data));
			}

			fclose(fp);
			cerr << "Send: " << filePath << " successfully." << endl;

			bzero((void *)&request.content, sizeof(request.content));

			if ( readn(nowfd, (void *)&request, sizeof(request)) <= 0 || 
					ntohl(request.checkSum) != calcCheckSum((char *)&request, sizeof(request) - 4) ||
					request.version != VERSION || (request.type != 'S')) {

				cerr << "Send Failed." << endl;
				Writen(nowfd, (void*) &ERRORI, sizeof(ERRORI));

				close(nowfd);
				pthread_exit (NULL);
			}

			cerr << "Completed." << endl;
			close(nowfd);
			break;
	}
}

int main()
{
	ERRORT.version = ERRORI.version = ERRORF.version = SUSSESS.version = VERSION;
	ERRORT.type = 'T';
	ERRORI.type = 'I';
	ERRORF.type = 'F';
	SUSSESS.type = 'S';

	ERRORT.checkSum = htonl(calcCheckSum((char *)&ERRORT, sizeof(ERRORT) - 4));
	ERRORI.checkSum = htonl(calcCheckSum((char *)&ERRORI, sizeof(ERRORI) - 4));
	ERRORF.checkSum = htonl(calcCheckSum((char *)&ERRORF, sizeof(ERRORF) - 4));
	SUSSESS.checkSum = htonl(calcCheckSum((char *)&SUSSESS, sizeof(SUSSESS) - 4));

	struct sockaddr_in cliaddr, servaddr;

	socklen_t clilen;

	int listenfd, connfd;
	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) 
	{
		cerr << "socket error" << endl;
		return -1;
	}

	//To avoid FIN_WAIT attack and avoid waiting longer than 2MSL
	//linger: FIN_WAIT
	//reuseaddr: waiting

	struct linger ling;
	ling.l_onoff = 1;
	ling.l_linger = 0;
	int optval1 = 1;
	int optval2 = 1;
	if((setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling))) < 0 ||
			setsockopt(listenfd, SOL_SOCKET,SO_REUSEADDR, &optval1, sizeof(optval1)) < 0 ||
			setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, &optval2, sizeof(optval2)) < 0)
	{
		cerr << "socket setting error" << endl;
		return -1;
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);

	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	Signal(SIGCHLD, sig_chld);
	Signal(SIGPIPE, SIG_IGN);

	listen(listenfd, BACKLOG);

	pthread_t tid;
	int thread_now = 0;

	Request ret;
	while (1) {

		clilen = sizeof(cliaddr);

		if ((connfd = Accept(listenfd, (struct sockaddr *) &cliaddr, &clilen)) == -1) {
			continue;
		}

		//disable nagle algorithm & enable keepalive
		struct timeval timeout1 = {0, 100000};
		struct timeval timeout2 = {0, 100000};
		int optval = 1;
		if(setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0 || 
				setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout1, sizeof(timeout1)) < 0 ||
				setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout1, sizeof(timeout1)) < 0)
		{
			Writen(connfd, (void*) &ERRORT, sizeof(ERRORT));

			close(connfd);
			cerr << "socket setting error.";
			continue;
		}

		int pp = pthread_create(&tid, NULL, doit, &connfd);

		if (pp != 0) {
			Writen(connfd, (void*) &ERRORT, sizeof(ERRORT));

			close(connfd);
			perror("Thread create failed.");
			continue;
		}
	}
}
