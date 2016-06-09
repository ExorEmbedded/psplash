#include "common.h"

#define BOOT1DEVICE                      "/dev/mmcblk1boot1"
#define BOOT1ROSYSFS                     "/sys/block/mmcblk1boot1/force_ro"

/***********************************************************************************************************
 Set the bootcounter (in eMMC) to the specified value
 ***********************************************************************************************************/
int setbootcounter(unsigned char val)
{
  #define BC_MAGIC 0xbc
  unsigned char buff[2];

  buff[0] = BC_MAGIC;
  buff[1] = val;

  FILE* sysfs_ro = fopen(BOOT1ROSYSFS, "w");
  FILE* fp = fopen(BOOT1DEVICE, "wb");

  if(fp == NULL || sysfs_ro == NULL)
  {
    fprintf(stderr,"setbootcounter cannot open file -> %s \n",BOOT1DEVICE);
    return -1;
  }

  if (fseek(fp, 0x80000, SEEK_SET))
  {
    fprintf(stderr,"setbootcounter cannot open file -> %s \n",BOOT1DEVICE);
    return -1;
  }

  fputs("0", sysfs_ro);
  fflush(sysfs_ro);

  fwrite(buff,1,2,fp);
  fflush(fp);
  fclose(fp);

  fputs("1", sysfs_ro);
  fflush(sysfs_ro);
  fclose(sysfs_ro);

  return 0;
}
