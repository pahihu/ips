
/*  IPS - virtual machine for the IPS (Interpreter for Process Structures)
 *  language / computing environment used onboard of AMSAT spacecraft
 *
 *  Copyright (C) 2003, Pieter-Tjerk de Boer -- pa3fwm@amsat.org
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  This module contains the unix- or ncurses-specific parts of the
 *  program; i.e., the processing of command-line options, the user
 *  interface and the timing stuff.
 *  It also contains the few lines of code that load the actual IPS
 *  binary image into the emulator's memory mem[] .
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <time.h>
#include <unistd.h>

#include <conio2.h>
#define WIN32_LEAN_AND_MEAN	1
#include <windows.h>

#include "ips.h"


/**************** timing-related code **********************/

void do_sleep(void)   /* sleep for a little while, i.e., give up CPU time to other processes */
{
	_sleep(10);
}


int test_20ms(void)    /* test whether another 20 milliseconds have passed by */
{
	struct _timeb tb;
   static long thr_msec=0;
	_ftime(&tb);
   if (tb.millitm<thr_msec || tb.millitm>=thr_msec+20) {
      thr_msec+=20;
      if (thr_msec==1000) thr_msec=0;
      return 1;
   }
   return 0;
}


void init_uhr(void)   /* initialize the UHR with the present time */
{
   time_t t;
   struct tm *tm;

   time(&t);
   tm=gmtime(&t);
   pokeB(UHR,0);
   pokeB(UHR+1,tm->tm_sec);
   pokeB(UHR+2,tm->tm_min);
   pokeB(UHR+3,tm->tm_hour);

   tm->tm_mon+=2;
   if (tm->tm_mon<4) { tm->tm_year--; tm->tm_mon+=12; }
   poke(UHR+4, (tm->tm_mon*306)/10 + tm->tm_year*365 + tm->tm_year/4 + tm->tm_mday - 28553 );
}



/**************** code for handling screen and keyboard **********************/

unsigned short norm_attr;
unsigned short rev_attr;
static HANDLE hStdin;
static DWORD old_mode;

void do_redraw(void)   /* refresh the screen */
{
   int i,j;
	struct char_info scr[16*64];
	unsigned short attr;

	_conio_gettext(2,2,64+1,16+1,scr);
   for (j=0;j<16;j++) {
      for (i=0;i<64;i++) {
         int c=mem[j*64+i];
         int inv=(c&0x80);
         if (inv) attr = rev_attr;
         else attr = norm_attr;
         c&=0x7f;
         if (c<0x20) c|=0x80;
			scr[j*64+i].letter = c;
			scr[j*64+i].attr   = attr;
      }
   }
	puttext(2,2,64+1,16+1,scr);
}

void init_console(void)
{
	int i;
	DWORD new_mode;
	struct text_info ti;

	hStdin  = GetStdHandle(STD_INPUT_HANDLE);

	GetConsoleMode(hStdin, &old_mode);
	new_mode = old_mode & ~ENABLE_PROCESSED_INPUT;
	SetConsoleMode(hStdin, new_mode);

	inittextinfo();
	gettextinfo(&ti);
	norm_attr = LIGHTGRAY + (     BLUE << 4);
	rev_attr  =      BLUE + (LIGHTGRAY << 4);

	textcolor(LIGHTGRAY);
	textbackground(BLUE);
	clrscr();
	cputsxy(1,1,
"+----------------------------------------------------------------+");
	for (i = 0; i < 16; i++)
		cputsxy(1,2+i,
"|                                                                |");
	cputsxy(1,18,
"+----------------------------------------------------------------+");
	cputsxy(18,1,"[Meinzer M-9097 IPS Computer]");
}

void end_console(void)
{
	normvideo();
	clrscr();
	SetConsoleMode(hStdin,old_mode);
	exit(0);
}

#define KEY_LEFT				0xE04B
#define KEY_RIGHT				0xE04D
#define KEY_UP					0xE048
#define KEY_DOWN				0xE050
#define KEY_BACKSPACE		0x0008
#define KEY_DC					0xE053
#define KEY_IC					0xE052	
#define CTRL_C					0x0003
#define ERR						0

