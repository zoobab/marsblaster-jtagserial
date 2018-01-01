// MarsBlaster.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string.h>
#include <conio.h>
#include <windows.h>

HANDLE g_hport = NULL;
int dwError=0;

int open_serial(char* pname)
{
	// Open the serial port.
	HANDLE hPort = CreateFile (pname, // Pointer to the name of the port
                      GENERIC_READ | GENERIC_WRITE,
                                    // Access (read/write) mode
                      0,            // Share mode
                      NULL,         // Pointer to the security attribute
                      OPEN_EXISTING,// How to open the serial port
                      0, //FILE_FLAG_OVERLAPPED, // Port attributes
                      NULL);        // Handle to port with attribute
                                    // to copy

	// If it fails to open the port, return FALSE.
	if ( hPort == INVALID_HANDLE_VALUE ) 
	{
		// Could not open the port.
		dwError = GetLastError ();
		printf("Cannot open COM port\n");
		return -1;
	}

	g_hport = hPort;
	DCB PortDCB;
	PortDCB.DCBlength = sizeof (DCB);     

	// Get the default port setting information.
	GetCommState (hPort, &PortDCB);

	// Change the DCB structure settings.
	PortDCB.BaudRate = CBR_38400;	// Current baud 
	PortDCB.fBinary = TRUE;			// Binary mode; no EOF check 
	PortDCB.fParity = TRUE;			// Enable parity checking. 
	PortDCB.fOutxCtsFlow = FALSE;   // No CTS output flow control 
	PortDCB.fOutxDsrFlow = FALSE;   // No DSR output flow control 
	PortDCB.fDtrControl = DTR_CONTROL_ENABLE; 
                                        // DTR flow control type 
	PortDCB.fDsrSensitivity = TRUE;	// DSR sensitivity 
	PortDCB.fTXContinueOnXoff = TRUE;	// XOFF continues Tx 
	PortDCB.fOutX = FALSE;				// No XON/XOFF out flow control 
	PortDCB.fInX = FALSE;               // No XON/XOFF in flow control 
	PortDCB.fErrorChar = FALSE;         // Disable error replacement. 
	PortDCB.fNull = FALSE;              // Disable null stripping.
	PortDCB.fRtsControl = RTS_CONTROL_ENABLE; 
										// RTS flow control 
	PortDCB.fAbortOnError = FALSE;      // Do not abort reads/writes on 
										// error.
	PortDCB.ByteSize = 8;               // Number of bits/bytes, 4-8 
	PortDCB.Parity = NOPARITY;          // 0-4=no,odd,even,mark,space 
	PortDCB.StopBits = ONESTOPBIT;      // 0,1,2 = 1, 1.5, 2 

	// Configure the port according to the specifications of the DCB 
	// structure.
	if (!SetCommState (hPort, &PortDCB))
	{
		dwError = GetLastError ();
		printf("Could not configure the serial port\n");
		return -1;
	}
	return 0;
}

int g_tms = -1;
int g_tdi = -1;

void set_tdi(int p)
{
	if(p==g_tdi)
		return;
	g_tdi = p;
	if(p)
		EscapeCommFunction(g_hport,SETRTS);
	else
		EscapeCommFunction(g_hport,CLRRTS);
}

void set_tms(int p)
{
	if(p==g_tms)
		return;
	g_tms = p;
	if(p)
		EscapeCommFunction(g_hport,SETDTR);
	else
		EscapeCommFunction(g_hport,CLRDTR);
}

void set_tck(int p)
{
	if(p)
		EscapeCommFunction(g_hport, SETBREAK);
	else
		EscapeCommFunction(g_hport, CLRBREAK);
}

int pulse_tck()
{
	set_tck(0);
	set_tck(1);
	return 0;
}

int pulse_tck_r()
{
	ULONG dw;
	set_tck(0);
	//wait CLK really zero
	while(1)
	{
		dw = 0;
		GetCommModemStatus(g_hport,&dw);
		if( (dw&0x20)==0 )
			break;
	}
	set_tck(1);
	//wait CLK really one
	while(1)
	{
		dw = 0;
		GetCommModemStatus(g_hport,&dw);
		if( (dw&0x20)!=0 )
			break;
	}

	int v = (dw>>4)&1;
	//printf(" %c %c %c\n",g_tms ? 'H' : 'L', g_tdi ? 'H' : 'L', v? 'H' : 'L');
	return v;
}

//go to Test Logic Reset state then to Idle
void goto_tlr()
{
	set_tms(1);
	set_tdi(1);
	for(int i=0; i<64; i++)
		pulse_tck();
	set_tms(0);
	pulse_tck();
}

int sir(int nclk, int val)
{
	int a=0;
	int b;
	set_tms(1);
	pulse_tck();
	pulse_tck();
	set_tms(0);
	pulse_tck();
	pulse_tck();

	for(int i=0; i<nclk; i++)
	{
		set_tdi(val&1);
		if(i==nclk-1)
			set_tms(1);
		val = val >> 1;
		b=pulse_tck_r();
		a=(a>>1) | (b<<15);
	}

	set_tdi(0);
	pulse_tck();
	set_tms(0);
	pulse_tck();
	return a;
}

int sdr(int nclk, int val)
{
	int a=0;
	int b;
	set_tms(1);
	pulse_tck();
	set_tms(0);
	pulse_tck();
	pulse_tck();

	for(int i=0; i<nclk; i++)
	{
		if(i==nclk-1)
				set_tms(1);
		set_tdi(val&1);
		val = val >> 1;
		b=pulse_tck_r();
		a=(a>>1) | (b<<15);
	}

	set_tdi(0);
	pulse_tck();
	set_tms(0);
	pulse_tck();
	return a;
}

