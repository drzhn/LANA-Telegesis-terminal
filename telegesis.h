#ifndef TELEGESIS_H
#define TELEGESIS_H

#include <stdio.h>   /* Стандартные объявления ввода/вывода */
#include <stdlib.h>  
#include <string.h>  /* Объявления строковых функций */
#include <unistd.h>  /* Объявления стандартных функций UNIX */
#include <fcntl.h>   /* Объявления управления файлами */
#include <termios.h> /* Объявления управления POSIX-терминалом */
#include <sys/ioctl.h>
#include <QString>


int TTYUSB_PORT=-1; /* Файловый дескриптор для порта */
//char *m_buf;
struct termios m_oldtio;

extern QString PORTNAME, Qcommand;//, Qresponse;

int connect_port(QString& Qportname)
{
    /*  List of status codes:
     *  0 - OK
     *  1 - error opening file descriptor
     *  2 - error opening control structure
     *  3 - error writing control structure to the port
     * */
    const char* portname_c = Qportname.toUtf8().constData();
    TTYUSB_PORT=open(portname_c, O_RDWR | O_NDELAY);
    if (TTYUSB_PORT<0)
    {
        TTYUSB_PORT=-1;
        return 1;
    }
    tcflush(TTYUSB_PORT, TCIOFLUSH);

    int n = fcntl(TTYUSB_PORT, F_GETFL, 0);
    fcntl(TTYUSB_PORT, F_SETFL, n & ~O_NDELAY);
    if (tcgetattr(TTYUSB_PORT, &m_oldtio)!=0) return 2;

    struct termios newtio;
    cfsetospeed(&newtio, (speed_t)B19200);
    cfsetispeed(&newtio, (speed_t)B19200);
    newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS8;
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~(PARENB | PARODD);
    newtio.c_cflag &= ~CRTSCTS;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_iflag=IGNBRK;
    newtio.c_iflag &= ~(IXON|IXOFF|IXANY);
    newtio.c_lflag=0;
    newtio.c_oflag=0;
    newtio.c_cc[VTIME]=1;
    newtio.c_cc[VMIN]=60;
    int mcs=0;
    ioctl(TTYUSB_PORT, TIOCMGET, &mcs);
    mcs |= TIOCM_RTS;
    ioctl(TTYUSB_PORT, TIOCMSET, &mcs);
    newtio.c_cflag &= ~CRTSCTS;
    if (tcsetattr(TTYUSB_PORT, TCSANOW, &newtio)!=0) return 3;

    return 0;
}

int sendByte(const char c)
{
	
    if (TTYUSB_PORT==-1)
	{
		return -1;
	}
    int res=write(TTYUSB_PORT, &c, 1);
	if (res<1)
	{
		return -1;
	}
	return 0;
}

int sendString(const char* s)
{
    unsigned int i;
    for (i=0; i<strlen(s); i++)
    {
        if (sendByte(s[i])!=0)
        {
            return -1;
        }
    }
    if (sendByte('\r')!=0)
    {
        return -1;
    }
    return 0;
}

/*void readData(char * c)
{
    int bytesRead =-1;
    while (bytesRead<0)
    {
        bytesRead = read(fd,c,1);
    }
}*/

 #endif /* TELEGESIS_H */
