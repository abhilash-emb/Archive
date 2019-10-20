/*
This project tries to control the movement of vehicle via an SMS, using raspberry Pi, USB camera, and a GSM board.
The vehicle starts as the program begins execution(can be changed later to user controlled start)
Vehicle can be started/stopped via an SMS in a certain format (The message should contain certain keywords)
- 10 digit mobile number (replace the 10 character xxxxxxxxxx in the code with the phone number you want to use) 
- STOP/RESET (stop/restart the vehicle)
- MSGOVER (indicating that the message is over)
Vehicle location through GPS and sn SMS is sent at every stoppage of the vehicle.
Also image is stored at the stoppage of the vehicle
*/

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

#define SYS_START	10
#define SYS_STOP	20
#define SYS_RESET	30

// init serial port 
void init_uart();

//writes to uart
void uart_write();

// reads from uart
void uart_read_until(const char *);

// send sms
void send_sms();

// initial state
void init_state();

// initialize gps
void init_gps();

// get gps coordinates
void get_gps_coordinates();

// read sms
int read_sms();

// global constants
const char at_D[] = {0x0D};
const char at_A[] = {0x1A};

// response strings
const char *OK 				= "OK";
const char *MSG 			= ">";
const char *ST_IP_INITIAL 	= "STATE: IP INITIAL";
const char *ST_IP_START   	= "STATE: IP START";
const char *DOT 			= ".";

// global variables
char uart_write_str[110];
char uart_read_str[256];
char gps_str[100];
int  uart0_filestream 	= -1;
int  sys_state			=  0;

// Program Start
int main()
{
	if (!bcm2835_init())
		return 1;
	
	init_uart();
		
	init_gps();
	
    init_state();
    
    while(1)
    {
		// wait for an sms
		read_sms();
	}

	return 0;
}

void init_uart()
{
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
    
    cout<<"UART successfully Initialized" <<endl;
}

void init_state()
{
	bcm2835_gpio_set(RPI_V2_GPIO_P1_03);
	sys_state = SYS_START;
}

void send_sms()
{
	//AT
	strcpy(uart_str,"AT");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK	
	uart_read_until(OK);
	
	//AT+CMGF=1
	strcpy(uart_str,"AT+CMGF=1");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK	
	uart_read_until(OK); 
		
	//AT+CMGS=""
	strcpy(uart_str,"AT+CMGS=\"XXXXXXXXXX\"");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read(MSG);
		
	//MSG GPS Coordinates
	strcpy(uart_str,gps_str);
	cout << uart_str << endl;
	strcat(uart_str,at_A);
	uart_write();
		
    //OK
    uart_read(OK);
	
    cout<<"message sent"<<endl;
}

// Serial write
void uart_write()
{
    int n = write(uart0_filestream, uart_str, strlen(uart_str));
    if (n < 0)  perror("Write Error");
}

// serial read
void uart_read_until(const char *p_label)
{
	int  n=-1,i=0;
	char tmp=0;
	
	for(i=0;i<256;i++) uart_read_str[i]=0;
	
	i=0;
	while(1) {
		n = read(uart0_filestream, &tmp, 1);
		if (n < 0)  perror("Read Error");
		uart_read_str[i++] = tmp;
		if(NULL != strstr(uart_read_str,"ERROR"))    perror("Response ERROR");
		if(NULL != strstr(uart_read_str,ptr_l))		 break;
	}
	
	cout<<p_label<<endl;
}

int read_sms()
{
	uart_read_until("MSGOVER");
	
	//sms received
	if( strstr(uart_read_str,"XXXXXXXXXX") ) {
		if( strstr(uart_read_str,"STOP") )  {
			// stop command
			if(sys_state == SYS_START)	stop_state();
		}
		else if( strstr(uart_read_str,"RESET") ) {
			// reset
			if(sys_state == SYS_STOP)	init_state();
		} 
	}
		
}

