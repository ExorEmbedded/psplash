/*
 *  Contains the helper functions for Exor dedicated customizations.
 *
 *  Copyright (C) 2014 Exor s.p.a.
 *  Written by: Giovanni Pavoni Exor s.p.a.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "psplash.h"
#include "psplash-fb.h"
#include "customizations.h"
#include <linux/i2c-dev.h>
#include <dirent.h> 
#include <linux/input.h>

#define MAXPATHLENGTH 200

#define SPLASH_HDRLEN         56
#define SPLASH_STRIDE_IDX     0
#define SPLASH_SIZE_IDX       2

#define DEFAULT_SPLASHPARTITION          "/dev/mmcblk1p1"
#define PATHTOSPLASH                     "/mnt/.splashimage"
#define SPLASHFILENAME                   "/splashimage.bin"

#define BRIGHTNESSDEVICE                 "/sys/class/backlight/"

#define SEEPROM_I2C_ADDRESS              "0-0054"
#define SEEPROM_I2C_BUS                  "i2c-0"
#define I2CSEEPROMDEVICE                 "/sys/class/i2c-dev/"SEEPROM_I2C_BUS"/device/"SEEPROM_I2C_ADDRESS"/eeprom"
#define BLDIMM_POS                       128

#define DEFAULT_TOUCH_EVENT              "/dev/input/event0"
/***********************************************************************************************************
 STATIC HELPER FUNCTIONS
 ***********************************************************************************************************/

//Helper function to write the synchornization file with the JMloader
static int SyncJMLauncher(char* msg)
{
  char* tmpdir;
  char fullpath[MAXPATHLENGTH];
  
  //Filepath is defined by the TMPDIR env. variable; the default value is /tmp
  tmpdir = getenv("TMPDIR");
  if (!tmpdir)
    tmpdir = "/tmp";
  
  //Filename is "taptap"
  strncpy(fullpath, tmpdir, MAXPATHLENGTH);
  strncat(fullpath,"/taptap", MAXPATHLENGTH);
  
  // Opens the file and write the sync message
  FILE* fp;
  if((fp = fopen(fullpath, "w"))==NULL)
  {
    fprintf(stderr,"SyncJMLauncher cannot open file -> %s \n",fullpath);
    return -1;
  }
  
  fprintf(fp,"%s",msg);
  fflush(fp);
  fclose(fp);
  
  return 0;
}


//Helper function for writing a parameter to sysfs
static int sysfs_write(char* pathto, char* fname, char* value)
{
  char str[MAXPATHLENGTH];
  FILE* fp;
  
  strncpy(str,pathto,MAXPATHLENGTH);
  strncat(str,fname,MAXPATHLENGTH);
  
  if((fp = fopen(str, "ab"))==NULL)
  {
    fprintf(stderr,"Cannot open sysfs file -> %s \n",str);
    return -1;
  }
  
  rewind(fp);
  fwrite(value,1,strlen(value),fp);
  fclose(fp);
  return 0;
}


//Helper function for reading a parameter from sysfs
static int sysfs_read(char* pathto, char* fname, char* value, int n)
{
  char str[MAXPATHLENGTH];
  FILE* fp;
  
  strncpy(str,pathto,MAXPATHLENGTH);
  strncat(str,fname,MAXPATHLENGTH);
  
  if((fp = fopen(str, "rb"))==NULL)
  {
    fprintf(stderr,"Cannot open sysfs file -> %s \n",str);
    return -1;
  }
  
  rewind(fp);
  fread (value, 1, n, fp);
  fclose(fp);
  return 0;
}


//Helper function to execute a shell command (specified as string) and return 0 if command succeeds, -1 if failed
static int systemcmd(const char* cmd)
{
  int ret;
  
  ret = system(cmd);
  if (ret == -1)
    return ret;	// Failed to execute the system function
    
    if(WIFEXITED(ret))
    {
      if(0 != WEXITSTATUS(ret))
	return -1;	// The executed command exited normally but returned an error
	else
	  return 0;	// The executed command exited normally and returned 0 (=SUCCESS)
    }
    return -1;		// The executed command did not terminate properly 
}


