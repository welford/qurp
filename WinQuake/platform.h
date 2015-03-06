#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "keymap.h"

typedef struct 
{
	float x, y;
}PVec2f;

typedef struct 
{
	int x, y;
}PVec2i;

//Basic input states that we can expect from most 
//application instances

#define NUMMOUSEBUTTONS 3

typedef struct
{
	int px,py;
	int dx,dy;
	int wheelRot;
	// int lmb;
	// int mmb;
	// int rmb;
	int button[NUMMOUSEBUTTONS];	// ordered left, right, middle
}SMouse;

#define BUTTON_PRESSED			0x1
#define BUTTON_TOGGLED			0x2
#define BUTTON_PRESS_TOGGLE		0x3
#define BUTTON_PRESS_RELEASE	0x2

#define BUTTON_RESET(A) A=0
#define BUTTON_PRESS(A) A|=BUTTON_PRESSED
#define BUTTON_TOGGLE(A) A|=BUTTON_TOGGLED
#define BUTTON_UNTOGGLE(A) A &=~BUTTON_TOGGLED

#define IS_BUTTON_PRESSED(A) (A & BUTTON_PRESSED)
#define IS_BUTTON_TOGGLE_PRESSED(A) (A == BUTTON_PRESS_TOGGLE)
#define IS_BUTTON_TOGGLE_RELEASED(A) (A == BUTTON_PRESS_RELEASE)

typedef struct
{
	char key[KB_MAX];
}Keyboard;

typedef struct
{
	PVec2f leftStick, rightStick;
	float leftTrigger,rightTrigger;
	char d_up,d_down,d_left,d_right; //dpad
	char fb_up,fb_down,fb_left,fb_right; //face buttons
	char start,select;
}Gamepad;

//not too keen on this, should just switch it to being pure C
//and maybe make a wrapper for it
typedef struct 
{
	int			m_quit;			//
	float		m_dt;			//delta time value
	PVec2i		m_size;			//
	PVec2i		m_position;		//
	SMouse		m_mouse;		//
	Keyboard	m_keyboard;		//
	Gamepad		m_gamepad;		//
	int			m_showingDebugConsole;
	void		*m_pData;		// platform specific data		
	/*
	//runtime generally called once per frame
	void Tick(void);
	void UpdateBuffers(void);
	void ShowDebugConsole(void);
	void HideDebugConsole(void);

	//Get/Set
	float GetDT(void)const{return m_dt;}		
	int Quit(void)const{return m_quit;}

	PVec2i&	GetSize(void){return m_size;}
	PVec2i&	GetPosition(void){return m_position;}
	SMouse& GetMouse(){return m_mouse;}
	Keyboard& GetKeyboard(){return m_keyboard;}	

	unsigned int  PosX(void)const{return m_position.x;}
	unsigned int  PosY(void)const{return m_position.y;}
	unsigned int  Width(void)const{return m_size.x;}
	unsigned int  Height(void)const{return m_size.y;}		
	const SMouse& GetMouseState() const{return m_mouse;}
	const Keyboard& GetKeyboardState() const{return m_keyboard;}
	const Gamepad& GetGamepad(unsigned int) const{return m_gamepad;}

	CPlatform(void);
	virtual ~CPlatform(void);
	*/
}CPlatform;

//allocate memory ebfore this
extern int Create(CPlatform* pPlatform, char* title, int glMajor, int glMinor, int width, int height, int redBits, int greeenBits, int blueBits, int alphaBits,int depthBits, int stencilBits, int nSamples);
extern void Tick(CPlatform* pPlatform, void(*input_callback)(unsigned int code, int pressed) );
extern void SwapBuffers(CPlatform* pPlatform);
extern void Close(CPlatform* pPlatform);

#endif //_PLATFORM_H_
