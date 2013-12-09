/**
 * RCSwitch daemon for the Raspberry Pi
 *
 * Setup
 *   Power to pin4
 *   GND   to pin6
 *   DATA  to pin11/gpio0
 *
 * Usage
 *   send xxxxxyyz to ip:port
 *   xxxxx  encoding
 *          00001 for first channel
 *   yy     plug
 *          01 for plug A
 *   z      action
 *          0:off|1:on|2:status
 *
 *  Examples of remote actions
 *    Switch plug 01 on 00001 to on
 *      echo 00001011 | nc 192.168.1.5 12345
 *
 *    Switch plug 01 on 00001 to off
 *      echo 00001010 | nc 192.168.1.5 12345
 *
 *    Get status of plug 01 on 00001
 *      echo 00001012 | nc 192.168.1.5 12345
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "daemon.h"
#include "RCSwitch.h"

RCSwitch mySwitch;

int main(int argc, char* argv[]) {
  /**
   * Setup wiringPi and RCSwitch
   * set high priority scheduling
   */
  if (wiringPiSetup () == -1)
    return 1;
  piHiPri(20);
  mySwitch = RCSwitch();
  mySwitch.setPulseLength(300);
  usleep(50000);
  mySwitch.enableTransmit(0);

  nPlugs=10;
  int nState[nPlugs];
  nTimeout=0;
  memset(nState, 0, sizeof(nState));

  /**
   * setup socket
   */
  int sockfd, newsockfd, portno;
  socklen_t clilen;
  char buffer[256];
  struct sockaddr_in serv_addr, cli_addr;
  int n;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");
  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = PORT;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");

  /*
   * start listening
   */
  while (true) {
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd,
                (struct sockaddr *) &cli_addr,
                &clilen);
    if (newsockfd < 0)
      error("ERROR on accept");
    bzero(buffer,256);
    n = read(newsockfd,buffer,255);
    if (n < 0)
      error("ERROR reading from socket");

    if (strlen(buffer) >= 8) {
      for (int i=0; i<5; i++) {
        nGroup[i] = buffer[i];
      }
      nGroup[5] = '\0';

      for (int i=5;i<7; i++) {
        nSwitchNumber = buffer[i]-48;
      }
      nAction = buffer[7]-48;
      nTimeout=0;
	/*
      if (strlen(buffer) >= 9) nTimeout = buffer[8]-48;
      if (strlen(buffer) >= 10) nTimeout = nTimeout*10+buffer[9]-48;
      if (strlen(buffer) >= 11) nTimeout = nTimeout*10+buffer[10]-48;
      if(nTimeout < 0)  nTimeout = 0;
	*/

      // printf("code: %s | unit: %d | action: %d | timeout: %d \n", nGroup, nSwitchNumber, nAction, nTimeout);

      /**
       * handle messages
       */
      int nAddr = getAddr(nGroup, nSwitchNumber);
printf("nAddr: %d", nAddr);
      char msg[13];
        switch (nAction) {
          /**
           * off
           */
          case 0:
            // mySwitch.switchOff(nGroup, nSwitchNumber);
            nState[nAddr] = 0;
            sprintf(msg, "%d", nState[nAddr]);
            n = write(newsockfd,msg,1);
            piThreadCreate(switchOff);
            break;
          /**
           * on
           */
          case 1:
            // piThreadCreate(switchOn);
            // mySwitch.switchOn(nGroup, nSwitchNumber);
            nState[nAddr] = 1;
            sprintf(msg, "%d", nState[nAddr]);
            n = write(newsockfd,msg,1);
            piThreadCreate(switchOn);
            break;
          /**
           * status
           */
          case 2:
            sprintf(msg, "%d", nState[nAddr]);
            n = write(newsockfd,msg,1);
            break;
        }
    }
    else {
      printf("message corrupted or incomplete");
    }


    if (n < 0)
      error("ERROR writing to socket");
  }

  /**
   * terminate
   */
  close(newsockfd);
  close(sockfd);
  return 0;
}

/**
 * error output
 */
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/**
 * calculate the array address of the power state
 */
int getAddr(const char* nGroup, int nSwitchNumber) {
  int i=0,len=0,r=0,w;
  len = strlen(nGroup);
  for(i=0;i<len;i++)
  {
          w=pow(2,i);
          r+=(nGroup[i]-48)*w;
  }

  return r + nSwitchNumber;
}

PI_THREAD(switchOn) {
  printf("switchOnThread: %d\n", nTimeout);
  char tGroup[6];
  int tSwitchNumber;
  memcpy(tGroup, nGroup, sizeof(tGroup));
  tSwitchNumber = nSwitchNumber;
  printf("######## code: %s | unit: %d | timeout: %d \n", nGroup, tSwitchNumber, nTimeout);
  // sleep(nTimeout*60);
  mySwitch.switchOn(tGroup, tSwitchNumber);
  return 0;
}

PI_THREAD(switchOff) {
  printf("switchOffThread: %d\n", nTimeout);
  char tGroup[6];
  int tSwitchNumber;
  memcpy(tGroup, nGroup, sizeof(tGroup));
  tSwitchNumber = nSwitchNumber;
  printf("code: %s | unit: %d | timeout: %d \n", nGroup, tSwitchNumber, nTimeout);
  // sleep(nTimeout*60);
  mySwitch.switchOff(tGroup, tSwitchNumber);
  return 0;
}