// Helper function to read the brightness value from SEEPROM.
// A value in the range 0-255 is returned 
// NOTE: 0 means min. brightness, not backlight off.
// In case of error, 255 is returned, to be on the safe side.
static int get_brightness_from_seeprom()
{
  //1: Here we open the SEEPROM, which is supposed to be connected on the i2c-0 bus.
  FILE* fp;
  if((fp = fopen(I2CSEEPROMDEVICE, "rb"))==NULL)
  {
    fprintf(stderr, "psplash: Error eeprom_open: dev= %s err=%s\n", I2CSEEPROMDEVICE, strerror(errno));
    return 255;
  }
  
  //2: Now read the brightness value from the corresponding offset
  char buf = 255;
  
  fseek (fp, BLDIMM_POS, SEEK_SET);
  if(1 !=fread(&buf, 1, 1, fp))
    fprintf(stderr, "psplash: Error reading the eeprom: err=%s\n", strerror(errno));

  //3: Close and return value
  fclose(fp);
  return (((int)buf) & 0xff) ;
}


//Sets the brightness value as desired (without saving to i2c SEEPROM)
static int SetBrightness(char* brightnessdevice, int* pval)
{
  char strval[50];
  
  sprintf (strval,"%d", *pval);
  sysfs_write(brightnessdevice,"brightness",strval);
  return 0;
}

/***********************************************************************************************************
 Set the bootcounter (in NVRAM) to the specified value
 ***********************************************************************************************************/
void setbootcounter(unsigned char val)
{
  printf("setbootcounter %d \n",val);
}


/***********************************************************************************************************
 Drawing the custom splashimage from splashimage.bin file

 NOTE: In order to speed up loading time, it is nedeed that both framebuffer and splashimage are in RGB565
       format.
***********************************************************************************************************/
int psplash_draw_custom_splashimage(PSplashFB *fb)
{
  char * splashpartition; //Partition containing the splashimage.bin file
  
  if(fb->bpp != 16)
  {
    fprintf(stderr,"psplash: custom splashimage available only on 16bpp framebuffer \n");
    return -1;
  }
  
  // Get the splash partition from the environment or use the default partition 
  splashpartition = getenv("SPLASHPARTITION");
  if (splashpartition == NULL)
    splashpartition = DEFAULT_SPLASHPARTITION;
  
  // Mount the splash partition 
  char umount_cmd[] = "umount "PATHTOSPLASH; 
  char mkdir_cmd[] = "mkdir "PATHTOSPLASH; 
  systemcmd(mkdir_cmd);
  
  char mount_cmd[MAXPATHLENGTH] = "mount ";
  strncat(mount_cmd, splashpartition, MAXPATHLENGTH/2);
  mount_cmd[MAXPATHLENGTH/2] = 0;
  strcat(mount_cmd, " ");
  strcat(mount_cmd, PATHTOSPLASH);
  systemcmd(mount_cmd);
  
  //Try to open the splash file
  char splashfile[] = PATHTOSPLASH SPLASHFILENAME;
  
  FILE* fp;
  if((fp = fopen(splashfile, "rb"))==NULL)
  {
    fprintf(stderr,"psplash: cannot open splashimage file -> %s \n",splashfile);
    goto error;
  }
  
  // Gets the header of the SPLASH image and calculates the dimensions of the stored image. performs sanity checks
  unsigned int  splash_width;
  unsigned int  splash_height;
  unsigned int  splash_posx = 0;
  unsigned int  splash_posy = 0;
  unsigned int  header[SPLASH_HDRLEN];
  
  rewind(fp);
  
  if(fread(header, 1, SPLASH_HDRLEN, fp) != SPLASH_HDRLEN)
  {
    fprintf(stderr,"psplash: wrong splashimage file \n");
    goto error;
  }
  
  splash_width = (header[SPLASH_STRIDE_IDX]) / 2 + 1;

  if ((splash_width > fb->width) || (splash_width < 10))
  {
    fprintf(stderr,"psplash: splashimage width error: %d \n",splash_width);
    goto error;
  }
  
  splash_height = (((header[SPLASH_SIZE_IDX]) / 2) / splash_width);
  
  if (splash_height > (fb->height)) splash_height = (fb->height);
  if (splash_height < 10) 
  {
    fprintf(stderr,"psplash: splashimage height error: %d \n",splash_height);
    goto error;
  }
  
  // calculates the position of the splash inside the display
  splash_posx = ((fb->width)  - splash_width ) / 2;
  splash_posy = ((fb->height) - splash_height) / 2;
  
  //And now draws the splashimage
  int          x;
  int          y;
  uint8        red;
  uint8        green;
  uint8        blue;
  uint16*      stride;
  uint16       rgb565color;
  
  stride = (char*) malloc (2 * (splash_width + 1));
  if (stride==NULL)
  {
    fprintf(stderr,"psplash: malloc error\n");
    goto error;
  }
  
  for(y=0; y<splash_height; y++)
  {
    fread(stride, 2 * splash_width, 1, fp);
    for(x=0; x<splash_width; x++)
    {
      rgb565color = stride[x];
      blue  = (uint8)((rgb565color << 3) & 0x00ff);
      green = (uint8)((rgb565color >> 3) & 0x00ff);
      red   = (uint8)((rgb565color >>8) & 0x00ff);
      psplash_fb_plot_pixel (fb, splash_posx + x, splash_posy + y, red, green, blue);
    }
  }
  
  free(stride);
  fclose(fp);
  // UnMount the splash partition 
  systemcmd(umount_cmd);
  
  return 0;

error:  
  if(fp)
    fclose(fp);
  // UnMount the splash partition 
  systemcmd(umount_cmd);
  
  return -1;
}