int stop_state()
{
	// stops the vehicle 
	bcm2835_gpio_set(RPI_V2_GPIO_P1_03);
	
	// take the picture
	if(-1 == take_picture()) { cout<<"FS webcam error"<<endl; }
	
	// send the gps coordinate
	send_sms();
	
	//set system state
	sys_state = SYS_STOP;
	
}

int take_picture()
{
	if(-1 == system("fswebcam image.jpeg") return -1;
	else return 0;
}

void init_gps()
{
	// AT
	strcpy(uart_str,"AT");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CGPSPWR=1
	strcpy(uart_str,"AT+CGPSPWR=1");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CGPSRST=1
	strcpy(uart_str,"AT+CGPSRST=1");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CFUN=1
	strcpy(uart_str,"AT+CFUN=1");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CIPSHUT
	strcpy(uart_str,"AT+CIPSHUT");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CIPSTATUS
	strcpy(uart_str,"AT+CIPSTATUS");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(ST_IP_INITIAL);
	
	// AT+CGDCONT=1,"IP",
	strcpy(uart_str,"AT+CGDCONT=1,\"IP\",");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CGACT=1,1
	strcpy(uart_str,"AT+CGACT=1,1");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CGATT=1
	strcpy(uart_str,"AT+CGATT=1");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CIPSTATUS
	strcpy(uart_str,"AT+CIPSTATUS");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(ST_IP_INITIAL);
	
	// AT+CGATT=1
	strcpy(uart_str,"AT+CSTT");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CGATT=1
	strcpy(uart_str,"AT+CIPSTATUS");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(ST_IP_START);
	
	// AT+CIICR
	strcpy(uart_str,"AT+CIICR");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	// AT+CIFSR
	strcpy(uart_str,"AT+CIFSR");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(DOT);
	uart_read_until(DOT);
	uart_read_until(DOT);
	
	//AT+CGPSINF=32
	strcpy(uart_str,"AT+CGPSINF=32");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
	
	//process uart_read_str for coordinates
	if(0 == process_gps_coordinates()) {cout<<"GPS successfully initialized" <<endl;}
	else {cout<<"GPS coordinates NOT SET !!!!"};
}

void get_gps_coordinates()
{
	//AT+CGPSINF=32
	strcpy(uart_str,"AT+CGPSINF=32");
	strcat(uart_str,at_D);
	cout << uart_str << endl;
	uart_write();
	
	//OK
	uart_read_until(OK);
}

int process_gps_coordinates()
{
	int i=0,j=0;
	
	while( (uart_read_str[i] != 'A') || (uart_read_str[i+1] != ',') || (uart_read_str[i-1] != ',') ) i++;
	i++;
	
	gps_str[j++] = 'L'; gps_str[j++] = 'A'; gps_str[j++] = 'T'; gps_str[j++] = 'I'; 
	gps_str[j++] = 'T'; gps_str[j++] = 'U'; gps_str[j++] = 'D'; gps_str[j++] = 'E'; 
	gps_str[j++] = ' ';
	
	while(uart_read_str[i] != ',') { 
		if(uart_read_str[i] == '0' && uart_read_str[i+1] == '0' && uart_read_str[i+2] =='0' && uart_read_str[i+3] == '0') return -1;
		else gps_str[j++] = uart_read_str[i++];
	}
	
	i+=2;
	
	gps_str[j++] = 'L'; gps_str[j++] = 'O'; gps_str[j++] = 'N'; gps_str[j++] = 'G'; 
	gps_str[j++] = 'I'; gps_str[j++] = 'T'; gps_str[j++] = 'U'; gps_str[j++] = 'D'; gps_str[j++] = 'E'; 
	gps_str[j++] = ' ';
	
	while(uart_read_str[i] != ',') {
		if(uart_read_str[i] == '0' && uart_read_str[i+1] == '0' && uart_read_str[i+2] =='0' && uart_read_str[i+3] == '0') return -1;
		else gps_str[j++] = uart_read_str[i++]; }
	
	return 0;
}
