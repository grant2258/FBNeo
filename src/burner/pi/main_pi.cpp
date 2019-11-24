
#include "burner.h"
#include <sys/stat.h>
#include <unistd.h>

extern int nExitEmulator;
//extern int nEnableFreeplayHack;
static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog);
static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi);
INT32 display_set_controls();
INT32 Mapcoins(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice);
#define KEY(x) { pgi->nInput = GIT_SWITCH; pgi->Input.Switch.nCode = (UINT16)(x); }
int nAppVirtualFps = 6000;			// App fps * 100
bool bRunPause = 0;
bool bAlwaysProcessKeyboardInput=0;
int usejoy=0;
int usebarcade=0;

/*  use snes layout for xbox360  controller 
a b r = kick weak, medium strong
x y l = punch weak, medium strong */
     
static unsigned int capcom6Layout[] = {
	2, 3, 4, 0, 1, 5,
};


/* keep the snes layout on xbox360 controller
  Phscial button layout on the encoder should be
   X Y L 
   2 3 4
   0 1 5
   A B R
  no need to change this for sf2 as use the capcom6Layout when for it when its needed its the same as that with the rows reversed
*/

static unsigned int barcade_Layout[] = {

        0, 1, 5, 2, 3, 4,

};


static bool usesStreetFighterLayout()
{
	int fireButtons = 0;
	bool Volume = false;
	struct BurnInputInfo bii;

	for (UINT32 i = 0; i < 0x1000; i++) {
		bii.szName = NULL;
		if (BurnDrvGetInputInfo(&bii,i)) {
			break;
		}
		
		BurnDrvGetInputInfo(&bii, i);
		if (bii.szName == NULL) {
			bii.szName = "";
		}

		bool bPlayerInInfo = (toupper(bii.szInfo[0]) == 'P' && bii.szInfo[1] >= '1' && bii.szInfo[1] <= '4'); // Because some of the older drivers don't use the standard input naming.
		bool bPlayerInName = (bii.szName[0] == 'P' && bii.szName[1] >= '1' && bii.szName[1] <= '4');

		if (bPlayerInInfo || bPlayerInName) {
			INT32 nPlayer = 0;

			if (bPlayerInName) {
				nPlayer = bii.szName[1] - '1';
			}
			if (bPlayerInInfo && nPlayer == 0) {
				nPlayer = bii.szInfo[1] - '1';
			}
			if (nPlayer == 0) {
				if (strncmp(" fire", bii.szInfo + 2, 5) == 0) {
					fireButtons++;
				}
			}
			if ((strncmp("Volume", bii.szName, 6) == 0) && (strncmp(" fire", bii.szInfo + 2, 5) == 0)) {
				Volume = true;
			}
		}
	}

	if (nFireButtons >= 5 && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPS2 && !Volume) {
		bStreetFighterLayout = true;
 	printf("Using SF2 Layout\n");
	}
return  bStreetFighterLayout;
}

void formatBinary(int number, int sizeBytes, char *dest, int len)
{
	int size = sizeBytes * 8;
	if (size + 1 > len) {
		*dest = '\0';
		return;
	}

	char *ch = dest + size;
	*ch = '\0';
	ch--;

	for (int i = 0; i < size; i++, ch--) {
		*ch = (number & 1) + '0';
		number >>= 1;
	}
}

void dumpDipSwitches()
{
	BurnDIPInfo bdi;
	char flags[9], mask[9], setting[9];

	for (int i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++) {
		formatBinary(bdi.nFlags, sizeof(bdi.nFlags), flags, sizeof(flags));
		formatBinary(bdi.nMask, sizeof(bdi.nMask), mask, sizeof(mask));
		formatBinary(bdi.nSetting, sizeof(bdi.nSetting), setting, sizeof(setting));

		printf("%d: % 3d - 0x%02x (%s) - 0x%02x (%s) - 0x%02x (%s) - %s\n",
			i,
			bdi.nInput,
			bdi.nFlags, flags,
			bdi.nMask, mask,
			bdi.nSetting, setting,
			bdi.szText);
	}
}

int enableFreeplay()
{
	int dipOffset = 0;
	int switchFound = 0;
	BurnDIPInfo dipSwitch;

	for (int i = 0; !switchFound && BurnDrvGetDIPInfo(&dipSwitch, i) == 0; i++) {
		if (dipSwitch.nFlags == 0xF0) {
			dipOffset = dipSwitch.nInput;
		}
		if (dipSwitch.szText && (strcasecmp(dipSwitch.szText, "freeplay") == 0
			|| strcasecmp(dipSwitch.szText, "free play") == 0)) {
			// Found freeplay. Is it a group or the actual switch?
			if (dipSwitch.nFlags & 0x40) {
				// It's a group. Find the switch
				for (int j = i + 1; !switchFound && BurnDrvGetDIPInfo(&dipSwitch, j) == 0 && !(dipSwitch.nFlags & 0x40); j++) {
					if (dipSwitch.szText && strcasecmp(dipSwitch.szText, "on") == 0) {
						// Found the switch
						switchFound = 1;
					}
				}
			} else {
				// It's a switch
				switchFound = 1;
			}
		}
	}

	if (switchFound) {
		struct GameInp *pgi = GameInp + dipSwitch.nInput + dipOffset;
		pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~dipSwitch.nMask) | (dipSwitch.nSetting & dipSwitch.nMask);
	}

	return switchFound;
}

