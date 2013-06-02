/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// in_rpi.c 

#include "quakedef.h"
//#include "vgakeyboard.h"

static unsigned char scantokey[128];

void Init_KBD(void)
{

	int i;
	//if (COM_CheckParm("-nokbd")) UseKeyboard = 0;
	for (i=0 ; i<128 ; i++)
		scantokey[i] = ' ';

	scantokey[42] = K_SHIFT;
	scantokey[54] = K_SHIFT;
	scantokey[72] = K_UPARROW;
	scantokey[103] = K_UPARROW;
	scantokey[80] = K_DOWNARROW;
	scantokey[108] = K_DOWNARROW;
	scantokey[75] = K_LEFTARROW;
	scantokey[105] = K_LEFTARROW;
	scantokey[77] = K_RIGHTARROW;
	scantokey[106] = K_RIGHTARROW;
	scantokey[29] = K_CTRL;
	scantokey[97] = K_CTRL;
	scantokey[56] = K_ALT;
	scantokey[100] = K_ALT;
//	scantokey[58] = JK_CAPS;
//	scantokey[69] = JK_NUM_LOCK;
	scantokey[71] = K_HOME;
	scantokey[73] = K_PGUP;
	scantokey[79] = K_END;
	scantokey[81] = K_PGDN;
	scantokey[82] = K_INS;
	scantokey[83] = K_DEL;
	scantokey[1 ] = K_ESCAPE;
	scantokey[28] = K_ENTER;
	scantokey[15] = K_TAB;
	scantokey[14] = K_BACKSPACE;
	scantokey[119] = K_PAUSE;
	scantokey[57] = ' ';

	scantokey[102] = K_HOME;
	scantokey[104] = K_PGUP;
	scantokey[107] = K_END;
	scantokey[109] = K_PGDN;
	scantokey[110] = K_INS;
	scantokey[111] = K_DEL;

	scantokey[2] = '1';
	scantokey[3] = '2';
	scantokey[4] = '3';
	scantokey[5] = '4';
	scantokey[6] = '5';
	scantokey[7] = '6';
	scantokey[8] = '7';
	scantokey[9] = '8';
	scantokey[10] = '9';
	scantokey[11] = '0';
	scantokey[12] = '-';
	scantokey[13] = '=';
	scantokey[41] = '`';
	scantokey[26] = '[';
	scantokey[27] = ']';
	scantokey[39] = ';';
	scantokey[40] = '\'';
	scantokey[51] = ',';
	scantokey[52] = '.';
	scantokey[53] = '/';
	scantokey[43] = '\\';

	scantokey[59] = K_F1;
	scantokey[60] = K_F2;
	scantokey[61] = K_F3;
	scantokey[62] = K_F4;
	scantokey[63] = K_F5;
	scantokey[64] = K_F6;
	scantokey[65] = K_F7;
	scantokey[66] = K_F8;
	scantokey[67] = K_F9;
	scantokey[68] = K_F10;
	scantokey[87] = K_F11;
	scantokey[88] = K_F12;
	scantokey[30] = 'a';
	scantokey[48] = 'b';
	scantokey[46] = 'c';
	scantokey[32] = 'd';       
	scantokey[18] = 'e';       
	scantokey[33] = 'f';       
	scantokey[34] = 'g';       
	scantokey[35] = 'h';       
	scantokey[23] = 'i';       
	scantokey[36] = 'j';       
	scantokey[37] = 'k';       
	scantokey[38] = 'l';       
	scantokey[50] = 'm';       
	scantokey[49] = 'n';       
	scantokey[24] = 'o';       
	scantokey[25] = 'p';       
	scantokey[16] = 'q';       
	scantokey[19] = 'r';       
	scantokey[31] = 's';       
	scantokey[20] = 't';       
	scantokey[22] = 'u';       
	scantokey[47] = 'v';       
	scantokey[17] = 'w';       
	scantokey[45] = 'x';       
	scantokey[21] = 'y';       
	scantokey[44] = 'z';       

	scantokey[78] = '+';
	scantokey[74] = '-';
}

void IN_KB_CALLBACK(unsigned int code, int press){
	Key_Event(scantokey[code], press);
}

void IN_Init (void)
{
	Init_KBD();
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

void IN_Move (usercmd_t *cmd)
{
}

