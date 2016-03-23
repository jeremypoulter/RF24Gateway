
// MySQL library source:sudo apt-get install libmysqlclient-dev [http://raspberrypi-easy.blogspot.co.uk/2013/12/how-to-use-database-mysql-with.html]
// Compile: g++ -Wall -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s -I./RF24/RPi/RF24/ -lrf24-bcm `mysql_config --cflags` `mysql_config --libs` main.cpp -o RF24Gateway
// g++ -Wall -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s -I./RF24/RPi/RF24/ -lrf24-bcm  main.cpp -o RF24Gateway


// ssh -L 3307:localhost:3306 gavin@178.238.150.250 -i id_rsa -N -f 

// Packet structure (13 bytes)
// message[0]: DeviceID0
// message[1]: DeviceID1
// message[2]: DeviceID2
// message[3]: DeviceID3
// message[4]: DeviceID4
// message[5]: DeviceID5
// message[6]: DeviceID6
// message[7]: DeviceID7

// message[8]: Counter
// message[9]: Temp0
// message[10]: Temp1
// message[11]: Test
// message[12]: Relayed


// Inclusions
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <fstream>

#include <signal.h> // from daemonisation example
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

//#include <mysql/mysql.h>

#include <time.h> // required to fetch time and date

#include <getopt.h>
#include <cstdlib>
//#include "../RF24.h";
#include <RF24/RF24.h>

// ......................................................
// Global declarations
using namespace std;
//RF24 radio("/dev/spidev0.0",8000000 , 25);  //spi device, speed and CSN,only CSN is NEEDED in RPI
RF24 radio(RPI_V2_GPIO_P1_18, 0, BCM2835_SPI_SPEED_8MHZ);

//RF24 radio(RPI_BPLUS_GPIO_J8_15,RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ); 

//MYSQL *mysql1;

// Packet structure constants
const int DeviceID0=0;
const int DeviceID1=1;
const int DeviceID2=2;
const int DeviceID3=3;
const int DeviceID4=4;
const int DeviceID5=5;
const int DeviceID6=6;
const int DeviceID7=7;

const int Counter=8;
const int Temp0=9;
const int Temp1=10;
const int Test=11;
const int Relayed=12;

const int role_pin = 7;
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

int RFChannel = 0;


// -------------------- SUPPORT FUNCTIONS ---------------------------
// Prepare the RF24 radio

static void rf24setup(void){
  printf("\nPreparing interface\n");
  radio.begin();
  printf("Begin\n");
  radio.setChannel(RFChannel);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);
  radio.setPayloadSize(0x0d);

  // ............................................
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);
  radio.startListening();
  radio.printDetails();
  printf("Details printed\n");
}

// Get current date/time, format is YYYY-MM-DD HH:mm:ss
static const std::string currentDateTime() 
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return buf;
}

// Daemonisation process 
// from: http://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux

/*
static void SignalHandler(int signum)
{
	exit(0);
}

static void daemonise()
{
    pid_t pid;

    // Open the log file
    openlog ("SH001d", LOG_PID, LOG_DAEMON);

    pid = fork(); // Fork off the parent process 
    syslog (LOG_NOTICE, "First fork executed.");

    if (pid < 0)  // An error occurred
        exit(EXIT_FAILURE);

    if (pid > 0)  // Success: Let the parent terminate
        exit(EXIT_SUCCESS);

    if (setsid() < 0)    // On success: The child process becomes session leader
        exit(EXIT_FAILURE);

    // Catch, ignore and handle signals
    //TODO: Implement a working signal handler 
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTERM, SignalHandler);
    syslog (LOG_NOTICE, "Signal handler specified");

    pid = fork();     // Fork off for the second time
    syslog (LOG_NOTICE, "Second fork executed");


    if (pid < 0)     // An error occurred
        exit(EXIT_FAILURE);

    if (pid > 0)    // Success: Let the parent terminate
        exit(EXIT_SUCCESS);

    umask(0);     // Set new file permissions
    syslog (LOG_NOTICE, "File permissions set");


    chdir("/home/pi/gavin/SensorLogs/");
    syslog (LOG_NOTICE, "Default directory set");


    // Close all open file descriptors
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>0; x--)
    {
        close (x);
    }
    syslog (LOG_NOTICE, "Files closed");

}
*/