/***********************************************************************************************************
 Updating the backlight brightness value with the one stored in I2C SEEPROM

 NOTE: Scaling is done to properly map the range [0..255] of the I2C SEEPROM stored value with the range 
       [1..max_brightness], which is the available dynamic range for the backlight driver.
 ***********************************************************************************************************/
void UpdateBrightness()
{
  int max_brightness;
  int target_brightness;
  
  char strval[5]={0,0,0,0,0};
  char brightnessdevice[MAXPATHLENGTH] = BRIGHTNESSDEVICE;
  
  // Get the full path for accessing the backlight driver: we should have an additonal subdir to be appended to the hardcoded path
  DIR           *d;
  struct dirent *dir;
  d = opendir(BRIGHTNESSDEVICE);
  if (d)
  {
    while ((dir = readdir(d)) != NULL)
    {
      if(dir->d_name[0] != '.')
      {
	strcat(brightnessdevice, dir->d_name);
	strcat(brightnessdevice,"/");
	break;
      }
    }
    closedir(d);
  }
  
  // Read the max_brightness value for the backlight driver and perform sanity check
  sysfs_read(brightnessdevice,"max_brightness",strval,3);
  max_brightness=atoi(strval);
  
  if((max_brightness < 1) || (max_brightness > 255))
    max_brightness = 100;
  
  // Read the target brightness from SEEPROM and perform scaling to suit the dynamic range of the backlight driver
  target_brightness = get_brightness_from_seeprom();
  
  target_brightness = 1 + (target_brightness * (max_brightness) - target_brightness / 2) / 255;
  
  if(target_brightness > max_brightness)
    target_brightness = max_brightness;
  
  if(target_brightness < 1)
    target_brightness = 1;
    
  // Transition loop to set the actual brightness value
  int usdelay = 1000000 / target_brightness;
  int i;
  
  for(i=1; i < target_brightness; i++)
  {
    SetBrightness(brightnessdevice, & i);
    usleep(usdelay);
  }
  
  SetBrightness(brightnessdevice, &target_brightness);
}


/***********************************************************************************************************
 Opening the touchscreen event for reading.

 ret = int touch_fd = file descriptor (< 0 if error)
 
 NOTE: The touch event can be defined by the "TSDEVICE" environment var. If TSDEVICE not defined, the
       default "/dev/input/event0" event is used.
 ***********************************************************************************************************/
int Touch_open()
{
  int touch_fd = -1;
  char *tsdevice = NULL;

  if( (tsdevice = getenv("TSDEVICE")) != NULL ) 
  {
    touch_fd = open(tsdevice,O_RDONLY | O_NONBLOCK);
  } 
  else 
  {
    touch_fd = open(DEFAULT_TOUCH_EVENT,O_RDONLY | O_NONBLOCK);
  }
  
  if(touch_fd < 0)
    fprintf(stderr, "psplash: Error opening the touch event: err=%s\n", strerror(errno));
  
  return touch_fd;
}


/***********************************************************************************************************
 Closing the touchscreen file descriptor.
 ***********************************************************************************************************/
void Touch_close(int touch_fd)
{
  if(touch_fd < 0)
    return;
  
  close(touch_fd);
}


/***********************************************************************************************************
 Touch handler: counts the number of tap-tap events detected and gets the last detected touch status.
 
 int  touch_fd   (file descriptor to touchscreen event)
 int* taptap     (taptap detected number)
 int* laststatus (0=up, 1=pressed)
 
 int ret = number of detected UP/DOWN events (0=nothing happened)
 ***********************************************************************************************************/
