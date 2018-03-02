#include "watermeterMQTT.h"

using namespace std;

// Device is a comport like /dev/ttyUSB1

string device = "/dev/ttyUSB1";
string datafile = "watermeter.dat";

const char *client_name = "watermeter"; 	// -c
const char *ip_addr     = "127.0.0.1";		// -i
uint32_t    port        = 1883;			// -p
const char *topic       = "home/watermeter/m3";	// -t


#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

double read_waterreading (const char* file_name)
{
	double i = 0;
	FILE* file = fopen (file_name, "r");
	if (file)
	{
		if (!feof(file)) fscanf (file, "%lf", &i);      
		fclose (file);        
	}
	return i;
}

void write_waterreading (const char* file_name, double waterreading)
{
	FILE* file = fopen (file_name, "w");
	fprintf (file, "%.3lf", waterreading);    
	fclose (file);        
}

// read the current level from DCD pin
int get_cts_state(int fd)
{
	int serial = 0;
	if(ioctl(fd, TIOCMGET, &serial) < 0)
	{
		printf("getctsstate ioctl() failed: fd:%d, %d: %s\n", fd, errno, strerror(errno));
		return -1;
	}

	return (serial & TIOCM_CTS) ? 1 : 0;
}

uint64_t get_posix_clock_time ()
{
    struct timespec ts;

    if (clock_gettime (CLOCK_MONOTONIC, &ts) == 0)
        return (uint64_t) (ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
    else
        return 0;
}

int   main(int argc, char * argv[])
{
	if (argc > 1)
	{
		printf ("\nReading configfile: %s\n", argv[1]);
		FILE *conf_fp = fopen (argv[1], "r");
		bool inisectionfound = false;
		while(!feof(conf_fp)) 
		{
			char iniline[100];  
			fscanf (conf_fp,"%s",iniline);
			if (iniline[0] == '[') inisectionfound = false;
			if (inisectionfound)
			{
				char name[100];
				char setting[100];
				sscanf(iniline, "%[^=]=%s", name, setting);
				if (strcasecmp(name, "serialdevice") == 0) device = string(setting);
				if (strcasecmp(name, "datafile") == 0) datafile = string(setting);
				if (strcasecmp(name, "tcpport") == 0) port = atoi(setting);
			}
			if (strcasecmp(iniline, "[watermeter]") == 0) inisectionfound = true;
		}
	}

	printf ("device = %s\n", device.c_str());
	printf ("datafile = %s\n", datafile.c_str());
	printf ("port = %d\n", port); 

	double waterreading_m3 = 0;
	waterreading_m3 = read_waterreading (datafile.c_str());
	double waterreading_m3_old = -1;
	double waterflow_m3h = 0;
	double waterflow_m3h_old = -1;

	
	int pipefd[2];
	pid_t cpid;
	char buf;
	mqtt_broker_handle_t *broker = 0;

	pipe(pipefd); // create the pipe
	cpid = fork(); // duplicate the current process
	if (cpid == 0) // if I am the child then
	{

		// Child is worker for TCP connections and database writes
		close(pipefd[1]); // close the write-end of the pipe, I'm not going to use it

		/* Initialize the timeout data structure. */
		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		/* select returns 0 if timeout, 1 if input available, -1 if error. */
		while(1)
		{

			if(broker == 0) {
			        puts("Connectiong to MQTT broker...");
				broker = mqtt_connect(client_name, ip_addr, port);
			}
 
			if (broker)
			{
				//printf ("%f = %f = %d\n", waterflow_m3h, waterflow_m3h_old, waterflow_m3h != waterflow_m3h_old);
				if (waterflow_m3h != waterflow_m3h_old)
				{
					char msg[80];
					waterflow_m3h_old = waterflow_m3h;
					sprintf (msg, "%.3f", waterflow_m3h);
	 				printf ("MQTT Sending: home/watermeter/m3h:%s\n",msg);
					if(mqtt_publish(broker, "home/watermeter/m3h", msg, QoS0, 1) == -1) 
					{
						printf("publish failed\n");
					}
				}
				//printf ("%f = %f = %d\n", waterreading_m3, waterreading_m3_old, waterreading_m3 != waterreading_m3_old);

				if (waterreading_m3 != waterreading_m3_old)
				{
					char msg[80];
					waterreading_m3_old = waterreading_m3;
					sprintf (msg, "%.3f", waterreading_m3);
					printf ("MQTT Sending: home/watermeter/m3:%s\n",msg);
					if(mqtt_publish(broker, "home/watermeter/m3", msg, QoS0, 1) == -1) 
					{
						printf("publish failed\n");
					}
				}
			}
			
			/* Initialize the file descriptor set. */
			fd_set set;
			FD_ZERO (&set);
			FD_SET (pipefd[0], &set);
			
			select (FD_SETSIZE,&set, NULL, NULL, &timeout);
			if (FD_ISSET(pipefd[0], &set))
			{
				// Received new watermeter values from Parent!
				char msg[80];
				bzero(msg, 80);
				if (!read(pipefd[0], &msg, 80)) 
				{
					printf ("Pipe to parent watermeter process has broken, exit..\n");
					exit(1);
				}
				
				int ctsstate;
				sscanf(msg, "%lf %lf %d", &waterreading_m3, &waterflow_m3h, &ctsstate);

				/* Re-Initialize the timeout data structure. */
				timeout.tv_sec = 10;
				timeout.tv_usec = 0;
				
			}
			else
			{
				/* Re-Initialize the timeout data structure. */
				timeout.tv_sec = 10;
				timeout.tv_usec = 0;
				
				// Select timeout
				if (waterflow_m3h > 0.001)
				{
					if (waterflow_m3h > 0.18) waterflow_m3h = 0.18;
					else waterflow_m3h = waterflow_m3h / 2;
				}
				else waterflow_m3h = 0;
			}
		}

		close(pipefd[0]); // close the read-end of the pipe
		exit(EXIT_SUCCESS);
	}
	else 
	// ##### THIS IS THE PARENT THAT READS THE PULSES FROM THE WATERMETER AND INFORMS THE CLIENT ####
	{
		// Parent does reading the water meter
		close(pipefd[0]); // close the read-end of the pipe, I'm not going to use it

		int omode = O_RDONLY;


		// open the serial stream
		int fd;
		fd = open(device.c_str(), omode, 0777);
		while (fd <= 0)
		{
			fd = open(device.c_str(), omode, 0777);
			printf("Error opening serial device %s: %s\n", device.c_str(), strerror(errno));
			sleep(1);
		}

		printf("Device opened: %s, fd:%d\n", device.c_str(),fd);


		// detect DCD changes forever
		int i=0;
		int ctsstate= get_cts_state(fd);
		int pctsstate = ctsstate;

		uint64_t tv_nsecold =  get_posix_clock_time();
		while(1)
		{
			printf("Waterreading_m3 = %.3f, Waterflow_m3h= %.3f, ctsstate=%d\n", waterreading_m3, waterflow_m3h,  ctsstate);
			fflush(stdout);

			// block until line changes state
			if(ioctl(fd, TIOCMIWAIT, TIOCM_CTS) < 0)
			{
				printf("waiting for interrupt ioctl(TIOCMIWAIT, TIOCM_CTS) failed: %d: %s\n", errno, strerror(errno));
				sleep(1);
			}

			pctsstate = ctsstate;
			ctsstate = get_cts_state(fd);
			if ((ctsstate != pctsstate) && (ctsstate == 1))
			{
				// Calculate waterflow
				uint64_t tv_nsecnew =  get_posix_clock_time();
				long ms = round ((tv_nsecnew - tv_nsecold) / 1000);
				tv_nsecold = tv_nsecnew;
				printf ("pulse = %ld ms\n", ms);
				waterflow_m3h = (double)((0.0005 * 1000  * 3600) / ms);
				
				// Calculate waterreading
				waterreading_m3+=0.001;
				
				// Write waterreading to file
				write_waterreading(datafile.c_str(), waterreading_m3);
				
				// Send values to child
				char msg[80];
				sprintf (msg,"%.3f %.3f %d", waterreading_m3, waterflow_m3h, ctsstate);
				write(pipefd[1], msg, strlen(msg)); // send values to child
			}
		}

		close(pipefd[1]); // close the write-end of the pipe, thus sending EOF to the reader
		wait(NULL); // wait for the child process to exit before I do the same
		exit(EXIT_SUCCESS);
	}
	return 0;
}