// Millisecond sleep function
static int msleep(unsigned long milisec)
{
    struct timespec req={0};
    time_t sec=(int)(milisec/1000);
    milisec=milisec-(sec*1000);
    req.tv_sec=sec;
    req.tv_nsec=milisec*1000000L;
    while(nanosleep(&req,&req)==-1)
         continue;
    return 1;
}

static bool String2Int(const std::string& str, int& result)
{
    return sscanf(str.c_str(), "%d", &result) == 1;
}

static string String2Upper(const string & s)
{
    string ret(s.size(), char());
    for(unsigned int i = 0; i < s.size(); ++i)
        ret[i] = (s[i] <= 'z' && s[i] >= 'a') ? s[i]-('a'-'A') : s[i];
    return ret;
}

static bool ParseOptions(string OptsFilename)
{
    string line;

  
    ifstream OptsFile (OptsFilename.c_str());
    if (OptsFile.is_open())
    {
        while ( getline (OptsFile,line) )
        {
			if (String2Upper(line.substr(0,9))=="RFCHANNEL") String2Int(line.substr(10,line.length()-10),RFChannel);
            
        }
        OptsFile.close();
    }
    else 
    {
        printf("Unable to open specified options file\n"); 
        return false;
    }
    printf("Settings after reading options file\n");
    printf("RF Channel %i\n",RFChannel);
    printf("\n");
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

// -------------------- MAIN ---------------------------

int main( int argc, char *argv[]){

	// Declarations
	uint8_t message[13];
	uint64_t deviceid;
	int tempint;
	double temperature;
	long counter; 
	string OptsFilename;

	counter=0;
        
        
	// Check arguments for filename
	if (argc != 2) //Check correct number of arguments we're supplied
	{
		printf("Incorrect number of arguments\n");
		exit(EXIT_FAILURE);
	}

	OptsFilename=argv[1];            // Pick options filename from command line
	ParseOptions(OptsFilename);      // Parse options file
        
	// Daemonising process
//	printf("Daemonising process\n");
//	daemonise();
//	syslog(LOG_NOTICE, "Daemonisation complete");

	// Initialise radio (and print settings to screen)
	rf24setup();
	radio.startListening(); // Start listening for incoming messages

	// Finishing
	while (1)
	{

		while ( ! radio.available() ) { // Wait for a message
			msleep(10);
		}

		radio.read( &message,13); // Get the message

		counter++;
                
		// Convert bytes into correct format for display
		tempint=(message[Temp0]*256)+message[Temp1];
		tempint=tempint>>4;
		if (tempint & (1<<11))
		   temperature = 0.0 - (((tempint ^ 0b111111111111)+1)*0.0625);
		else
		   temperature = tempint * 0.0625;
		deviceid = ((uint64_t)message[DeviceID0]<<56) + ((uint64_t)message[DeviceID1]<<48) + ((uint64_t)message[DeviceID2]<<40) + ((uint64_t)message[DeviceID3]<<32) + ((uint64_t)message[DeviceID4]<<24) + ((uint64_t)message[DeviceID5]<<16) + ((uint64_t)message[DeviceID6]<<8) + (uint64_t)message[DeviceID7];

		// printf("Message received: %ld %s nodec=%x %6.2f DeviceID:%016llX Test:%d Relayed:%d\n",counter,currentDateTime().c_str(),message[Counter],temperature,deviceid,message[Test],message[Relayed]);
		printf("{deviceId:\"%016llX\",temp:%6.2f}\n", deviceid, temperature);
	}

	syslog (LOG_NOTICE, "NRFText terminated.");
	closelog();
	return EXIT_SUCCESS;
}
