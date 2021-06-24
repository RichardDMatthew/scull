/* swaphintstest.c
 * A simple example of a C program to test some of the
 * operations of the "/dev/swaphints" device (a.k.a "swaphints0"),
 * and the 
 * ($Id: swaphintstest.c,v 1.1 2010/05/19 20:40:00 baker Exp baker $)
 */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "swaphints.h"

int main() {
   int fd, result, len;
   char buf[10];
   const char *str;

   swaphints_request_t *requests;
   swaphints_response_t *responses;

   requests = (swaphints_request_t*)malloc(sizeof(swaphints_request_t));
   responses = (swaphints_response_t*)malloc(sizeof(swaphints_response_t));
  
   if ((fd = open("/dev/swaphints", O_WRONLY)) == -1) {
      perror("1. open failed");
      return -1;
   }

   for(uint64_t i = 0; i < MAX_PFNCOUNT; i++){
      requests->pfns[i] = i;
   }
   requests->count = MAX_PFNCOUNT;

   for(uint64_t i = 0; i < MAX_PFNCOUNT; i++){
      printf("%lu,",requests->pfns[i]);
   }
   printf("\n count is %u\n", requests->count);

   if ((result = ioctl(fd, SWAPHINTS_SEND_REQUEST, requests)) != 0) {
      fprintf(stdout, "1. requests failed - result %d", result);
      return -1;
   } 

   if ((result = ioctl(fd, SWAPHINTS_GET_RESPONSE, responses)) != 0) {
      fprintf(stdout, "1. responses failed - result %d", result);
      return -1;
   } 

   for(uint64_t i = 0; i < MAX_PFNCOUNT; i++){
      printf("%lu,",responses->returncodes[i]);
   }
   printf("\n count is %u\n", responses->count);



   close(fd);
   return 0;
   
}