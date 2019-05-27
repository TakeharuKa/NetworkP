#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <iostream>
#define MAXLINE 1024
#define BUFFER_SIZE 1024
using namespace std;

char username[111], password[111], guy[111];

const string start_end_identifier = "##";
const string divide_1 = ":";
const string divide_2 = ",";
string msg;

char buff[MAXLINE + 10];

string first_msg, first_val, second_msg, second_val;

int close_connection;

	char *servInetAddr = "127.0.0.1";
	int socketfd;
	struct sockaddr_in sockaddr;
	char recvline[MAXLINE], sendline[MAXLINE];

string con(char *t)
{
	string s = "";
	int l = (int)strlen(t);
	for (int i = 0; i < l; i++)
		s += t[i];
	return s;
}

void checkbuff()
{
	//{{{
	string s = "";
	int l = (int)strlen(buff);
	for (int i = 0; i < l; i++)
		s += buff[i];
	int first = s.find("##");
	if (first < 0) //no command found
		return;
	first += 2;
	int end = s.find("##", first) - 1;
	if (end <= first)
	{
		//printf("command error\n");
		return;
	}

	s = s.substr(first, end - first + 1); //command

	first = s.find(":");

	if (first < 0)
	{
		//printf("command first : error\n");
		return;
	}

	first_msg = "";
	first_val = "";

	first_msg = s.substr(0, first); //first_msg

	end = s.find(",", first + 1);

	if (first < 0)
	{
		//printf("command first , error\n");
		return;
	}

	first_val = s.substr(first + 1, end - first - 1); //first_val

	first = end + 1;
	end = end + 1;

	first = s.find(":", first);

	if (first < 0)
	{
		int a = 1;
		//printf("command second : not found\n");
	}
	else
	{
		second_msg = "";
		second_val = "";

		second_msg = s.substr(end, first - end);

		end = first + 1;

		first = s.find(",", first + 1);

		if (first < 0)
		{
			int a = 1;
			//printf("command second , not_found\n");
		}

		second_val = s.substr(end, first - end);

		first = end + 1;
	}
	
	//cout << endl;

	//cout << "first_msg:" << first_msg << " first_val:" << first_val << endl;
	//cout << "second_msg:" << second_msg << " second_val:" << second_val << endl;
	//}}}

	
}