int setDipSwitch(int switchNo)
{
	int dipOffset = 0;
	int groupSet = 0;
	BurnDIPInfo dipSwitch;
	BurnDIPInfo group;

	for (int i = 0; BurnDrvGetDIPInfo(&dipSwitch, i) == 0; i++) {
		if (dipSwitch.nFlags == 0xF0) {
			dipOffset = dipSwitch.nInput;
		}
		if (dipSwitch.nFlags & 0x40) {
			groupSet = 1;
			BurnDrvGetDIPInfo(&group, i);
		}
		if (i != switchNo) {
			continue;
		}
		if (dipSwitch.nFlags & 0x40) {
			// A group
			fprintf(stderr, "Can't set dipswitch - group, not switch\n");
			break;
		}

		struct GameInp *pgi = GameInp + dipSwitch.nInput + dipOffset;
		pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~dipSwitch.nMask) | (dipSwitch.nSetting & dipSwitch.nMask);

		if (groupSet) {
			printf("Set '%s': %s\n", group.szText, dipSwitch.szText);
		} else {
			printf("Set %s\n", dipSwitch.szText);
		}

		return 1;
	}

	return 0;
}

int parseSwitches(int argc, char *argv[])
{
	int dipSwitchSet = 0;
        for (int i = 1; i < argc; i++) {
                if (*argv[i] != '-') {
			continue;
		}

		if (strcmp(argv[i] + 1, "f") == 0) {
 			if (enableFreeplay()) {
				printf("Freeplay enabled\n");
			} else {
				fprintf(stderr, "Don't know how to enable freeplay - try the hack\n");
			}
//		} else if (strcmp(argv[i] + 1, "F") == 0) {
//			nEnableFreeplayHack = 1;
//			printf("Freeplay hack enabled\n");

		} else if (strcmp(argv[i] + 1, "dumpswitches") == 0) {
			dumpDipSwitches();
		} else if (strncmp(argv[i] + 1, "ds=", 3) == 0) {
			char format[16];
			strncpy(format, argv[i] + 4, sizeof(format) - 1);
			int switchNo = atoi(format);
			if (switchNo != 0) {
				dipSwitchSet |= setDipSwitch(switchNo);
			}
		} else if (strcmp(argv[i] + 1, "joy") == 0) {
			usejoy=1;
		} else if (strcmp(argv[i] + 1, "barcade") == 0) {
			printf("barcade switch set\n");
                        usebarcade=1;
                }

	}

	return dipSwitchSet;
}

int main(int argc, char *argv[])
{
	const char *romname = NULL;
//	nEnableFreeplayHack = 0;

	for (int i = 1; i < argc; i++) {
		if (*argv[i] != '-') {
			romname = argv[i];
		}
	}

	if (romname == NULL) {
		printf("Usage: %s [-f] [-F] [-dumpswitches] [-joy]  <romname>\n", argv[0]);
		printf("e.g.: %s mslug\n", argv[0]);

		return 0;
	}

	ConfigAppLoad();
	SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO);
	BurnLibInit();

	int driverId = -1;
	for (int i = 0; i < (int) nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if (strcmp(BurnDrvGetTextA(0), romname) == 0) {
			driverId = i;
			break;
		}
	}

	if (driverId < 0) {
		fprintf(stderr, "%s is not supported by FB Alpha\n", romname);
		return 1;
	}

	printf("Starting %s\n", romname);


	printf("%i joysticks were found.\n\n", SDL_NumJoysticks() );
	printf("The names of the joysticks are:\n");

	if (SDL_Init( SDL_INIT_JOYSTICK ) < 0)
	    {
	        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        
	    }

	for( int i=0; i < SDL_NumJoysticks(); i++ ) 
	{
	        printf("    %s\n", SDL_JoystickName(i));
    	}

	// Create the nvram directory, if it doesn't exist
	const char *nvramPath = "./nvram";
	if (access(nvramPath, F_OK) == -1) {
		fprintf(stderr, "Creating NVRAM directory at \"%s\"\n", nvramPath);
		mkdir(nvramPath, 0777);
	}

	bCheatsAllowed = false;

	if (DrvInit(driverId, 0) != 0) {
		fprintf(stderr, "Driver init failed\n");
		return 0;
	}

	bStreetFighterLayout = usesStreetFighterLayout();
	parseSwitches(argc, argv);
	if (!usejoy){
		/*keyboard p1, joy0 p2) */
		GameInpConfig(0, 0, 1);
		GameInpConfig(1, 1, 1);
	}
	else {
        	/* p1 joy0 , p2 joy1 */
        	GameInpConfig(0, 1, 1);
        	GameInpConfig(1, 2, 1);
	}
	display_set_controls();
	RunMessageLoop();

	DrvExit();
	MediaExit();

	ConfigAppSave();
	BurnLibExit();
	SDL_Quit();

	if (nExitEmulator == 2) {
		return 2; // timeout
	}

	return 0;
}


