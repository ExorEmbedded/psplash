#include "common.h"

#define NVRAMDEVICE                      "/sys/class/rtc/rtc0/device/nvram"

/***********************************************************************************************************
 Set the bootcounter (in NVRAM) to the specified value
 ***********************************************************************************************************/
int setbootcounter(unsigned char val)
{
  #define NVRAM_MAGIC 0xbc
  unsigned char buff[2];
  
  buff[0] = NVRAM_MAGIC;
  buff[1] = val;
  
  // Opens the nvram device and updates the bootcounter
  FILE* fp;
  if((fp = fopen(NVRAMDEVICE, "w"))==NULL)
  {
    fprintf(stderr,"setbootcounter cannot open file -> %s \n",NVRAMDEVICE);
    return -1;
  }
  
  fwrite(buff,1,2,fp);
  fflush(fp); //Just in case ... but not really needed
  fclose(fp);
  
  return 0;
}
