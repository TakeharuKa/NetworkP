#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
using namespace std;

#define MAX_SIZE 4096
#define DEFAULT_PORT 9877
#define BACKLOG 20

struct Chat {
	char version;
	char type;
	char filename[18];

	uint32_t packetCount;
	uint32_t checkSum;
}chat;

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

uint32_t calcCheckSum(char* data, int length)
{
	uint32_t ret = 0;

	for(int i = 0; i < length; i ++) {
		ret += *(data + i);
	}

	return ret;
}

void recvFile(int clientfd, int datafd, string fileName) {
    char pathbuf[MAX_SIZE];
    string filePath = string(getcwd(pathbuf, sizeof(pathbuf)));
    fileName = filePath + "/" + fileName;

    string msg;
    char fileSizeStr[100] = {0};
    read(datafd, fileSizeStr, 100);
    int fileSize = atoi(fileSizeStr);
	cerr << fileSize << endl;
	
    FILE *fp = fopen(fileName.c_str(), "wb");
    if (fp == NULL) {
        string msg = "ERROR: Can not open file";
        cout << msg << endl;
        return;
    }

    char databuf[MAX_SIZE];
    bzero(databuf, MAX_SIZE);
    int len = 0;
    while ((len = read(datafd, databuf, MAX_SIZE)) > 0) {
		cerr << databuf << endl;
        if (fwrite(databuf, sizeof(char), len, fp) < len) {
            cout << "File: " << fileName << "write failed" << endl;
            return;
        }
        bzero(databuf, MAX_SIZE);
        fileSize -= len;
        if (fileSize <= 0) break;
    }
    fclose(fp);
    cout << "Received File: " << fileName << " successfully!" << endl;
}

