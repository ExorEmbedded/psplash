/* 
 *  pslash - a lightweight framebuffer splashscreen for embedded devices. 
 *
 *  Copyright (c) 2006 Matthew Allum <mallum@o-hand.com>
 *
 *  Parts of this file ( fifo handling ) based on 'usplash' copyright 
 *  Matthew Garret.
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
#include "psplash-poky-img.h"
#include "psplash-bar-img.h"
#include "radeon-font.h"
#include "customizations.h"

//Global variable indicating the font scale factor: 0=>1x 1=>2x 2=>4x
int FONT_SCALE;

void
psplash_exit (int signum)
{
  DBG("mark");

  psplash_console_reset ();
}

void
psplash_draw_msg (PSplashFB *fb, const char *msg)
{
  int w, h;

  psplash_fb_text_size (fb, &w, &h, &radeon_font, msg);

  DBG("displaying '%s' %ix%i\n", msg, w, h);

  /* Clear */

  psplash_fb_draw_rect (fb, 
			0, 
			10, 
			fb->width,
			h+10,
			PSPLASH_TEXTBK_COLOR);

  psplash_fb_draw_text (fb,
			(fb->width-w)/2, 
			15,
			PSPLASH_TEXT_COLOR,
			&radeon_font,
			msg);
}

void
psplash_draw_progress (PSplashFB *fb, int value)
{
  int x, y, width, height, barwidth;

  /* 4 pix border */
  x      = ((fb->width  - BAR_IMG_WIDTH)/2) + 4 ;
  y      = fb->height - (fb->height/6) + 4;
  width  = BAR_IMG_WIDTH - 8; 
  height = BAR_IMG_HEIGHT - 8;

  if (value > 0)
    {
      barwidth = (CLAMP(value,0,100) * width) / 100;
      psplash_fb_draw_rect (fb, x + barwidth, y, 
    			width - barwidth, height,
			PSPLASH_BAR_BACKGROUND_COLOR);
      psplash_fb_draw_rect (fb, x, y, barwidth,
			    height, PSPLASH_BAR_COLOR);
    }
  else
    {
      barwidth = (CLAMP(-value,0,100) * width) / 100;
      psplash_fb_draw_rect (fb, x, y, 
    			width - barwidth, height,
			PSPLASH_BAR_BACKGROUND_COLOR);
      psplash_fb_draw_rect (fb, x + width - barwidth,
			    y, barwidth, height,
			    PSPLASH_BAR_COLOR);
    }

  DBG("value: %i, width: %i, barwidth :%i\n", value, 
		width, barwidth);
}

static int 
parse_command (PSplashFB *fb, char *string, int length) 
{
  char *command;
  int   parsed=0;

  parsed = strlen(string)+1;

  DBG("got cmd %s", string);
	
  if (strcmp(string,"QUIT") == 0)
    return 1;

  command = strtok(string," ");

  if (!strcmp(command,"PROGRESS")) 
    {
      psplash_draw_progress (fb, atoi(strtok(NULL,"\0")));
    } 
  else if (!strcmp(command,"MSG")) 
    {
      psplash_draw_msg (fb, strtok(NULL,"\0"));
    } 
  else if (!strcmp(command,"QUIT")) 
    {
      return 1;
    }

  return 0;
}

