/*************************************************
*ͷ�ļ�
*************************************************/
#include <stdio.h>
#include <fcntl.h> 
#include <unistd.h>
#include <termios.h> 
#include <sys/types.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <time.h>

volatile unsigned int cardid ;         //��Ƭ��ID
static struct timeval timeout;         //����
#define DEV_PATH   "/dev/ttySAC1"      //�豸����


/*************************************************
* ���ڲ�������
* ���ô��ڲ���:9600���� 
*************************************************/
void init_tty(int fd)
{    
	//�������ô��ڵĽṹ��
	struct termios termios_new;
	//����ոýṹ��
	bzero( &termios_new, sizeof(termios_new));
	//	cfmakeraw()�����ն����ԣ���������termios�ṹ�еĸ���������
	cfmakeraw(&termios_new);
	//���ò�����
	termios_new.c_cflag=(B9600);
	//cfsetispeed(&termios_new, B9600);
	//cfsetospeed(&termios_new, B9600);
	//CLOCAL��CREAD�ֱ����ڱ������Ӻͽ���ʹ�ܣ���ˣ�����Ҫͨ��λ����ķ�ʽ����������ѡ�    
	termios_new.c_cflag |= CLOCAL | CREAD;
	//ͨ��������������λΪ8λ
	termios_new.c_cflag &= ~CSIZE;
	termios_new.c_cflag |= CS8; 
	//��������żУ��
	termios_new.c_cflag &= ~PARENB;
	//һλֹͣλ
	termios_new.c_cflag &= ~CSTOPB;
	tcflush(fd,TCIFLUSH);
	// �����ý����ַ��͵ȴ�ʱ�䣬������Ҫ����Խ�������Ϊ0
	termios_new.c_cc[VTIME] = 10;
	termios_new.c_cc[VMIN] = 1;
	// �����������/���������
	tcflush (fd, TCIFLUSH);
	//������ú󣬿���ʹ�����º������������
	if(tcsetattr(fd,TCSANOW,&termios_new) )
		printf("Setting the serial1 failed!\n");

}


/*����У���*/
unsigned char CalBCC(unsigned char *buf, int n)
{
	int i;
	unsigned char bcc=0;
	for(i = 0; i < n; i++)
	{
		bcc ^= *(buf+i);
	}
	return (~bcc);
}


int PiccRequest(int fd)
{
	unsigned char WBuf[128], RBuf[128];
	int  ret, i,len;
	fd_set rdfd;
	
	memset(WBuf, 0, 128);
	memset(RBuf,0,128);
	WBuf[0] = 0x07;	//֡��= 7 Byte
	WBuf[1] = 0x02;	//����= 0 , ��������= 0x01
	WBuf[2] = 0x41;	//����= 'C'
	WBuf[3] = 0x01;	//��Ϣ����= 0
	WBuf[4] = 0x52;	//����ģʽ:  ALL=0x52
	WBuf[5] = CalBCC(WBuf, WBuf[0]-2);		//У���
	WBuf[6] = 0x03; 	//������־

	FD_ZERO(&rdfd); 
	FD_SET(fd,&rdfd);
	tcflush (fd, TCIFLUSH);
	write(fd, WBuf, 7);;
	ret = select(fd + 1,&rdfd, NULL,NULL,&timeout);
	switch(ret)
	{
		case -1:
			perror("select error\n");
			break;
		case  0:
			printf("Request timed out.\n");
			break;
		default:
			usleep(10000);
			ret = read(fd, RBuf, 8);
			len = ret;
			ret = read(fd, RBuf+len, 8);
			len +=ret;
			if (len < 0)
			{
				printf("len = %d, %m\n", len, errno);
				break;
			}
			if (RBuf[2] == 0x00)	 	//Ӧ��֡״̬����Ϊ0 ������ɹ�
			{
				return 0;
			}
			break;
	}
	return -1;
}