int get_ch(void)
{
	int c = 0;

	if (kbhit()) {
		c = getch();
		if (0 == c || 0xE0 == c) {
			if (!c) c = 0xF0;
			c = (c << 8) + getch();
		}
	}
	return c;
}

int handle_keyboard(void)
{
   int c=0;
   static int insertmode=1;
   int ret=1;

   mem[input_ptr]&=0x7f;

	c = get_ch();

	if (CTRL_C == c)
		end_console();

   switch (c) {
      case ERR: 
         ret=0;
         break;
      case '\n':
      case '\r':
         return 2;
      case KEY_LEFT:
         input_ptr--;
         if (input_ptr<0) input_ptr=0;
         break;
      case KEY_UP:
         input_ptr-=64;
         if (input_ptr<0) input_ptr=0;
         break;
      case KEY_RIGHT:
         input_ptr++;
         if (input_ptr>TVE) input_ptr=TVE;
         break;
      case KEY_DOWN:
         input_ptr+=64;
         if (input_ptr>TVE) input_ptr=TVE;
         break;
      case KEY_BACKSPACE:
         input_ptr--;
         if (input_ptr<0) input_ptr=0;
      case KEY_DC:
         if (insertmode) {
            memmove(mem+input_ptr,mem+input_ptr+1,TVE-input_ptr);
            mem[TVE]=' ';
         } else {
            mem[input_ptr]=' ';
         }
         break;
      case KEY_IC:
         insertmode=!insertmode;
         break;
      default:
         if (insertmode) memmove(mem+input_ptr+1,mem+input_ptr,TVE-input_ptr);
         pokeB(input_ptr++,c); 
         if (input_ptr>TVE) input_ptr=TVE;
   }

   if (!peekB(READYFLAG)) mem[input_ptr]|=0x80;

   return ret;
}

/**************** startup code **********************/

int main(int argc,char **argv)
{
   FILE *f;
   char *cmd=NULL;
   char *image="IPS-Mu.bin";

   /* parse command line */
   while (1) {
      int i;
      i=getopt(argc,argv,"c:f:i:xh");
      if (i<0) break;
      switch (i) {
         case 'c':
            if (!optarg) goto error;
            cmd=optarg;
            break;
         case 'f':
            if (!optarg) goto error;
            cmd=malloc(strlen(optarg)+16);
            if (!cmd) goto error;
            sprintf(cmd,"\" %s \" READ",optarg);
            break;
         case 'i':
            if (!optarg) goto error;
            image=optarg;
            break;
         case 'x':
            image="IPS-Xu.bin";
            break;
         default:
error:
            printf("\n"
"IPS for unix/linux\n"
"------------------\n"
"Options are:\n"
"-c <commands>   - initial IPS commands\n"
"-f <filename>   - load IPS source file, equivalent to -c '\" <filename> \" READ'\n"
"-i <filename>   - load image from file, default is IPS-Mu.bin\n"
"-x              - be cross-compiler, equivalent to -i IPS-Xu.bin\n"
"");
            return 1;
      }
   }

   /* load the image */
   f=fopen(image,"r");
   if (!f) {
      fprintf(stderr,"Can't open image file \"%s\".\n",image);
      return 1;
   }
   fread(mem,1,65536,f);
   fclose(f);

   /* if commands were given on the command line, copy them into the screen memory as input */
   if (cmd) {
      if (strlen(cmd)>64*8) {
         fprintf(stderr,"Command too long (max. 512 bytes)\n");
         return 1;
      }
      memcpy(mem+64*8,cmd,strlen(cmd));
      poke(a_PE,0x200+strlen(cmd));
      mem[READYFLAG]=1;
   }

   /* initialize the ncurses stuff */
   init_console();

   /* initialize the UHR */
   init_uhr();

   /* finally, run the emulator */
   emulator();

   return 0;
}

