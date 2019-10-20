/* this project uses rasperberry pi and gsm board to send the dicom image over gprs.
The image is uploaded to the server, via FTP protocol.
Image is in dicom format and is converted to JPEG format before uploading.
Uploaded image can be viewed by typing the URL on any browser.
Uses the dcm library to convert the dcm image to JPEG.
The FTP uploading happends via the AT commands that are executed on the GSM board which can be connected to the Pi via serial port.
*/
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"

#include <iostream>
using namespace std;

#include <bcm2835.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// Serial read write functions
int  uart_write();
int  uart_read_OK();
int  uart_read_MSG();
int  uart_read_FTPPUT(const char *);
int  uart_read_gen();
int  uart_write_tmp(int size);
int  uart_read_temp();

// convert image format
int  convert_dcm_2_jpg();

// send sms
void send_sms();

// upload file through FTP to server
void upload_file();

// global constants
const char at_D[] = {0x0D};
const char at_A[] = {0x1A};
const char *l1    = "+FTPPUT:1,1,1200";
const char *l2    = "+FTPPUT:2,1000";
const char *l3    = "+FTPPUT:2,";
const char *l4    = "+FTPPUT:1,0";

// global variables
char	   uart_str[1000];
int  	   uart0_filestream = -1;
int		   tx_enable = 1; 
OFString   patientName;

//tmp
int read_ok;

int main()
{
	if (!bcm2835_init())
		return 1;
	
	// Open the Port. We want read/write, no "controlling tty" status, and open it no matter what state DCD is in
    uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart0_filestream == -1) 	perror("open_port: Unable to open /dev/ttyAMA0 - ");
	
		// Set Port Parameters
	struct termios options;
    tcgetattr(uart0_filestream, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    options.c_cc[VMIN] = 1;
    tcflush(uart0_filestream, TCIFLUSH);
	if(-1 == tcsetattr(uart0_filestream, TCSANOW, &options))	perror("tccsetattr error  !!!! ");
	
	// Turn off blocking for reads, use (fd, F_SETFL, FNDELAY) if you want that
    fcntl(uart0_filestream, F_SETFL, 0);
    
    // dummy read
    //char tmp;
    //read(uart0_filestream, &tmp, 1);
    
    
   	// convert & store the image info
   	//convert_dcm_2_jpg();
		
	// send sms
	//send_sms();
	
	// upload file throught FTP
	upload_file();
	   
    // clean up
    close(uart0_filestream);
    bcm2835_close();
  
	return 0;
	
}

int convert_dcm_2_jpg()
{
	DcmFileFormat fileformat;
    OFCondition status = fileformat.loadFile("one.dcm");
    if (status.good())
    {
      if (fileformat.getDataset()->findAndGetOFString(DCM_PatientName, patientName).good())
        cout << "Patient's Details Successfully Read " << endl;
      else
        cerr << "Error: cannot access Patient's Name!" << endl;
    }
    else
      cerr << "Error: cannot read DICOM file (" << status.text() << ")" << endl;

    if(-1 == system("dcmj2pnm one.dcm one.jpeg --write-jpeg"))
      cerr << "Error in conversion of DCM to JPEG" << endl;
    else
      cout << "DCM File Converted to JPEG successfully" << endl;
      
    return 0;
		
}

