#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
/*todo
  add a swich to only set controllers that arent already configured.
  at the moment it will just loop through all your joysticks you have plugged in.

  add error checking and abort option

*/

#define yindent 18
FILE * fp;

void PrintSDL(SDL_Surface* font,SDL_Surface* dest,int x,int y,char* text,...)
{
	char buf[500];
	int i,len;
	SDL_Rect Rsrc,Rdst;
	va_list args;
	va_start(args,text);
	vsprintf(buf,text,args);
	va_end(args);
	Rsrc.w = font->w/16;
	Rsrc.h = font->h/14;
	len = (int)strlen(buf);
	for(i=0;i<len;i++)
	{
		Rsrc.x = Rsrc.w*((unsigned char)(buf[i])%16);
		Rsrc.y = Rsrc.h*((unsigned char)(buf[i])/16-2);  // on saute les 31 premiers codes ASCII
		Rdst.x = x;
		Rdst.y = y;
		SDL_BlitSurface(font,&Rsrc,dest,&Rdst);
		x+=Rsrc.w;
	}
}

char name[8][6] = {
	{'A', 0},
	{'B', 0},
	{'X', 0},
	{'Y', 0},
	{'L', 'B', 0},
	{'R', 'B', 0},
	{'S', 't', 'a', 'r', 't', 0},
	{'S', 'e', 'l', 0},
};

int joystickmap[4][8] = { -1 };

static SDL_Joystick* JoyList[4];


int state (int buttonCount, SDL_Joystick *joystick)
{
	for (int button = 0;  button < buttonCount; button++)
	{
		int state = SDL_JoystickGetButton(joystick, button);
		if (state) return button;
	}
	return -1;
}

int main(int argc, char *argv[])
{
	int buttonCount;
	int joystick_count;
	 SDL_Surface *screen,*font;
	if (SDL_Init( SDL_INIT_VIDEO | SDL_INIT_JOYSTICK ) < 0)
	{
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	screen=SDL_SetVideoMode(800,600,32,SDL_SWSURFACE|SDL_DOUBLEBUF);  
	font=SDL_LoadBMP("font.bmp");
	if (!font) return -1;
	joystick_count = SDL_NumJoysticks();
	PrintSDL(font,screen,0,0 *yindent, "%d Joystick(s) Detected.", joystick_count );


	for( int i=0; i < joystick_count; i++ )
	{
		JoyList[i] = SDL_JoystickOpen(i);

		PrintSDL(font,screen,0,2 *yindent, "************************************************\n");
		PrintSDL(font,screen,0,3 *yindent,  "*[LB]                                     [RB] *\n");
		PrintSDL(font,screen,0,4 *yindent,  "*                                              *\n");
		PrintSDL(font,screen,0,5 *yindent,  "*     [^]                              [Y]     *\n");
		PrintSDL(font,screen,0,6 *yindent,  "*                                              *\n");
		PrintSDL(font,screen,0,7 *yindent,  "* [<]     [>]   [start] [select]  [X]   [B]    *\n");
		PrintSDL(font,screen,0,8 *yindent,  "*                                              *\n");
		PrintSDL(font,screen,0,9 *yindent,  "*     [v]                          [A]         *\n");
		PrintSDL(font,screen,0,10 *yindent, "*                                              *\n");
		PrintSDL(font,screen,0,11 *yindent, "************************************************\n");

		PrintSDL(font,screen,0,13 *yindent, "Setting up Joystick %d\n",i);
		PrintSDL(font,screen,0,14 *yindent, "                                                     ");
		PrintSDL(font,screen,0,14 *yindent, "%s\n",SDL_JoystickName(i));

		SDL_Joystick *joystick = JoyList[i];
		SDL_JoystickEventState(SDL_IGNORE);
		buttonCount = SDL_JoystickNumButtons(joystick);
		int counter =0;
		int ask_for_button=1;

		fp = fopen(SDL_JoystickName(i),"w");

		while (counter < 8 ) // use a while loop for a change :)
		{
			SDL_PumpEvents();
			SDL_JoystickUpdate();

			int result;

			if ( ask_for_button )
			{
				PrintSDL(font,screen,0,15 *yindent, "                                                     ");
				PrintSDL(font,screen,0,15 *yindent, "please press a button for %s \n",name[counter]);
				SDL_Flip(screen);
				ask_for_button = 0;
			}
			result = state(buttonCount,joystick);

			if (result != -1)
			{
//				PrintSDL(font,screen,0,16 *yindent, "                                                     ");
//				PrintSDL(font,screen,0,16 *yindent, "button %d selected for %s\n",result, name[counter]);
				fprintf(fp, "%s %d\n",name[counter], result);
				counter ++;
				ask_for_button =1;
				SDL_Flip(screen);
				SDL_Delay(300);
			}
		}
		fclose(fp);
	}
	SDL_FreeSurface(font);
	SDL_Quit();
};