/*����ײ����ȡ��Χ�����ID*/
int PiccAnticoll(int fd)
{
	unsigned char WBuf[128], RBuf[128];
	int ret, i,len;
	fd_set rdfd;;
	memset(WBuf, 0, 128);
	memset(RBuf,0,128);
	WBuf[0] = 0x08;	//֡��= 8 Byte
	WBuf[1] = 0x02;	//����= 0 , ��������= 0x01
	WBuf[2] = 0x42;	//����= 'B'
	WBuf[3] = 0x02;	//��Ϣ����= 2
	WBuf[4] = 0x93;	//����ײ0x93 --һ������ײ
	WBuf[5] = 0x00;	//λ����0
	WBuf[6] = CalBCC(WBuf, WBuf[0]-2);		//У���
	WBuf[7] = 0x03; 	//������־
	tcflush (fd, TCIFLUSH);
	FD_ZERO(&rdfd);
	FD_SET(fd,&rdfd);
	write(fd, WBuf, 8);
	
	ret = select(fd + 1,&rdfd, NULL,NULL,&timeout);
	switch(ret)
	{
		case -1:
			perror("select error\n");
			break;
		case  0:
			perror("Timeout:");
			break;
		default:
			usleep(10000);
                        ret = read(fd, RBuf, 10);
                        len = ret;
                        ret = read(fd, RBuf+len, 10);
                        len +=ret;

			if (len < 0)
			{
				printf("len = %d, %m\n", len, errno);
				break;
			}
			if (RBuf[2] == 0x00) //Ӧ��֡״̬����Ϊ0 ���ȡID �ɹ�
			{
				cardid = (RBuf[4]<<24) | (RBuf[5]<<16) | (RBuf[6]<<8) | RBuf[7];
				return 0;
			}
	}
	return -1;
}

void get_json(int ID)
{
	//�õ���ǰʱ��
	time_t t;
	time(&t);//��ȡʱ��
	struct tm *lt = localtime(&t);//תΪʱ��ṹ
	printf("%d/%d/%d %d:%d:%d\n", 
		lt->tm_year+1900, lt->tm_mon+1,
		lt->tm_mday, lt->tm_hour,
		lt->tm_min, lt->tm_sec);

	//���ļ�
	int fd = open("./data.json", O_RDWR | O_CREAT | O_TRUNC);
	if(fd < 0)
	{
		perror("open() failed");
		return;
	}
	//д������
	char json[1024];//����һ���㹻��Ŀռ䣬���������
	sprintf(json, "[{\"cardId\":\"0x%x\", \"startTime\":\"%d/%d/%d %d:%d:%d\"}]",
		ID, lt->tm_year+1900, lt->tm_mon+1,
		lt->tm_mday, lt->tm_hour,
		lt->tm_min, lt->tm_sec);

	printf("%s\n", json);
	//�����ļ�ƫ��
	lseek(fd, -1, SEEK_END);
	write(fd, json, strlen(json));
	
	//�ر��ļ�
	close(fd);
}

int main(void)
{
	int ret, i;
	int serial_fd;

	serial_fd = open(DEV_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (serial_fd < 0)
	{
		fprintf(stderr, "Open Gec6818_ttySAC1 fail!\n");
		return -1;
	}
	/*��ʼ������*/
	init_tty(serial_fd);
	while(1)
	{ 
		sleep(1);
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		
		/*�������߷�Χ�Ŀ�*/
		if ( PiccRequest(serial_fd) )
		{
			printf("The request failed!\n");
			continue;
		}
		/*���з���ײ����ȡ���߷�Χ������ID*/
		if( PiccAnticoll(serial_fd) )
		{
			printf("Couldn't get card-id!\n");
			continue;
		}
		printf("card ID = %x\n", cardid);
		if(cardid == 0x2e4cfc7a)
		{
			printf("111\n");
		}
		else
		{
			printf("222\n");
		}
		
		get_json(cardid);
		
		system("sz data.json");
		
		cardid = 0;
	}
	
	close(serial_fd);
	
	return 0;
}
