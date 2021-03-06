#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "curterm.h"

static int ctrl_c_event;

#ifdef __MINGW32__
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <conio.h>

static HANDLE hStdin;
static HANDLE hStdout;

void gotoxy(int x, int y)
{
	COORD xy;

	xy.X = x;
	xy.Y = y;
	SetConsoleCursorPosition(hStdout, xy);
}

void clrscr(void)
{
	CONSOLE_SCREEN_BUFFER_INFO scr;
	COORD ul = {0, 0};
	DWORD n, size;

	GetConsoleScreenBufferInfo(hStdout, &scr);
	size = scr.dwSize.X * scr.dwSize.Y;
	FillConsoleOutputCharacter(hStdout, ' ', size, ul, &n);
	FillConsoleOutputAttribute(hStdout,   0, size, ul, &n);
	SetConsoleCursorPosition(hStdout, ul);
}

BOOL CtrlC(DWORD ct)
{
	if (CTRL_C_EVENT == ct) {
		ctrl_c_event = 1;
		return TRUE;
	}
	return FALSE;
}

void initterm()
{
	static int _init = 0;

	if (!_init) {
		hStdin  = GetStdHandle(STD_INPUT_HANDLE);
		hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		_init = 1;
	}
}

void cbreak(int enable)
{
	initterm();

	ctrl_c_event = 0;
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlC, enable ? FALSE : TRUE)) 
	{
		printf("failed to install Ctrl-C handler\n");
		exit(1);
	}

}

void prepterm(int dir)
{
	static DWORD told, tnew;

	initterm();

	if (dir) {
		GetConsoleMode(hStdin, &told);
		tnew  = told;
		tnew &= ~ENABLE_PROCESSED_INPUT;
		SetConsoleMode(hStdin, tnew);
	}
	else
		SetConsoleMode(hStdin, told);

	return;
}

int has_key()
{
	return kbhit();
}

int getkey(void)
{
	int ch;

	ch = getch();
	if (0 == ch)
		ch = 0xF0;
	if (0xF0 == ch || 0xE0 == ch)
		ch = (ch << 8) + getch();

	return ch;
}

#else

#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

void prepterm(int dir)
{
	static struct termios told, tnew;

	if (dir) {
		tcgetattr(STDIN_FILENO, &told);
		tnew = told;
		tnew.c_lflag &= ~(ICANON | ECHO );
		tcsetattr(STDIN_FILENO, TCSANOW | TCSAFLUSH, &tnew);
	}
	else
		tcsetattr(STDIN_FILENO, TCSANOW | TCSAFLUSH, &told);
}

int has_key(void)
{
	struct timeval tv;
	fd_set rd;

	tv.tv_sec  = 0;
	tv.tv_usec = 0;

	FD_ZERO(&rd);
	FD_SET(STDIN_FILENO, &rd);

	select(STDIN_FILENO + 1, &rd, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &rd);
}

#define ESC '\033'

int getkey(void)
{
	int ch;

	ch = getchar();
	/* NOTE: cannot read ESC key */
	if (ESC == ch) {
		ch = (u_char)getchar();
		ch = (ch << 8) + (u_char)getchar();
	}

	return ch;
}

void gotoxy(int x, int y)
{
	char buf[16];

	sprintf(buf, "%c[%d;%dH", ESC, y, x);
	write(1, buf, strlen(buf));
}

void clrscr(void)
{
	char buf[16];

	sprintf(buf, "%c[2J", ESC);
	write(1, buf, strlen(buf));
	gotoxy(0, 0);
}

void ctrlc_handler(int sig)
{
	ctrl_c_event = 1;
}

void cbreak(int enable)
{
	if (enable)
		signal(SIGINT, SIG_DFL);
	else
		if (SIG_ERR == signal(SIGINT, ctrlc_handler)) {
			printf("failed to install Ctrl-C handler\n");
			exit(1);
		}
}

#endif

#ifdef MAIN
int main(int argc, char *argv[])
{
	int c;

	c = 0;
	prepterm(1);
	while (c != 'q') {
		if (has_key()) {
			printf("%04X ", (c = getkey()));
			if (27 == c) {
				printf("%d ", (c = getkey()));
				printf("%d ", (c = getkey()));
			}
			printf("\n");
		}
	}
	prepterm(0);
	return 0;
}
#endif

void kbflush(void)
{
	while (has_key())
		getkey();
}

int has_ctrlc(void)
{
	if (ctrl_c_event) {
		ctrl_c_event = 0;
		return 1;
	}
	return 0;
}
