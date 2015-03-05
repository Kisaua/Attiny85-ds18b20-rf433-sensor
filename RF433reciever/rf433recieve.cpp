#include <wiringPi.h>
#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <sched.h>
#include <sstream>
#include <stdint.h>


using namespace std;

//store data into array 
#define MAX_CHANGE_TO_KEEP 86
static int durations[MAX_CHANGE_TO_KEEP];
#define NUMBYTES 5
unsigned char data[NUMBYTES];
const char* value [NUMBYTES] = {
					"",
					"Battery charge, BC = ", 
					"DS 18b20 temperature HByte, DSHB = ",
					"DS 18b20 temperature LByte, DSLB = ",
					"CRC ="
};


#define DECOUPLING_MASK 0b11001010
 
//void log(string a){
 // cout << a ;
//}

string longToString(long mylong){
    string mystring;
    stringstream mystream;
    mystream << mylong;
    return mystream.str();
}

string intToString(int myint){
    string mystring;
    stringstream mystream;
    mystream << myint;
    return mystream.str();
}

string measure2String(long duration){
    string mystring;
    stringstream mystream;
    mystream << duration;
    return mystream.str();
}

/******************************************************************************\
|* Compute an 8-bit CRC 
|*
|* Pass a seed of 0 to start the CRC of a byte-stream. Pass the result as 
|* the seed for subsequent bytes
\******************************************************************************/
unsigned char oneWireCrc8(unsigned char inData, unsigned char seed)
	{
    unsigned char bitsLeft;
    unsigned char temp;

    for (bitsLeft = 8; bitsLeft > 0; bitsLeft--)
    	{
        temp = ((seed ^ inData) & 0x01);
        if (temp == 0)
            seed >>= 1;
        else
        	{
            seed ^= 0x18;
            seed >>= 1;
            seed |= 0x80;
        	}
        inData >>= 1;
    	}
    return seed;    
	}
/******************************************************************************\
|* Compute the CRC for a data for identifier
\***********************oneWireDataCrc*******************************************************/
unsigned char oneWireDataCrc(unsigned char * data)
	{
    unsigned char i;
    unsigned char crc8 = 0;
    
    for (i = 0; i < 4; i++)
    	{
        crc8 = oneWireCrc8(*data, crc8);
        data++;
    	}
    return crc8;
	}

	
void handleInterrupt() {
 
  static unsigned int duration;
  static unsigned int changeCount;
  static unsigned long lastTime;
  static unsigned char error;

  static int lockPassed;
  static int lockChange;

  long time = micros();
  duration = time - lastTime;

  //wait for lock
  if (lockChange == 0 && duration > 22000 && duration < 24000 && lockPassed == 0) { 
    changeCount = 0;
    //log("Lock started"); cout << endl;
    //log(measure2String(duration));cout << endl;
    //store duration
    durations[changeCount++] = duration;
    lockChange = 1;
  } else if (lockChange == 1 && duration > 7000 && duration < 9000 && lockPassed == 0) {
    //log("Lock started 2"); cout << endl;
    //log(measure2String(duration));cout << endl;
    durations[changeCount++] = duration;
    lockChange = 2;
  } else if (lockChange == 2 && duration > 3500 && duration < 4500 && lockPassed == 0) {
    //log("Lock started 3"); cout << endl;
    //log(measure2String(duration));cout << endl;
    durations[changeCount++] = duration;
    lockChange = 3;
    //lockPassed = 1; //lock is finished
  } else if (lockChange == 3 && duration > 1100 && duration < 1500 && lockPassed == 0) {
   // log("Lock started 4"); cout << endl;
    //log(measure2String(duration));cout << endl;
    durations[changeCount++] = duration;
    lockChange = 4;
    lockPassed = 1; //lock is finished
  }


  else if (lockPassed != 0) { // REST OF DATA
    //log(measure2String(duration));

    if (changeCount > MAX_CHANGE_TO_KEEP) { // store 100 change Max
      //ended
      lockPassed = 0;
      changeCount = 0;
      lockChange = 0;
	  error  = 0;
	  static int j=0; //
	  uint8_t newData = 0;
	  uint8_t mask = 0b10000000;
	  
      //log("===============================");
      //start after the lock
      for (int i=4; i < MAX_CHANGE_TO_KEEP; i=i+2) {
        
		 if ((i-4)%16==0) {
          //space each 8bits
		  //cout << newData;
		  data[j] = newData ^ DECOUPLING_MASK;
		  mask = 0b10000000;
		  newData = 0;
		  j=(i-4)/16;
        //  cout << " ";
        }

        if (durations[i] > 600 && durations[i] < 950 &&
            durations[i+1] > 600 && durations[i+1] < 950  ) {
            //a 0
           // cout << "0";
            mask >>= 1;
        } else if (durations[i] > 600 && durations[i] < 950 &&
            durations[i+1] > 1500 && durations[i+1] < 1850  ) {
            //a 0
           // cout << "1";
			newData |= mask;
            mask >>= 1;
        } else {
          //errror reset
		  error  = 1;
          //log ("EROOR");cout << endl;
          //log(measure2String(durations[i]));cout << " ";
          //log(measure2String(durations[i+1]));cout << endl;
          changeCount = 0;
          lockChange = 0;
          lockPassed = 0;
          break;
        }
		
      }
      //cout << endl;
      //chesk crs for array
	  
	  if (error == 0 ) {
	  FILE *myfile;
	  myfile = fopen ("78/data", "w+");	  
	  if (myfile == NULL)
        printf("ERROR open File");
     else{
	 //cout << data[0];
	  //cout << " ";
	  char datacrc;
	  //datacrc = oneWireDataCrc(data);
	  if (oneWireDataCrc(data) == data [4]) {
	    for (int i = 0; i < NUMBYTES; i++) {
	    fprintf (myfile, "%s %u\n", value[i], data[i]);
	    //cout << static_cast<unsigned> (data[i]);
	    //cout << " ";
	    }
	  }
	  //cout << static_cast<unsigned> (datacrc);
	  //cout << time;
	  //fprintf (myfile, "%s\n", time);
	  }
	  fclose(myfile);
      //cout << endl;
	  
	  }
    } else {
      //store duration
      durations[changeCount++] = duration;
    }
  } else {
    //wait for another 
    changeCount = 0;
    lockChange = 0;
    lockPassed = 0;
  }

  lastTime = time;  
}



int main(void) {
  if(wiringPiSetup() == -1)
       return 0;

	//attach interrupt to changes on the pin
	wiringPiISR(2, INT_EDGE_BOTH, &handleInterrupt);
  //interrupt will handle changes
  while(true);
}