void 
psplash_main (PSplashFB *fb, int pipe_fd, bool disable_touch)
{
  int            err;
  ssize_t        length = 0;
  fd_set         descriptors;
  struct timeval tv;
  char          *end;
  char           command[2048];
  int            taptap=0;
  int            laststatus=0;
  int            touch_fd = -1;

  tv.tv_sec = 0;
  tv.tv_usec = 200000;

  FD_ZERO(&descriptors);
  FD_SET(pipe_fd, &descriptors);

  end = command;

  while (1)
    {
    startloop:
      // Handles tap-tap touchscreen sequence
      if(touch_fd < 0 && disable_touch == FALSE)
            touch_fd = Touch_open();

      if(Touch_handler(touch_fd, &taptap, &laststatus))
      {
	TapTap_Progress(fb, taptap);
	if(taptap > TAPTAP_TH)
	{
	  TapTap_Detected(touch_fd, fb, laststatus);
	  return;
	}
      }

      err = select(pipe_fd+1, &descriptors, NULL, NULL, &tv);
      
      if (err <= 0) 
	{
	  if((err==0))
	  { // This is the select(9 timeout case, needed only to handle the tap-tap sequence, so repeat the loop.
	    tv.tv_sec = 0;
	    tv.tv_usec = 200000;
	    
	    FD_ZERO(&descriptors);
	    FD_SET(pipe_fd,&descriptors);
	    goto startloop;
	  }
	  return;
	}
      
      length += read (pipe_fd, end, sizeof(command) - (end - command));

      if (length == 0) 
	{
	  /* Reopen to see if there's anything more for us */
	  close(pipe_fd);
	  pipe_fd = open(PSPLASH_FIFO,O_RDONLY|O_NONBLOCK);
	  goto out;
	}
      
      if (command[length-1] == '\0') 
	{
	  if (parse_command(fb, command, strlen(command))) 
	    return;
	  length = 0;
	} 
      else if (command[length-1] == '\n') 
	{
	  command[length-1] = '\0';
	  if (parse_command(fb, command, strlen(command))) 
	    return;
	  length = 0;
	} 

    out:
      end = &command[length];
    
      tv.tv_sec = 0;
      tv.tv_usec = 200000;
      
      FD_ZERO(&descriptors);
      FD_SET(pipe_fd,&descriptors);
    }

  return;
}

int 
main (int argc, char** argv) 
{
  char      *tmpdir;
  int        pipe_fd, i = 0, angle = 0, ret = 0;
  PSplashFB *fb;
  bool       disable_console_switch = FALSE;
  bool       disable_touch = FALSE;
  
  signal(SIGHUP, psplash_exit);
  signal(SIGINT, psplash_exit);
  signal(SIGQUIT, psplash_exit);

  while (++i < argc)
    {
      if (!strcmp(argv[i],"-n") || !strcmp(argv[i],"--no-console-switch"))
        {
	  disable_console_switch = TRUE;
	  continue;
	}

      if (!strcmp(argv[i],"-a") || !strcmp(argv[i],"--angle"))
        {
	  if (++i >= argc) goto fail;
	  angle = atoi(argv[i]);
	  continue;
	}
	
      if (!strcmp(argv[i],"--notouch"))
      {
	disable_touch = TRUE;
	continue;
      }
 
    fail:
      fprintf(stderr, 
	      "Usage: %s [-n|--no-console-switch][-a|--angle <0|90|180|270>]\n", 
	      argv[0]);
      exit(-1);
    }

  tmpdir = getenv("TMPDIR");

  if (!tmpdir)
    tmpdir = "/tmp";

  chdir(tmpdir);

  if (mkfifo(PSPLASH_FIFO, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))
    {
      if (errno!=EEXIST) 
	{
	  perror("mkfifo");
	  exit(-1);
	}
    }

  pipe_fd = open (PSPLASH_FIFO,O_RDONLY|O_NONBLOCK);
  
  if (pipe_fd==-1) 
    {
      perror("pipe open");
      exit(-2);
    }

  if (!disable_console_switch)
    psplash_console_switch ();

  if ((fb = psplash_fb_new(angle)) == NULL) {
	  ret = -1;
	  goto fb_fail;
  }
  
  /* Set the font size, based on the display resolution and screen orientation */
  if(fb->width < USESMALLFONT_TH)
    FONT_SCALE = 0; // Small fonts (scale = 1x)
  else
    FONT_SCALE = 1; // large fonts (scale = 2x)
    
  /* Clear the background with #ecece1 */
  psplash_fb_draw_rect (fb, 0, 0, fb->width, fb->height,
                        PSPLASH_BACKGROUND_COLOR);
			
  if(-1 == psplash_draw_custom_splashimage(fb))
  {

  /* Draw the Poky logo  */
  psplash_fb_draw_image (fb, 
			 (fb->width  - POKY_IMG_WIDTH)/2, 
			 ((fb->height * 5) / 6 - POKY_IMG_HEIGHT)/2,
			 POKY_IMG_WIDTH,
			 POKY_IMG_HEIGHT,
			 POKY_IMG_BYTES_PER_PIXEL,
			 POKY_IMG_RLE_PIXEL_DATA);
  }

  psplash_draw_progress (fb, 0);

  UpdateBrightness();

  psplash_main (fb, pipe_fd, disable_touch);

  psplash_fb_destroy (fb);

  // Clear the bootcounter
  setbootcounter(0);

 fb_fail:
  unlink(PSPLASH_FIFO);

  if (!disable_console_switch)
    psplash_console_reset ();

  return ret;
}
