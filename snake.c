#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h> // notice this! you need it!
#include <errno.h>

#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>

int kbhit (void)
{
  struct timeval tv;
  fd_set rdfs;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&rdfs);
	FD_SET(STDIN_FILENO, &rdfs);

	select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &rdfs);
}

int msleep(long tms)
{
    struct timespec ts;
    int ret;

    if (tms < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;

    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

    return ret;
}

void changemode(int dir)
{
  static struct termios oldt, newt;

	if ( dir == 1 )
  {
    tcgetattr( STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);
  }
  else
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
}

#define SPIEL_GESCHWINDIGKEIT 200
#define BREITE 30
#define HOEHE 40
#define ITEM_WAND '#'
#define ITEM_LEER ' '
#define ITEM_FUTTER 'x'
#define ITEM_SCHLANGE_KOPF '@'

#define STARTFLAECHE_SEITENLAENGE 5
#define ANZAHL_START_FUTTERSTUECKE 10
#define ANZAHL_WAENDE 8
#define WAENDE_LAENGE 4

#define clear() printf("\033[H\033[J")
#define gotoxy(x,y) printf("\033[%d;%dH", (y), (x))
#define cursor_hide() printf("\e[?25l")
#define cursor_show() printf("\e[?25h")

#define MIN(a,b) ((a < b) ? (a):(b))
#define MAX(a,b) ((a> b) ? (a):(b))

struct koerper_glied {
	int x;
	int y;
};

char spielfeld[HOEHE][BREITE];

// TODO: uncomment line
struct koerper {
	// struct koerper_glied liste[ANZAHL_START_FUTTERSTUECKE + 1];
	struct koerper_glied liste[100];
	int laenge;
	int letzter_index;
};

struct koerper schlangen_koerper = {
	.laenge = ANZAHL_START_FUTTERSTUECKE + 1,
	.letzter_index = 0
};

// Startposition und -richtung der Schlange
int sx = BREITE / 2;
int sy = HOEHE / 2;

int dx = 1;
int dy = 0;

// Wie oft wurde das Spielfeld schon neu gezeichnet
unsigned long long update = 0;

int zufalls_zahl(int min, int max)
{
	return (rand() % (max - min + 1)) + min;
}

void spielfeld_aufbauen_begrenzungsrahmen()
{
	// Rahmen
	for (int i = 0; i < BREITE * HOEHE; i++)
	{
		int row = i / BREITE;
		int col = i % BREITE;
		if (row == 0 || row == HOEHE - 1)
			spielfeld[row][col] = ITEM_WAND;
		else if (col == 0 || col == BREITE - 1)
			spielfeld[row][col] = ITEM_WAND;
		else
			spielfeld[row][col] = ITEM_LEER;
	}
}

void spielfeld_aufbauen_hindernisse_vertikal(int linien_anz, int linien_lng)
{
	// Hindernisse
	for (int linie_num = 0; linie_num < linien_anz; linie_num++)
	{
		int zufallsY = zufalls_zahl(1, (HOEHE - linien_lng - 2));
		int zufallsX = zufalls_zahl(1, (BREITE - 2));
		for (int y = 0; y < linien_lng; y++)
		{
			spielfeld[y + zufallsY][zufallsX] = ITEM_WAND;
		}
	}
}

void spielfeld_aufbauen_hindernisse_horizontal(int linien_anz, int linien_lng)
{
	// Hindernisse
	for (int linie_num = 0; linie_num < linien_anz; linie_num++)
	{
		int zufallsY = zufalls_zahl(1, (HOEHE - 2));
		int zufallsX = zufalls_zahl(1, (BREITE - 2 - linien_anz));
		for (int x = 0; x < linien_lng; x++)
		{
			spielfeld[zufallsY][x + zufallsX] = ITEM_WAND;
		}
	}
}

void spielfeld_aufbauen_futter(int anz)
{
	for (int i = 0; i < anz; i++)
	{
		int y = zufalls_zahl(1, HOEHE - 2);
		int x = zufalls_zahl(1, BREITE - 2);
		spielfeld[y][x] = ITEM_FUTTER;
	}
}

void spielfeld_aufbauen_startflaeche(int seitenlaenge)
{
	int mitteX = BREITE / 2;
	int mitteY = HOEHE / 2;
	int s_halbe = seitenlaenge / 2;
	for(int dy = -s_halbe; dy < s_halbe + 1; dy++) {
		for(int dx = -s_halbe; dx < s_halbe + 1; dx++) {
			spielfeld[mitteY + dy][mitteX + dx] = ITEM_LEER;
		}
	}
}

void spielfeld_aufbauen()
{
	spielfeld_aufbauen_begrenzungsrahmen();
	spielfeld_aufbauen_hindernisse_vertikal(ANZAHL_WAENDE, WAENDE_LAENGE);
	spielfeld_aufbauen_hindernisse_horizontal(ANZAHL_WAENDE, WAENDE_LAENGE);
	spielfeld_aufbauen_startflaeche(STARTFLAECHE_SEITENLAENGE);
	spielfeld_aufbauen_futter(ANZAHL_START_FUTTERSTUECKE);
}

void spielfeld_zeichnen()
{
	gotoxy(0,0);
	char spielfeld_str[HOEHE * (BREITE + 1)];
	for (int i = 0; i < HOEHE * (BREITE + 1); i++)
	{
		int row = i / ( BREITE  + 1);
		int col = i % (BREITE + 1);
		spielfeld_str[i] = spielfeld[row][col];
		if (col == BREITE)
			spielfeld_str[i] = '\n';
	}

	// schlangen kopf
	spielfeld_str[(sy * (BREITE + 1) + sx)] = ITEM_SCHLANGE_KOPF;

	// koerper
	for(int i = 0; i < schlangen_koerper.letzter_index; i++) {
		struct koerper_glied glied = schlangen_koerper.liste[i];
		spielfeld_str[(glied.y * (BREITE + 1)) + glied.x] = 'o';
	}

	update ++;
	printf("%s", spielfeld_str);
	printf("Update: %lld", update);
}

void aendere_schlangen_laufrichtung(char taste) {
	switch (taste)
	{
	case 'a':
		dx = -1;
		dy = 0;
		break;
	case 'd':
		dx = 1;
		dy = 0;
		break;
	case 'w':
		dx = 0;
		dy = -1;
		break;
	case 's':
		dx = 0;
		dy = 1;
		break;
	}
}

void bewege_schlange() {

	sx += dx;
	sy += dy;
}

int kollision_koerper() {
	for(int i = 0; i < schlangen_koerper.letzter_index; i++) {
		struct koerper_glied glied = schlangen_koerper.liste[i];
		if(glied.x == sx && glied.y == sy) return 1;
	}
	return 0;
}

void wachse(struct koerper_glied glied) {
	for (int i = schlangen_koerper.letzter_index; i >= 1; i--)
	{
		schlangen_koerper.liste[i] = schlangen_koerper.liste[i-1];
	}
	schlangen_koerper.liste[0] = glied;
	schlangen_koerper.letzter_index++;
}

void loesche_letztes_glied() {
	schlangen_koerper.letzter_index--;
}

int main()
{
	srand(time(NULL));

	int game_over = 0;
	spielfeld_aufbauen();

	clear();
	cursor_hide();
	// spielfeld[sy][sx] = ITEM_SCHLANGE_KOPF;
	changemode(1);

	char taste;
	do
	{
		// Schlange bewegen
		struct koerper_glied glied = {
			.x = sx,
			.y = sy
		};

		bewege_schlange();
		wachse(glied);
		if(kollision_koerper()) game_over = 1;

		if (spielfeld[sy][sx] == ITEM_WAND) game_over = 1;
		if (spielfeld[sy][sx] != ITEM_FUTTER) loesche_letztes_glied();
		if (spielfeld[sy][sx] == ITEM_FUTTER) spielfeld[sy][sx] = ITEM_LEER;

		if(kbhit()) {
			taste = getchar();
			aendere_schlangen_laufrichtung(taste);
		}

		if(taste == 'q') game_over = 1;
		if(taste == 27) game_over = 1;
		spielfeld_zeichnen();
		msleep(SPIEL_GESCHWINDIGKEIT);
	} while (game_over == 0);

	if(game_over == 1) {
		gotoxy(0,0);
		printf("GAME OVER");
	}

	cursor_show();
	changemode(0);
	return 0;
}