/* const */ TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize)
{
	if (pszOutString) {
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (TCHAR*)pszInString;
}


/* const */ char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize)
{
	if (pszOutString) {
		strcpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
}


bool AppProcessKeyboardInput()
{
	return true;
}


static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog) {
	struct GameInp* pgi = NULL;
	unsigned int i;
	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		struct BurnInputInfo bii;
		// Get the extra info about the input
		bii.szInfo = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal == NULL) {
			continue;
		}
		if (bii.szInfo == NULL) {
			bii.szInfo = "";
		}
		GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, bii.szInfo);
	}
	for (i = 0; i < nMacroCount; i++, pgi++) {
		GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, pgi->Macro.szName);
	}
	GameInpCheckLeftAlt();
	return 0;
}

static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi) {
	switch (nPcDev) {
		case 0:
			GamcPlayer(pgi, szi, nPlayer, -1); // Keyboard
			GamcAnalogKey(pgi, szi, nPlayer, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
 			Mapcoins(pgi, szi, nPlayer, nPcDev);
			break;
		case 1:
			GamcPlayer(pgi, szi, nPlayer, 0); // Joystick 1
			GamcAnalogJoy(pgi, szi, nPlayer, 0, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			 Mapcoins(pgi, szi, nPlayer, nPcDev);


			break;
		case 2:
			GamcPlayer(pgi, szi, nPlayer, 1); // Joystick 2
			GamcAnalogJoy(pgi, szi, nPlayer, 1, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
                        Mapcoins(pgi, szi, nPlayer, nPcDev);

			break;
		case 3:
			GamcPlayer(pgi, szi, nPlayer, 2); // Joystick 3
			GamcAnalogJoy(pgi, szi, nPlayer, 2, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			break;
		case 4:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x10, nAnalog); // X-Arcade left side
			GamcMisc(pgi, szi, -1);
			break;
		case 5:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x11, nAnalog); // X-Arcade right side
			GamcMisc(pgi, szi, -1);
			break;
		case 6:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x00, nAnalog); // HotRod left side
			GamcMisc(pgi, szi, -1);
			break;
		case 7:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x01, nAnalog); // HotRod right size
			GamcMisc(pgi, szi, -1);
			break;
	}
}

INT32 Mapcoins(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice)
{
	INT32 nJoyBase = 0;
	if (nDevice == 0) return 0; // using keyboard for this player
 	nDevice--; 
	nJoyBase = 0x4000;
        nJoyBase |= nDevice << 8;
 	bStreetFighterLayout= usesStreetFighterLayout();
	switch (nPlayer) 
	{
		case 0:
			if (strcmp(szi, "p1 coin" ) == 0 && nPlayer == 0 ) 
			{
                                KEY(nJoyBase + 0x80 + 8);
                                return 0;
			        
        		}
                        if (strcmp(szi, "p1 start") == 0 && nPlayer == 0 )
                        {
                                KEY(nJoyBase + 0x80 + 9);
                                return 0;
			}
			if (strncmp(szi, "p1 fire ", 7) == 0  ) 
			{
				char *szb = szi + 7;
				INT32 nButton = strtol(szb, NULL, 0);
				if (nButton >= 1) 
					{
						nButton--;
					}
				if (bStreetFighterLayout ) KEY(nJoyBase + 0x80 + capcom6Layout[nButton]);
				if (!bStreetFighterLayout && usebarcade)  KEY(nJoyBase + 0x80 +  barcade_Layout[nButton])
			}
			break;
                case 1:
                        if (strcmp(szi, "p2 coin" ) == 0 && nPlayer == 1 )
                        {
                                KEY(nJoyBase + 0x80 + 8);
                                return 0;

                        }
                        if (strcmp(szi, "p2 start") == 0 && nPlayer == 1 )
                        {
                                KEY(nJoyBase + 0x80 + 9);
                                  return 0;

                        }
                        if (strncmp(szi, "p2 fire ", 7  ) == 0  &&  bStreetFighterLayout  )
                        {
                                char *szb = szi + 7;
                                INT32 nButton = strtol(szb, NULL, 0);
                                if (nButton >= 1)
                                        {
                                                nButton--;
                                        }
                                if (bStreetFighterLayout ) KEY(nJoyBase + 0x80 + capcom6Layout[nButton]);
                                if (!bStreetFighterLayout && usebarcade)  KEY(nJoyBase + 0x80 +  barcade_Layout[nButton])

			}
                        break;

	}

	return 0;
}


INT32 display_set_controls()
{
        struct GameInp* pgi = NULL;
        unsigned int i;
        for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
                struct BurnInputInfo bii;
                // Get the extra info about the input
                bii.szInfo = NULL;
                BurnDrvGetInputInfo(&bii, i);
                if (bii.pVal == NULL) {
                        continue;
                }
                if (bii.szInfo == NULL) {
                        bii.szInfo = "";
                }
                printf("%s %s\n",  bii.szInfo ,InputCodeDesc( pgi->Input.Switch.nCode));

        }

        return 0;
}

