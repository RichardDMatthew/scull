/* sculltest.c
 * A simple example of a C program to test some of the
 * operations of the "/dev/scull" device (a.k.a "scull0"),
 * and the 
 * ($Id: sculltest.c,v 1.1 2010/05/19 20:40:00 baker Exp baker $)
 */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
   int fd, result, len;
   char buf[10];
   const char *str;
   if ((fd = open("/dev/scull", O_WRONLY)) == -1) {
      perror("1. open failed");
      return -1;
   }

   str = "abcde"; 
   len = strlen(str);
   if ((result = write(fd, str, len)) != len) {
      perror("1. write failed");
      return -1;
   }
   close(fd);

   if ((fd = open("/dev/scull", O_RDONLY)) == -1) {
      perror("2. open failed");
      return -1;
   }
   if ((result = read(fd, &buf, sizeof(buf))) != len) {
      fprintf(stdout, "1. read failed, buf=%s",buf);
      return -1;
   } 
   buf[result] = '\0';
   if (strncmp (buf, str, len)) {
      fprintf (stdout, "failed: read back \"%s\"\n", buf);
   } else {
      fprintf (stdout, "passed\n");
   }
   close(fd);
   
   
   str = "xyz"; len = strlen(str);
   if ((fd = open ("/dev/scullpipe", O_RDWR)) == -1) {
      perror("3. open failed");
      return -1;
   }
   if ((result = write (fd, str, len)) != len) {
      perror("3. write failed");
      return -1;
   }
   if ((result = read (fd, &buf, sizeof(buf))) != len) {
      perror("3. read failed");
      return -1;
   }
   buf[result] = '\0';
   if (strncmp (buf, str, len)) {
      fprintf (stdout, "failed: read back \"%s\"\n", buf);
   } else {
      fprintf (stdout, "passed\n");
   }
   close(fd);
   return 0;
   
}