int Touch_handler(int touch_fd, int* taptap, int* laststatus)
{
  struct input_event ev;
  fd_set fdset;
  struct timeval tv;
  int count = 0;
  int nfds;
  int ret;
  
  if(touch_fd < 0)
    return 0;
  
  while (1) 
  {
    FD_ZERO(&fdset);
    FD_SET(touch_fd, &fdset);
    
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
    nfds = select(touch_fd + 1, &fdset, NULL, NULL, &tv);
    if (nfds == 0) 
      break;
    
    ret = read(touch_fd, &ev, sizeof(struct input_event));
    if (ret < (int)sizeof(struct input_event)) 
    {
      break;
    }
    
    if (ev.type == EV_KEY)
      if((ev.code == BTN_TOUCH) || (ev.code == BTN_LEFT))
      {
	if(ev.value == 0)
	{ //pen UP 
	  *taptap = *taptap + 1;
	  *laststatus = 0;
	  count++;
	}
	else if(ev.value == 1)
	{ //pen DW
	  *laststatus = 1;
	  count++;
	}
      }
  }
  
  return count;
}


/***********************************************************************************************************
 TapTap_Progress: Updates the tap-tap counting on display, to give a visual feedback to the user

 PSplashFB* fb   (pointer to framebuffer structure)
 int taptap      (taptap detected number)
 ***********************************************************************************************************/
void TapTap_Progress(PSplashFB *fb, int taptap)
{
  extern void  psplash_draw_msg (PSplashFB *fb, const char *msg);
  
  // Exit in case the actual taptap counter is still too low
  if(taptap <= TAPTAP_THLO)
    return;

  // Build a string representing the actual TAP-TAP counter value
  char msg[MAXPATHLENGTH] = "Tap-tap : ";
  int i;
  
  for(i=0; i<TAPTAP_TH; i++)
  {
    if(i<taptap)
      strcat(msg,"#");
    else
      strcat(msg,".");
  }
  
  // Draw the string
  psplash_draw_msg (fb, msg);
}


/***********************************************************************************************************
 TapTap_Detected: Handles the sequence for deciding what to do when TAP-TAP detected.

 PSplashFB* fb   (pointer to framebuffer structure)
 int laststatus  (last touchscreen status)

 output: int ret (1=exit, 0=continue)
 ***********************************************************************************************************/
int TapTap_Detected(int touch_fd, PSplashFB *fb, int laststatus)
{
  extern void  psplash_draw_msg (PSplashFB *fb, const char *msg);
  extern int reboot(int cmd);
  
  int refreshtrigger = 0xff;
  int time; //Time [s/200]
  char msg[MAXPATHLENGTH];
  int taptap = 0;
  
  // Perform synchronization with the JMlauncher: put it in wait status
  SyncJMLauncher("wait");
  
  // Perform countdown, touch status reading and msg updating, based on touch status (pressed or not pressed)
  for(time = 1000; time > 0; time -= 50)
  {
    if((time%200) == 0)    //refresh printout at least every second
      refreshtrigger = 0xff;
    
    Touch_handler(touch_fd, &taptap, &laststatus);
    if(laststatus != refreshtrigger)
    { //It is time to refresh the printout
      refreshtrigger = laststatus;
      if(laststatus == 0) 
	sprintf(msg, "** TAP-TAP DETECTED  %d **\n>> RESTART: RECOVERY OS\n   SYSTEM SETTINGS\n",(int)(time/200));
      else
	sprintf(msg, "** TAP-TAP DETECTED  %d **\n   RESTART: RECOVERY OS\n>> SYSTEM SETTINGS\n",(int)(time/200));
      // Draw the string
      psplash_draw_msg (fb, msg);
    }
    usleep(200000);
  }
  
  // Now, based on the touchscreen laststatus (pressed or not pressed) the proper action will be taken ...
  if(laststatus == 0)
  { // In this case we will restart the recovery OS
    sprintf(msg, "** TAP-TAP DETECTED  %d **\n\nRESTARTING: RECOVERY OS ...\n",(int)(time/200));
    psplash_draw_msg (fb, msg);
    usleep(2000000);
    
    // The recovery OS is forced to boot by setting the bootcounter over the threshold limit
    setbootcounter(100); 
    //Now perform reboot unconditionally (please note the JMloader is still kept into the "wait" status, so it will not try booting anything in the meanwhile)
    #define LINUX_REBOOT_CMD_RESTART        0x01234567
    sync();
    reboot(LINUX_REBOOT_CMD_RESTART);    
    //We should never get here !!!
    while(1)
      usleep(200);
  }
  else
  { // In this case we will inform the JMloader to start the system settings menu by setting the "disable-kiosk" status, then we will normally exit
    sprintf(msg, "** TAP-TAP DETECTED  %d **\n\nENTERING SYSTEM SETTINGS ...\n",(int)(time/200));
    psplash_draw_msg (fb, msg);
    usleep(2000000);
    SyncJMLauncher("disable-kiosk");
  }
  return 0;
}
