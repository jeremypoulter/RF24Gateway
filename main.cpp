
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
#include <stdint.h>
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

#ifndef TEST
#include <RF24/RF24.h>
#endif

#include <curl/curl.h>
#include <json/json.h>

// ......................................................
// Global declarations
using namespace std;

#ifndef TEST
//RF24 radio("/dev/spidev0.0",8000000 , 25);  //spi device, speed and CSN,only CSN is NEEDED in RPI
RF24 radio(RPI_V2_GPIO_P1_18, 0, BCM2835_SPI_SPEED_8MHZ);
//RF24 radio(RPI_BPLUS_GPIO_J8_15,RPI_BPLUS_GPIO_J8_24, BCM2835_SPI_SPEED_8MHZ); 
#endif

//MYSQL *mysql1;

// Packet structure constants
#define DeviceID0       0
#define DeviceID1       1
#define DeviceID2       2
#define DeviceID3       3
#define DeviceID4       4
#define DeviceID5       5
#define DeviceID6       6
#define DeviceID7       7
#define Counter         8
#define Temp0           9
#define Temp1           10
#define Test            11
#define Relayed         12

const int role_pin = 7;
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

// Config
int RFChannel = 0;
string EmonCmsBaseUrl = "";
string EmonCmsApiKey = "";
bool SendAck = false;

string mqtt_server = "";
string mqtt_topic = "";
string mqtt_user = "";
string mqtt_pass = "";

// -------------------- SUPPORT FUNCTIONS ---------------------------
// Prepare the RF24 radio

static void rf24setup(void){
#ifndef TEST
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
#endif
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

static bool ParseOptions(string OptsFilename)
{
    ifstream configStream;
    configStream.open(OptsFilename);
    if(configStream.is_open()) 
    {
        Json::Value config;
        configStream >> config;
        RFChannel = config.get("rfChannel", 120).asInt();
        SendAck = config.get("sendAck", false).asBool();

        if(config.isMember("emoncms"))
        {
            EmonCmsBaseUrl = config["emoncms"].get("url", "http://emoncms.org/").asString();
            EmonCmsApiKey = config["emoncms"].get("key", "").asString(); 
        }

        if(config.isMember("mqtt"))
        {
            mqtt_server = config["mqtt"].get("server", "").asString();
            mqtt_topic = config["mqtt"].get("topic", "").asString();
            mqtt_user = config["mqtt"].get("user", "").asString();
            mqtt_pass = config["mqtt"].get("pass", "").asString();
        }

        return true;
    }

    return false;
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
	CURL *curl;
	CURLcode res;
	
	printf("RF24Gateway\n");
	curl = curl_easy_init();

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

#ifndef TEST
	radio.startListening(); // Start listening for incoming messages
#else 
    temperature = 22.0;
#endif

	fflush(stdout);
	
    while (1)
	{
#ifndef TEST
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

		printf("Message received: %ld %s nodec=%d %6.2f DeviceID:%016llX Test:%d Relayed:%d\n",
                counter,
                currentDateTime().c_str(),
                message[Counter],
                temperature,
                deviceid,
                message[Test],
                message[Relayed]);

        if(SendAck) {
            radio.stopListening(); // stop listening so we can send an acknowledgement
            msleep(50);
		    radio.writeAckPayload(2,&message,13);
            msleep(50);
	        radio.startListening(); // Start listening for incoming messages
		    printf("ACK\n");
        }
#else
        sleep(1);
        deviceid = 0x822F762B3ACF2B22LL;
        temperature += rand() - 0.5;
#endif

		fflush(stdout);
		
		if("" != EmonCmsBaseUrl && "" != EmonCmsApiKey && NULL != curl)
		{
			int node = 10;
			char url[2048];
                        const char *base = EmonCmsBaseUrl.c_str();
                        const char *key = EmonCmsApiKey.c_str();
			
			sprintf(url, "%s/input/post.json?node=%d&json={%016llX_temp:%.2f}&apikey=%s", 
						 base, node, deviceid, temperature, key);
			
            curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

			
			// Perform the request, res will get the return code 
			printf("EmonCMS POST: "); fflush(stdout);
			res = curl_easy_perform(curl);
			printf("\n"); fflush(stdout);

			// Check for errors 
			if(res != CURLE_OK)
			{
			    fprintf(stderr, "curl_easy_perform() failed: %s\n",
						curl_easy_strerror(res));
			}
			
		}
		
	}

	// always cleanup
	curl_easy_cleanup(curl);

	//syslog (LOG_NOTICE, "NRFText terminated.");
	//closelog();
	
	return EXIT_SUCCESS;
}
