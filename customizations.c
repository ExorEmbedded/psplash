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

#define MAXPATHLENGTH 200

#define SPLASH_HDRLEN         56
#define SPLASH_STRIDE_IDX     0
#define SPLASH_SIZE_IDX       2

#define DEFAULT_SPLASHPARTITION          "/dev/mmcblk1p1"
#define PATHTOSPLASH                     "/mnt/.splashimage"
#define SPLASHFILENAME                   "/splashimage.bin"

/***********************************************************************************************************
 STATIC HELPER FUNCTIONS
 ***********************************************************************************************************/
 
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
  char mkdir_cmd[] = "mkdir "PATHTOSPLASH; 
  systemcmd(mkdir_cmd);
  
  char mount_cmd[MAXPATHLENGTH] = "mount ";
  strncat(mount_cmd, splashpartition, MAXPATHLENGTH/2);
  mount_cmd[MAXPATHLENGTH/2] = 0;
  strcat(mount_cmd, " ");
  strcat(mount_cmd, PATHTOSPLASH);
  fprintf(stderr,"-> %s \n", mount_cmd);
  systemcmd(mount_cmd);
  
  //Try to open the splash file
  char splashfile[] = PATHTOSPLASH SPLASHFILENAME;
  
  FILE* fp;
  if((fp = fopen(splashfile, "rb"))==NULL)
  {
    fprintf(stderr,"psplash: cannot open splashimage file -> %s \n",splashfile);
    return -1;
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
  return 0;

error:  
  if(fp)
    fclose(fp);
  
  return -1;
}