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

const int PACKAGE_SIZE = 1024;
const int LISTENQ = 1024;
//const uint32_t MAX_FILE_SIZE = 10000000;

struct Request {
	char version;
	char type;
	char content[18];

	uint32_t packetCount;
	uint32_t checkSum;
}failed;

struct dataPacket {
	char data[1000];

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

int Readn(int fd, void *vptr, int n)
{
	int ret;
	if((ret = readn(fd, vptr, n)) == -1) {
		cerr << "Read error" << endl;
		close(fd);
		pthread_exit (NULL);
	}
	return ret;
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

vector<string> getFiles(string cate_dir)
{
	vector<string> files;

	DIR *dir;
	struct dirent *ptr;
	char base[1000];

	if ((dir=opendir(cate_dir.c_str())) == NULL)
	{
		perror("Open dir error...");
		exit(1);
	}

	while ((ptr=readdir(dir)) != NULL)
	{
		if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0) 
			continue;

		else if(ptr->d_type == 8)
			files.push_back(ptr->d_name);

		else if(ptr->d_type == 10)
			continue;

		else if(ptr->d_type == 4)
			files.push_back(ptr->d_name);

	}
	closedir(dir);

	sort(files.begin(), files.end());
	for(int i = 0; i < files.size(); i ++)
		cerr << files[i] << endl;

	return files;
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

void so_reuseaddr (int s)
{
	//To avoid waiting longer than 2MSL
	int optval = 1;

	if((setsockopt(s, SOL_SOCKET,SO_REUSEADDR, &optval, sizeof(optval))) < 0)
	{
		cerr << "set soreuseaddr error" << endl;
		exit(1);
	}
}

void tcp_nodelay (int s)
{
	//disable nagle algorithm
	int optval = 1;

	if((setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval))) < 0)
	{
		cerr << "set tcpnodelay error" << endl;
		exit(1);
	}
}

void so_linger (int s)
{
	//To avoid FIN_WAIT attack
	struct linger ling;

	ling.l_onoff = 1;
	ling.l_linger = 0;
	if((setsockopt(s, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling))) < 0)
	{
		cerr << "set solinger error" << endl;
		exit(1);
	}
}

void so_keepalive (int s)
{
	int optval = 1;
	if((setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval))) < 0)
	{
		cerr << "set slkeepalive error" << endl;
		exit(1);
	}
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

	if ((sigfunc = signal(signo, func)) == SIG_ERR)
		printf("signal error\n");
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

void checkCheckSum(char *data, int length, uint32_t checkSum, int fd) {
	uint32_t realcheckSum = calcCheckSum(data, length);

	cerr << "comp: " << checkSum << " " << realcheckSum << endl;
	if(checkSum != realcheckSum) {
		cerr << "Checksum invalid" << endl;
		close(fd);
		pthread_exit (NULL);
	}
}

void *doit(void *_nowfd)
{
	pthread_detach(pthread_self());

	cerr << "doingit" << endl;
	int nowfd = *((int *)_nowfd);

	struct timeval timeout1 = {0, 100000}; //timeout = 1s
	if(setsockopt(nowfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout1, sizeof(timeout1)) < 0) {
		cerr << "Set timeout error" << endl;
		close(nowfd);
		pthread_exit (NULL);
	}

	struct Request request;
	cerr << sizeof(request) << endl;

	Readn(nowfd, (void *)&request, sizeof(request));

	request.checkSum = ntohl(request.checkSum);
	
	checkCheckSum((char *) &request, sizeof(request) - 4, request.checkSum, nowfd);

	request.packetCount = ntohl(request.packetCount);

	cerr << request.type << " " << request.version << " " << request.content << " " << " " << request.packetCount << " " << request.checkSum << endl;

	if (request.version != VERSION || (request.type != 'U' && request.type != 'D')) {
		cerr << "Request invalid" << endl;
		close(nowfd);
		pthread_exit (NULL);
	}


	switch(request.type)
	{
		case 'U':

			cerr << "Receiving File" << endl;
			char pathbuf[100];

			string filePath = string(getcwd(pathbuf, sizeof(pathbuf))) + "/" + request.content + "0";
			FILE *fp = fopen(filePath.c_str(), "ab");

			struct dataPacket data;
			for (uint32_t i = 0; i < request.packetCount; i ++){
				//bzero((void *)&data, sizeof(data));
				int nread = Readn(nowfd, (void *) &data, sizeof(data));
				//cerr << "checksum: " << data.checkSum << endl;

				data.order = ntohl(data.order);
				data.checkSum = ntohl(data.checkSum);
				checkCheckSum((char*) &data, sizeof(data) - 4, data.checkSum, nowfd);

				cerr << "data: " << data.data << endl;
				fwrite(data.data, sizeof(char), 1000, fp);
			}

			cerr << "Received: " << filePath << " successfully." << endl;

			Request ret;
			ret.version = VERSION;
			ret.type = 'S';
			bzero((void *)&ret.content, sizeof(ret.content));
			ret.packetCount = 0;
			ret.checkSum = calcCheckSum((char *)&ret, sizeof(ret) - 4);

			Writen(nowfd, (void*) &ret, sizeof(ret));

			fclose(fp);
			close(nowfd);
	}
}

int main()
{
	//getFiles("./AAA");
	struct sockaddr_in cliaddr, servaddr;

	socklen_t clilen;

	int listenfd, connfd;
	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) 
	{
		cerr << "socket error" << endl;
		return -1;
	}

	so_reuseaddr(listenfd);
	so_linger(listenfd);

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);

	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	Signal(SIGCHLD, sig_chld);
	Signal(SIGPIPE, SIG_IGN);

	listen(listenfd, BACKLOG);

	/*
	   pthread_t pid[222];
	   for (int i = 0; i < 22; i ++)
	   tid[i] = (pthread_t) - 1;
	   */
	const int THREAD_MAX = 1000;

	pthread_t tid;
	int thread_now = 0;

	while (1) {

		clilen = sizeof(cliaddr);
		if ((connfd = Accept(listenfd, (struct sockaddr *) &cliaddr, &clilen)) == -1) {
			continue;
		}

		tcp_nodelay(connfd);

		int pp = pthread_create(&tid, NULL, doit, &connfd);

		if (pp != 0) {
			perror("Thread create failed.");
			continue;
		}
	}
}