void idle(int n)
{
//	set_tms(1);
	for(int i=0; i<n; i++)
		pulse_tck();
}

void get_id()
{
	int a=0;
	goto_tlr();
	idle(2);
	a=sdr(16,0xFFFF);
	a=sir(10,0x2CC);
	sdr(16,0xFFFF);
	sir(10,0x203);
	idle(2);
	sdr(13,0x89);
	sir(10,0x205);
	idle(2);
	a=sdr(16,0xFFFF);
	printf("%04X\n",a);
	a=sdr(16,0xFFFF);
	printf("%04X\n",a);
}

#define MAX_STRING_LENGTH (1024*4)
char rbuffer[MAX_STRING_LENGTH];
char command[MAX_STRING_LENGTH];

/*
process typical strings:
SIR 10 TDI (203);
*/
void do_SIR()
{
	//get command parameters
	unsigned int sir_arg = 0;
	unsigned int tdi_arg = 0;
	int n = sscanf(rbuffer,"SIR %d TDI (%X);",&sir_arg,&tdi_arg);
	if(n!=2)
	{
		printf("error processing SIR\n");
	}
	sir(sir_arg,tdi_arg);
}

/*
process typical strings:
SDR 16 TDI (FFFF) TDO (C0C7) MASK (FFFF);
SDR 16 TDI (FFFF) TDO (2027);
SDR 13 TDI (0000);
*/
void do_SDR()
{
	//get command parameters
	unsigned int sdr_arg = 0;
	unsigned int tdi_arg = 0;
	unsigned int tdo_arg = 0;
	unsigned int mask_arg = 0;
	int n = sscanf(rbuffer,"SDR %d TDI (%X) TDO (%X) MASK (%X);",&sdr_arg,&tdi_arg,&tdo_arg,&mask_arg);
	if(n==2)
	{
		//2 params
		sdr(sdr_arg,tdi_arg);
	}
	else
	if(n==3 || n==4)
	{
		//3-4 params
		unsigned int r;
		r=sdr(sdr_arg,tdi_arg);
		if(r!=tdo_arg)
		{
			//error
			printf("TDO returned (%04X) is not equal to expected (%04X)\n",r,tdo_arg);
			ExitProcess(-1);
		}
	}
	else
	{
		printf("error processing SDR\n");
	}
}

/*
process typical strings:
RUNTEST 53 TCK;
*/
void do_RUNTEST()
{
	//get command parameters
	unsigned int tck = 0;
	int n = sscanf(rbuffer,"RUNTEST %d TCK;",&tck);
	if(n==1)
	{
		//1 param
	}
	else
	{
		n = sscanf(rbuffer,"RUNTEST IDLE %d TCK",&tck);
		if(n==0)
		 printf("error processing RUNTEST\n");
	}

	//assume clock would be 10Mhz then pause is (milliseconds)
	unsigned int pause = tck/10000;

	idle(16);
	if(pause)
		Sleep(pause);
}

/*
process typical strings:
STATE IDLE;
*/
void do_STATE()
{
	idle(2);
	sdr(16,0xFFFF);
}

//reset JTAG
void do_TRST()
{
	goto_tlr();
}

void do_ENDDR()
{
}

void do_ENDIR()
{
}

void do_FREQUENCY()
{
}

int main(int argc, char* argv[])
{
	printf("Hello World!\n");
	printf("MarsBlaster - burn MAX2 CPLD from Altera Vector Programming File *.SVF!\n");
	printf("Serial COM port to JTAG is used for programming\n");
	printf("Usage example: >MarsBlaster.exe COM4 myfile.svf\n\n");
	if(argc<3)
	{
		printf("No input parameters!\n");
		return -1;
	}
	
	if(open_serial(argv[1])==-1)
	{
		printf("Cannot open serial port %s\n",argv[1]);
		return -1;
	}

	//open file
	FILE* f = fopen(argv[2],"r");
	if(f==NULL)
	{
		printf("Cannot open SVF file %s\n",argv[2]);
		return -1;
	}

	//read and process SVF file
	while(1)
	{
		//get string from text file
		char* pstr = fgets(rbuffer,MAX_STRING_LENGTH-1,f);
		if(pstr==NULL)
			break;

		int len = strlen(pstr);
		if(pstr[len-1]==0xA)
			pstr[len-1] = 0;

		//analyze 1st word
		int n = sscanf(pstr,"%s",command);
		if(n==0)
			break;

		if(command[0]=='!')
		{
			//line is commented
			//check important commented lines
			if( 
				(strcmp(command,"!CHECKING")==0) ||
				(strcmp(command,"!BULK")==0) ||
				(strcmp(command,"!PROGRAM")==0) ||
				(strcmp(command,"!VERIFY")==0)
				)
			{
				printf("-----------------------------------\n");
				printf("%s\n",rbuffer);
			}
		}
		else
		{
			//real command is here
			if(strcmp(command,"SIR")==0)
				do_SIR();
			else
			if(strcmp(command,"SDR")==0)
				do_SDR();
			else
			if(strcmp(command,"RUNTEST")==0)
				do_RUNTEST();
			else
			if(strcmp(command,"STATE")==0)
				do_STATE();
			else
			if(strcmp(command,"TRST")==0)
				do_TRST();
			else
			if(strcmp(command,"ENDDR")==0)
				do_ENDDR();
			else
			if(strcmp(command,"ENDIR")==0)
				do_ENDIR();
			else
			if(strcmp(command,"FREQUENCY")==0)
				do_FREQUENCY();
			else
				printf("Unknown command\n");
		}
	}

	fclose(f);
	return 0;
}

