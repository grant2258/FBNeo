#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
//

FILE * fp;


char name[8][6] = {
                        {'A', 0     },
                        {'B', 0     },
 			{'X', 0     },
			{'Y', 0     },
			{'L', 'B', 0},
 			{'R', 'B', 0},
			{'S', 't', 'a', 'r', 't' ,0 },
                        {'S', 'e', 'l' ,0 },
};
int joystickmap[4][8] = { -1 } ;

static SDL_Joystick* JoyList[4];


int state (int buttonCount, SDL_Joystick *joystick)
{
	for (int button = 0;  button < buttonCount; button++) 
	{
          int    state = SDL_JoystickGetButton(joystick, button);
                          if (state) return button;
	}
	return -1;
}

void main(void)
{
	int buttonCount;
	int joystick_count;
	if (SDL_Init( SDL_INIT_VIDEO | SDL_INIT_JOYSTICK ) < 0)
	{
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
	        exit(1);
	}
	joystick_count = SDL_NumJoysticks();
	printf("%d joysticks were found.\n\n", joystick_count );
	printf("The names of the joysticks are:\n");

   //     printf("Controller name:%s          \n",
        printf("\r[LB]                                  [RB]\n");
        printf("\r                                         \n");
        printf("\r                                  [Y]    \n");
        printf("\r                                         \n");
        printf("\r           [start] [selct]   [X]   [B]   \n");
        printf("\r                                         \n");
        printf("\r                               [A]       \n");
        printf("\r                                         \n");

	for( int i=0; i < joystick_count; i++ ) 
	{
		printf("  %d  %s\n",i, SDL_JoystickName(i));
		JoyList[i] = SDL_JoystickOpen(i);
	


		SDL_Joystick *joystick = JoyList[i];
		SDL_JoystickEventState(SDL_IGNORE);
		buttonCount = SDL_JoystickNumButtons(joystick);
		int counter =0;
		int ask_for_button=1;
		
//		FILE * fp;
		fp = fopen(SDL_JoystickName(i),"w");
		while (counter < 8 ) // use a while loop for a change :)

		{
			SDL_PumpEvents();
        		SDL_JoystickUpdate();
			
			int result;
		
			if ( ask_for_button )
			{
		
			printf("please press a button for %s \n",name[counter]); 
				 ask_for_button = 0;
			}
			result = state(buttonCount,joystick);
			
			if (result != -1)  
			{	printf("button %d selected\n",result);
				fprintf(fp, "BUTTON%d = %d\n",counter, result);  
				counter ++;
				ask_for_button =1;
				SDL_Delay(300);
			}
		}
		fclose(fp);	
	}
}