void now_login()
{
	char read[1111];
	while (true)
	{
		strcpy(username, "w");
		printf("\n--please input your command--\n");
		printf("\"q\" or \"quit\" =====> quit\n");
		printf("\"send\" ======> Send files\n");
		printf("\"recv\" ======> Recv files\n");
		fgets(read, 1024, stdin);

		int length = (int)strlen(read);
		while (read[length - 1] == '\n' || read[length - 1] == '\r')
		{
			read[length - 1] = '\0';
			length -= 1;
		}

		puts("");

		if (strcmp(read, "quit") == 0)
			exit(0);

		if (strcmp(read, "q") == 0)
			exit(0);

		//{{{
		if (strcmp(read, "send") == 0)
		{
			char temp[1111];
			/*
			printf("--Please enter your friend's name--\n");

			puts("");

			int length = (int)strlen(temp);
			while (temp[length - 1] == '\n' || temp[length - 1] == '\r')
			{
				temp[length - 1] = '\0';
				length -= 1;
			}

			string send_to = con(temp); //send to whom?

			*/

			printf("--Please enter the filename you want to send--\n");
			printf("--Make sure the directory is your running directory--\n");
			fgets(temp, 1024, stdin);
			puts("");

			length = (int)strlen(temp);
			while (temp[length - 1] == '\n' || temp[length - 1] == '\r')
			{
				temp[length - 1] = '\0';
				length -= 1;
			}

			string file_name = con(temp); //filename

			string send_to = "w";
			
			// 打开文件并读取文件数据
			FILE *fp = fopen(file_name.c_str(), "r");
			if(NULL == fp)
			{
				printf(">> File:%s Not Found <<\n", file_name.c_str() );
				continue;
			}

			string msg = "";
			msg = start_end_identifier;
			msg += "send_filename" + divide_1 + file_name + divide_2 + \
					username + divide_1 + send_to + divide_2 + start_end_identifier;

			if((send(socketfd,msg.c_str(),strlen(msg.c_str()),0)) < 0)
			{
				printf("send mes error: %s errno : %d",strerror(errno),errno);
				exit(0);
			}

			//get prepare result
			int n = recv(socketfd, buff, MAXLINE, 0);
			//printf("n:%d\n", n);
			buff[n] = '\0';
			checkbuff(); //check the msg from server

			//printf("get returned message!\n");

			if (first_msg == "send_prepare_result")
			{
				if (first_val == "not_friend")
				{
					printf(">> You are not friend with \"%s\", can not send file <<\n", send_to.c_str());
					continue;
				}
				if (first_val == "file_not_found")
				{
					printf(">> File not found in running directly, make sure file exist <<\n");
					continue;
				}
				if (first_val != "succeed")
					continue;
			}

			sleep(1); //wait for 1 second to send;


				bzero(buff, BUFFER_SIZE);
				int length = 0;
				// 每读取一段数据，便将其发送给客户端，循环直到文件读完为止
				while((length = fread(buff, sizeof(char), BUFFER_SIZE, fp)) > 0)
				{
					printf("> Sending!!!\n");
					if(send(socketfd, buff, length, 0) < 0)
					{
						printf(">> Send File:%s Failed. <<\n", file_name.c_str());
						break;
					}
					bzero(buff, BUFFER_SIZE);
				}

				// 关闭文件
				fclose(fp);
				printf(">> File:%s Transfer Successful! <<\n", file_name.c_str());

		}
		//}}}

		if (strcmp(read, "recv") == 0)
		{

			//send the filename that you want to receive
			string msg = "";
			msg = start_end_identifier;
			msg += "recv_files" + divide_1 + username + divide_2 + \
					start_end_identifier;

			if((send(socketfd,msg.c_str(),strlen(msg.c_str()),0)) < 0)
			{
				printf("send mes error: %s errno : %d",strerror(errno),errno);
				exit(0);
			}

			bool ff = false;
			printf("sleeping\n");
			sleep(1);
			while (true)
			{
				//get prepare result
				int n = recv(socketfd, buff, MAXLINE, 0);
				buff[n] = '\0';
				//printf("checking buff buff:%s\n", buff);
				checkbuff(); //check the msg from server
				//printf("first\n");

				if (first_msg == "recv_files_result")
				{
					string res = first_val;
					if (res == "")
					{
						printf(">> Sorry, It seems that you don't have any file to download <<\n");
						ff = false;
						break;
					}
					printf("> Your files are listed below\n");
					printf("%s", res.c_str());
					ff = true;
					break;
				}
			}

			if (ff == false) continue;

			printf("--Please enter the filename you want to download--\n");

			char temp[1111];
			fgets(temp, 1024, stdin);
			puts("");

			int length = (int)strlen(temp);
			while (temp[length - 1] == '\n' || temp[length - 1] == '\r')
			{
				temp[length - 1] = '\0';
				length -= 1;
			}

			string file_name = con(temp);

			//send the filename that you want to receive
			msg = "";
			msg = start_end_identifier;
			msg += "recv_filename" + divide_1 + file_name + divide_2 + \
					username + divide_1 + "hahaha" + divide_2 + start_end_identifier;

			if((send(socketfd,msg.c_str(),strlen(msg.c_str()),0)) < 0)
			{
				printf("send mes error: %s errno : %d",strerror(errno),errno);
				exit(0);
			}

			//sleep(1);
			while (true)
			{

				//get prepare result
				int n = recv(socketfd, buff, MAXLINE, 0);
				buff[n] = '\0';
				//printf("buff:%s\n", buff);
				checkbuff(); //check the msg from server

				//printf("get returned message!\n");

				if (first_msg == "prepare_result")
				{
					if (first_val != "succeed")
					{
						printf(">> File not found <<\n");
						break;
					}
					sleep(1);

					printf("> Prepare to receive file\n");

					// 打开文件，准备写入
					FILE *fp = fopen(file_name.c_str(), "w");
					if(NULL == fp)
					{
						printf(">> File:\t%s Can Not Open To Write\n <<", file_name.c_str());
						exit(1);
					}

					struct timeval timeout1={0,100000};//1s
					//setsockopt((*it).fd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
					setsockopt(socketfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout1,sizeof(timeout1));

					// 从服务器接收数据到buffer中
					// 每接收一段数据，便将其写入文件中，循环直到文件接收完并写完为止
					bzero(buff, BUFFER_SIZE);
					int length = 0;
					while((length = recv(socketfd, buff, BUFFER_SIZE, 0)) > 0)
					{
						printf("> Receiving\n");
						if(fwrite(buff, sizeof(char), length, fp) < length)
						{
							printf(">> File:\t%s Write Failed <<\n", file_name.c_str());
							break;
						}
						bzero(buff, BUFFER_SIZE);
					}		

					// 接收成功后，关闭文件，关闭socket
					printf(">> Receive File:\t%s From Server IP Successful! <<\n", file_name.c_str());
					fclose(fp);
					
					break;
				}
			}


		}

	}
}


//{{{
void login()
{
	//int n;

	socketfd = socket(AF_INET,SOCK_STREAM,0);

	memset(&sockaddr,0,sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(10004);
	inet_pton(AF_INET,servInetAddr,&sockaddr.sin_addr);

	if((connect(socketfd,(struct sockaddr*)&sockaddr,sizeof(sockaddr))) < 0 )
	{
		printf("connect error %s errno: %d\n",strerror(errno),errno);
		return;
		//exit(0);
	}

	sleep(1);

close_connection = 0;

	now_login();
	close(socketfd);
}
//}}}


int main()
{

	char read[1234];

	login();
	close(socketfd);
	exit(0);
}
