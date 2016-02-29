#include <stdio.h>
#include <ncurses.h>
#include <sys/select.h>


void your_callback() {
	static int n=0;

	printw("%s n=%d\n", __FUNCTION__,n);

	n++;
}

int main() {
	int ch;
	struct timeval t;

	initscr();
	cbreak();
	noecho();
	nodelay(stdscr, TRUE);
	scrollok(stdscr, TRUE);

	for ( ; ; ) {
		/* 100 millisecond timer */
		t.tv_sec = 0;
		t.tv_usec = 100000; 
		select(0, NULL, NULL, NULL, &t); 
		your_callback();

		ch=getch();
		if ( ch != ERR ) {
			printw("You said: %c\n",ch);
			refresh();
		}
		usleep(1000);
	}

	endwin();
	
	return 0;
}