void send_sms()
{
	if(tx_enable) {
		
		strcpy(uart_str,"AT");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
		
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	
	if(tx_enable) {
		
		strcpy(uart_str,"AT+CMGF=1");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT + CMGF Write Error  !!!! ");
		
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
		
	if(tx_enable) {
		
		strcpy(uart_str,"AT+CMGS=\"9886889561\"");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT + CMGS Write Error  !!!! ");
	
	}
	
	tx_enable = 0;
	if(-1 == uart_read_MSG()) perror("Serial Read Error  !!!! ");
	
	
	if(tx_enable) {
		
		strcpy(uart_str,patientName.data());
		cout << uart_str << endl;
		strcat(uart_str,at_A);
		if(-1 == uart_write())	perror("message Write Error  !!!! ");
	
	}
    
    tx_enable = 0;
    if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	
    cout<<"message sent"<<endl;
    
}

// Serial write
int uart_write()
{
    int n = write(uart0_filestream, uart_str, strlen(uart_str));
    if (n < 0)  return -1;
    if(read_ok) cout<<"Bytes Written "<<n<<endl;
}

int uart_write_tmp(int size)
{
	int n = write(uart0_filestream, uart_str, size);
    if (n < 0)  return -1;
    if(read_ok) cout<<"Bytes Written "<<n<<endl;
}

// Serial read for OK response
int uart_read_OK()
{
	int  n=-1;
	char tmp=0,prev=0;
	
	//if(read_ok) cout<<"in read OK"<<endl;
	
	while(1) {
		n = read(uart0_filestream, &tmp, 1);
		if (n < 0)  return -1;
		if(read_ok)	printf("%x\n",tmp);
		if(tmp == 'K' && prev == 'O')	 break;
		prev = tmp;
	}
	cout<<"OK"<<endl;	
		
	tx_enable = 1;
	return 0;
				
}

int uart_read_MSG()
{	
	int  n=-1;
	char tmp=0;
		
	while(1) {
		n = read(uart0_filestream, &tmp, 1);
		if (n < 0)  return -1;
		//printf("%x\n",tmp);
		if(tmp == '>')	 break;
	}
	
	cout<<">"<<endl;
	tx_enable = 1;
	return 0;
	
}

int uart_read_FTPPUT(const char *ptr_l)
{
	int  n=-1,i=0,j=0;
	char tmp=0,prev=0;
	char resp_buf[256];
		
	for(i=0;i<256;i++) resp_buf[i]=0;
	
	while(1) {
		n = read(uart0_filestream, &tmp, 1);
		if (n < 0)  return -1;
		if(read_ok)	printf("%x\n",tmp);
		resp_buf[j++] = tmp;
		if(NULL != strstr(resp_buf,ptr_l)) break;
	}
	
	cout<<ptr_l<<endl;
	tx_enable = 1;
	return 0;
}

int uart_read_gen()
{
	int  n=-1,i=0,j=0;
	char tmp=0,prev=0;
	char resp_buf[256];
		
	for(i=0;i<256;i++) resp_buf[i]=0;
	
	while(1) {
		n = read(uart0_filestream, &tmp, 1);
		if (n < 0)  return -1;
		//printf("%x\n",tmp);
		resp_buf[j++] = tmp;
		//cout<<resp_buf<<endl;
		if(NULL != strstr(resp_buf,"ERROR")) {/*cout<<"error"<<endl;*/tx_enable = 1; return 6;}
		if(NULL != strstr(resp_buf,"OK")) {/*cout<<"success"<<endl;*/ break;}
		
	}
	
	cout<<"OK"<<endl;
	tx_enable = 1;
	return 0;
	
}

int uart_read_temp()
{
	int  n=-1,i=0,j=0;
	char tmp=0,prev=0;
	char resp_buf[256];
		
	for(i=0;i<256;i++) resp_buf[i]=0;
	
	while(1) {
		n = read(uart0_filestream, &tmp, 1);
		if (n < 0)  return -1;
		printf("%x\n",tmp);
		//resp_buf[j++] = tmp;
		//cout<<resp_buf<<endl;
	}
	
	tx_enable = 1;
	return 0;
	
}

void upload_file()
{
	//AT
	if(tx_enable) {
		strcpy(uart_str,"AT");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	
	//at+sapbr=3,1,"Contype","GPRS"
	if(tx_enable) {
		strcpy(uart_str,"AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	
	//at+sapbr=3,1,"APN","internet"
	if(tx_enable) {
		strcpy(uart_str,"AT+SAPBR=3,1,\"APN\",\"internet\"");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	
	//at+sapbr=1,1
	int sapbr_cnt = 5,ret;
	while(sapbr_cnt) {
		if(tx_enable) {
			strcpy(uart_str,"AT+SAPBR=1,1");
			strcat(uart_str,at_D);
			cout << uart_str << endl;
			if(-1 == uart_write())	perror("AT Write Error  !!!! ");
		}
		tx_enable = 0;
		ret = uart_read_gen();
		if(-1 == ret) perror("Serial Read Error  !!!! ");
		else if(0 == ret) {break;}
		else if(6 == ret && 1 == sapbr_cnt) {cout << "SAPBR setting unsuccessfull" << endl; return;}
		cout<<"Retrying..."<< sapbr_cnt <<endl;
		sapbr_cnt--;
	}
		
	//at+sapbr=2,1
	if(tx_enable) {
		strcpy(uart_str,"AT+SAPBR=2,1");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
		
	//at+ftpcid=1
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPCID=1");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
		
	//AT+FTPSERV="www.kaimsofttech.com"
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPSERV=\"www.kaimsofttech.com\"");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	
	//AT+FTPUN="kaimsoft"
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPUN=\"kaimsoft\"");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	
	//AT+FTPPW="1234Four!"
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPPW=\"1234Four!\"");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");

	//AT+FTPPUTNAME="one.jpeg"
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPPUTNAME=\"one9.jpeg\"");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}

	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");

	//at+ftpputpath="/www/prestashop/"
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPPUTPATH=\"/www/prestashop/\"");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");

	//at+ftpput=1
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPPUT=1");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	
	}
	
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	tx_enable = 0;
	if(-1 == uart_read_FTPPUT(l1)) perror("Serial Read Error  !!!! ");
	
	//Read file and calculate size - send 1000 bytes at a time
	FILE *fp = NULL;
	int size,div,mod,cnt,total_bytes=0;
	
	if(NULL == (fp = fopen("one.jpeg","rb")))	perror(" file open error!!!! ");
	fseek(fp,0,SEEK_END);
	size = ftell(fp);
	fseek(fp,0,SEEK_SET);
	div = size/1000;
	mod = size%1000;
	fclose(fp);
	
	/*cout<< "div = " << div  << endl;	
	cout<< "mod = " << mod  << endl;
	cout<< "size = "<< size << endl;*/
	
	int c,fd;
	fd = open("one.jpeg",O_RDONLY);
	if(fd == -1) perror("file open error");
		
	//at+ftpput=2,1000
	int temp_cnt = 0;
	cnt = 1000;
	while(div--) {
		if(tx_enable) {
			strcpy(uart_str,"AT+FTPPUT=2,1000");
			strcat(uart_str,at_D);
			cout << uart_str << endl;
			if(-1 == uart_write())	perror("AT Write Error  !!!! ");
		}
		tx_enable = 0;
		/*if(temp_cnt>1) {
			cout<<"Stuck Here "<<temp_cnt<<endl;
			uart_read_temp();
			return;
		}*/
		if(-1 == uart_read_FTPPUT(l2)) perror("Serial Read Error  !!!! ");
		if(tx_enable) {
			temp_cnt++;
			c = read(fd,uart_str,cnt);
			if(temp_cnt>1) cout<<"Bytes read "<<c<<endl;
			if(c == -1) perror("Read System call failed");
			if(-1 == uart_write_tmp(c))	perror("AT Write Error  !!!! ");
		}
		tx_enable = 0;
		if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
		total_bytes += cnt;
	} 
	
	//at+ftpput=2,mod
	char mod_tmp[10];
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPPUT=2,");
		snprintf(mod_tmp, 10, "%d", mod); 
		strcat(uart_str,mod_tmp);
		strcat(uart_str,at_D`
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}

	tx_enable = 0;
	if(-1 == uart_read_FTPPUT(l3)) perror("Serial Read Error  !!!! ");
	
	if(tx_enable) {
		c = read(fd,uart_str,mod);
		if(c == -1) perror("Read System call failed");
		if(-1 == uart_write_tmp(c))	perror("AT Write Error  !!!! ");
	}
	
	tx_enable=0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	tx_enable=0;
	if(-1 == uart_read_FTPPUT(l1)) perror("Serial Read Error  !!!! ");
	
	total_bytes += mod;
	
	close(fd);
	
	//total bytes written into the file
	cout << "total bytes written : " << total_bytes << endl;

	//at+ftpput=2,0
	if(tx_enable) {
		strcpy(uart_str,"AT+FTPPUT=2,0");
		strcat(uart_str,at_D);
		cout << uart_str << endl;
		if(-1 == uart_write())	perror("AT Write Error  !!!! ");
	}
	tx_enable = 0;
	if(-1 == uart_read_OK()) perror("Serial Read Error  !!!! ");
	tx_enable = 0;
	if(-1 == uart_read_FTPPUT(l4)) perror("Serial Read Error  !!!! ");
	
	cout<<"file uploaded successfully"<<endl;
			
}