void * read_msg(void *arg)
{
    int *fd = (int *) arg;
    int nread = 0;
    char buffer[1024];
    int clientfd = fd[0];
    int datafd = fd[1];

    while((nread = read(clientfd, buffer, sizeof(buffer))) > 0)
    {
        buffer[nread] = '\0';
        printf("%s\n",buffer);
        string command = string(buffer);
        string op;
        int sp = command.find(' ');
        if (sp == -1) {
            op = command;
        } else {
            op = command.substr(0, sp);
        }
        if (op == "recvfile") {
            string sender, fileName;
            int nextsp = command.find(' ', sp + 1);
            sender = command.substr(sp + 1, nextsp - sp - 1);
            fileName = command.substr(nextsp + 1, command.length() - 1 - nextsp);
            recvFile(clientfd, datafd, fileName);
        }
        memset(buffer,0,sizeof(buffer));
    }
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

void sendFile(int clientfd, string fileName) {
    char pathbuf[MAX_SIZE];
    string filePath = string(getcwd(pathbuf, sizeof(pathbuf)));
    fileName = filePath + "/" + fileName;

	cerr << fileName.c_str() << endl;
    FILE *fp = fopen(fileName.c_str(), "rb");

    if (fp == NULL) {
        string msg = "ERROR: File does not exit";
        cout << msg << endl;
        return;
    }

    // send the size of file 
    uint32_t fileSize = 0;
    int len = 0;
    char databuf[MAX_SIZE];
    bzero(databuf, MAX_SIZE);
    while ((len = fread(databuf, sizeof(char), MAX_SIZE, fp)) > 0) {
        fileSize += len;
        bzero(databuf, MAX_SIZE);
    }

	fileSize = htonl(fileSize);
    char fileSizeStr[100] = {0};
    sprintf(fileSizeStr, "%u", fileSize);

    // send the data
    fp = fopen(fileName.c_str(), "rb");

	dataPacket pack;
	for(uint32_t i = 0; i < 3; i ++) {
		bzero((void *)pack.data, sizeof(pack.data));
    	len = fread(pack.data, sizeof(char), 1000, fp);
		pack.order = htonl(i);

		pack.checkSum = calcCheckSum((char *)&pack, sizeof(pack) - 4);

		pack.checkSum = htonl(pack.checkSum);

        if (writen(clientfd, &pack, sizeof(pack)) < 0) {
            cout << "Send File: " << fileName << "Failed." << endl;
            break;
        }
    }

	Chat ret; 
    fclose(fp);

	Readn(clientfd, (void *)&ret, sizeof(ret));

	cerr << ret.type << " " << ret.version << " " << ret.packetCount << " " << ret.checkSum << endl;

    cout << "File: " << fileName << " transfer successfully!" << endl;

}

int main(int argc, char *argv[]) 
{ 
    int clientSocket, clientDataSocket; 
    char buffer[1024]; 
    struct sockaddr_in serverAddr, serverDataAddr; 
    struct hostent *host; 

        /* 使用hostname查询host 名字 */
    if(argc != 2) { 
        fprintf(stderr,"Usage:%s hostname \a\n",argv[0]); 
        exit(1); 
    } 

    if((host = gethostbyname(argv[1]))==NULL) {
        fprintf(stderr,"Gethostname error\n"); 
        exit(1); 
    } 

	cerr << host->h_addr << endl;

    /* 客户程序开始建立 clientSocket描述符 */ 
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1 ||
        (clientDataSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {// AF_INET:Internet;SOCK_STREAM:TCP
        fprintf(stderr,"Socket Error:%s\a\n",strerror(errno)); 
        exit(1); 
    } 

	int on = 1;
	if((setsockopt(clientSocket,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))<0 || setsockopt(clientDataSocket,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on))<0 ) {
		cerr << "setsockopt failed" << endl;
		exit(1);
	}

	cerr << clientSocket << endl;
	/* 客户程序填充服务端的资料 */ 
	bzero(&serverAddr,sizeof(serverAddr)); 
	serverAddr.sin_family = AF_INET;         
	serverAddr.sin_port = htons(DEFAULT_PORT);  
	serverAddr.sin_addr =* ((struct in_addr *)host->h_addr); 

	int nowsock = 10;

	bzero(&serverDataAddr,sizeof(serverDataAddr)); 
	serverDataAddr.sin_family = AF_INET;         
	serverDataAddr.sin_port = htons(DEFAULT_PORT + nowsock);  
	serverDataAddr.sin_addr =* ((struct in_addr *)host->h_addr); 

	if(bind(clientDataSocket, (struct sockaddr *)&serverDataAddr, sizeof(serverDataAddr)) == -1) {
		fprintf(stderr,"Bind error:%s\n\a",strerror(errno));
		close(clientDataSocket);
		exit(1);
	}

	if(listen(clientDataSocket, BACKLOG) == -1) {
		fprintf(stderr,"Listen error:%s\n\a",strerror(errno));
		close(clientDataSocket);
		exit(1);
	}

	if (connect(clientSocket, (struct sockaddr *)(&serverAddr), sizeof(struct sockaddr)) == -1) {
		fprintf(stderr,"Connect Error:%s\a\n",strerror(errno)); 
		exit(1); 
	} 

	chat.type = 'U';
	chat.version = 'd';
	chat.packetCount = 3;
	chat.packetCount = htonl(chat.packetCount);

	//chat.packetCount = htonl(3);

	strcpy(chat.filename, "result.png");

	chat.checkSum = calcCheckSum((char*)&chat, sizeof(chat) - 4);

	cerr <<"checksum:"<< chat.checkSum << endl;

	chat.checkSum = htonl(chat.checkSum);

	write(clientSocket, (void *)&chat, sizeof(chat));


	string filename = "result.png";
	sendFile(clientSocket, filename);

	/*
	unsigned int length = sizeof(struct sockaddr_in);
	sockaddr_in serverAddrr;

	cerr << "Waiting for connection..." << endl; 
	int serverDataSocket = accept(clientDataSocket, (struct sockaddr *) &serverAddrr, &length);

	if (serverDataSocket < 0) {
		fprintf(stderr,"Connect Error:%s\a\n",strerror(errno)); 
		exit(1); 
	} 
	*/

	//cerr << "Connected sock No." << serverDataSocket << endl;
	//sendFile(serverDataSocket, sbuffer);

	/* 结束通讯 */ 
	close(clientSocket); 
	return 0;
} 

