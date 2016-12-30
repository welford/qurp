#include "platform.h"

#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <linux/input.h>
#include <libudev.h>

#define MAXEVHANDLES 8	// Arbitary (number of /dev/input/event* to try)

typedef struct
{
	int dev_event[MAXEVHANDLES];

	uint32_t screen_width;
	uint32_t screen_height;
	//udev objects 
	struct udev *udev;	

	struct udev_monitor *p_device_monitor;
	int device_monitor_fd;

	// OpenGL|ES objects
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGL_DISPMANX_WINDOW_T nativewindow;

} STATE;


static void PrintConfigAttributes(EGLDisplay display, EGLConfig config)
{	
	EGLBoolean result;
	EGLint value;

	printf("--------------------------------------------------------------------------\n",value);

	eglGetConfigAttrib(display,config,EGL_CONFIG_ID,&value);
	printf("EGL_CONFIG_ID %d\n",value);		

	eglGetConfigAttrib(display,config,EGL_BUFFER_SIZE,&value);
	printf("EGL_BUFFER_SIZE %d\n",value);
	eglGetConfigAttrib(display,config,EGL_RED_SIZE,&value);
	printf("EGL_RED_SIZE %d\n",value);
	eglGetConfigAttrib(display,config,EGL_GREEN_SIZE,&value);
	printf("EGL_GREEN_SIZE %d\n",value);
	eglGetConfigAttrib(display,config,EGL_BLUE_SIZE,&value);
	printf("EGL_BLUE_SIZE %d\n",value);
	eglGetConfigAttrib(display,config,EGL_ALPHA_SIZE,&value);
	printf("EGL_ALPHA_SIZE %d\n",value);
	eglGetConfigAttrib(display,config,EGL_DEPTH_SIZE,&value);
	printf("EGL_DEPTH_SIZE %d\n",value);
	eglGetConfigAttrib(display,config,EGL_STENCIL_SIZE,&value);
	printf("EGL_STENCIL_SIZE %d\n",value);
	eglGetConfigAttrib(display,config,EGL_SAMPLE_BUFFERS,&value);
	printf("EGL_SAMPLE_BUFFERS %d\n",value);
	eglGetConfigAttrib(display,config,EGL_SAMPLES,&value);
	printf("EGL_SAMPLES %d\n",value);

	eglGetConfigAttrib(display,config,EGL_CONFIG_CAVEAT,&value);
	switch(value)
	{
		case  EGL_NONE : printf("EGL_CONFIG_CAVEAT EGL_NONE\n"); break;
		case  EGL_SLOW_CONFIG : printf("EGL_CONFIG_CAVEAT EGL_SLOW_CONFIG\n"); break;
	}	

	eglGetConfigAttrib(display,config,EGL_MAX_PBUFFER_WIDTH,&value);
	printf("EGL_MAX_PBUFFER_WIDTH %d\n",value);
	eglGetConfigAttrib(display,config,EGL_MAX_PBUFFER_HEIGHT,&value);
	printf("EGL_MAX_PBUFFER_HEIGHT %d\n",value);
	eglGetConfigAttrib(display,config,EGL_MAX_PBUFFER_PIXELS,&value);
	printf("EGL_MAX_PBUFFER_PIXELS %d\n",value);
	eglGetConfigAttrib(display,config,EGL_NATIVE_RENDERABLE,&value);
	printf("EGL_NATIVE_RENDERABLE %s \n",(value ? "true" : "false"));
	eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_ID,&value);
	printf("EGL_NATIVE_VISUAL_ID %d\n",value);
	eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_TYPE,&value);
	printf("EGL_NATIVE_VISUAL_TYPE %d\n",value);				
	eglGetConfigAttrib(display,config,EGL_SURFACE_TYPE,&value);
	printf("EGL_SURFACE_TYPE %d\n",value);
	eglGetConfigAttrib(display,config,EGL_TRANSPARENT_TYPE,&value);
	printf("EGL_TRANSPARENT_TYPE %d\n",value);
}

void ResetInputDevice(STATE* pState, const int idx)
{
	if (idx > -1 && idx < MAXEVHANDLES)
	{
		if (pState->dev_event[idx] != -1)
			close(pState->dev_event[idx]);
	}
}

//this should change, it does look like event increments from 0 but i have no idea
//if that is guranteed, best to store the name along with the index for registering and
//unregistering of files.
void ResetInputDeviceByName(STATE* pState, const char* ev_path)
{
	int idx = -1;

	//get index based on name
	if (ev_path != NULL){
		const char *p_event_str = strstr(ev_path, "/event");
		if (p_event_str){
			const char *p_number = p_event_str + 6; //+6 for "/event"
			idx = atoi(p_number);
		}
	}
	ResetInputDevice(pState, idx);
}

//
void ResetInputDevices(STATE* pState)
{
	int ev_chan;
	for (ev_chan = 0; ev_chan<MAXEVHANDLES; ev_chan++)
	{
		if (pState->dev_event[ev_chan] != -1)
			close(pState->dev_event[ev_chan]);
		pState->dev_event[ev_chan] = -1;
	}
}

//
void RegisterInputDevice(STATE* pState, const unsigned int idx)
{
	// Keyboard and mouse can appear on any event channel
	// FIXME Do we also need to check /dev/input/mouse0 etc?
	// FIXME What about multiple interfaces for the same device?
	if (idx < MAXEVHANDLES)
	{
		//if (pState->dev_event[ev_chan] != -1 && IsValidFD(pState->dev_event[ev_chan])) //already registered, don't bother....
		// continue;

		char ev_path[100]; // FIXME array bounds
		sprintf(ev_path, "/dev/input/event%d", idx);
		pState->dev_event[idx] = open(ev_path, O_RDONLY | O_NONBLOCK);
		if (pState->dev_event[idx] != -1)
		{
#if RELEASE_BUILD
			// Prevent input from propagating to shell (unfortunately
			// this breaks control-c handling)
			ioctl(pState->dev_event[idx], EVIOCGRAB, (void*)1);
#endif
		}
	}
}

//this should change, it does look like event increments from 0 but i have no idea
//if that is guranteed, best to store the name along with the index for registering and
//unregistering of files.
void RegisterInputDeviceByName(STATE* pState, const char* ev_path)
{
	int idx = -1;

	//get index based on name
	if (ev_path != NULL){
		const char *p_event_str = strstr(ev_path, "/event");
		if (p_event_str){
			const char *p_number = p_event_str + 6; //+6 for "/event"
			idx = atoi(p_number);
		}
	}

	// Keyboard and mouse can appear on any event channel
	if (idx > -1 && idx < MAXEVHANDLES)
	{
		pState->dev_event[idx] = open(ev_path, O_RDONLY | O_NONBLOCK);
		if (pState->dev_event[idx] != -1)
		{
#if RELEASE_BUILD
			// Prevent input from propagating to shell (unfortunately
			// this breaks control-c handling)
			ioctl(pState->dev_event[idx], EVIOCGRAB, (void*)1);
#endif
		}
	}
}

int InitialisePlatformGfx(CPlatform* pPlatform, int glMajor, int glMinor, int width, int height, int redBits, int greenBits, int blueBits, int alphaBits, int depthBits, int stencilBits, int nSamples)
{
	int32_t success = 0;
	EGLBoolean result;
	EGLint num_config;

	DISPMANX_ELEMENT_HANDLE_T	dispman_element;
	DISPMANX_DISPLAY_HANDLE_T	dispman_display;
	DISPMANX_UPDATE_HANDLE_T	dispman_update;
	VC_DISPMANX_ALPHA_T			dispman_alpha;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
	EGLConfig config;
	STATE* pState = (STATE*)pPlatform->m_pData;

	pPlatform->m_size.x = width;
	pPlatform->m_size.y = height;

	EGLint attribute_list[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 16,
		EGL_STENCIL_SIZE, 8,
		EGL_SAMPLE_BUFFERS, 0,
		EGL_SAMPLES, 0,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};

	EGLint gles_attribute_list[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 1,
		EGL_NONE
	};

	attribute_list[1] = redBits;
	attribute_list[3] = greenBits;
	attribute_list[5] = blueBits;
	attribute_list[7] = alphaBits;
	attribute_list[9] = depthBits;
	attribute_list[11] = stencilBits;

	if (nSamples > 0)
	{
		attribute_list[13] = 1;
		attribute_list[15] = nSamples;
	}

	gles_attribute_list[1] = glMajor;

	switch (eglQueryAPI())
	{
	case EGL_OPENGL_API:
		printf("EGL_OPENGL_API\n"); break;
	case EGL_OPENGL_ES_API:
		printf("EGL_OPENGL_ES_API\n"); break;
	case EGL_OPENVG_API:
		printf("EGL_OPENVG_API\n"); break;
	}	

	//--------------------------------
	//platform specific stuff
	//--------------------------------

	//must be called before any GPU calls can be made
	bcm_host_init();

	// create an EGL window surface
	success = graphics_get_display_size(0 /* LCD */, &pState->screen_width, &pState->screen_height);
	assert(success >= 0);

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = pState->screen_width;	//resolution on the display device
	dst_rect.height = pState->screen_height;//

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = width << 16;	//gl rendered resolution
	src_rect.height = height << 16;	//	

	dispman_display = vc_dispmanx_display_open(0 /* LCD */);
	dispman_update = vc_dispmanx_update_start(0);

	dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
	dispman_alpha.opacity = 0xFF;
	dispman_alpha.mask = 0; // NULL

	dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
		0/*layer*/, &dst_rect, 0/*src*/,
		//&src_rect, DISPMANX_PROTECTION_NONE, 0/*alpha*/, 0/*clamp*/, 0/*transform*/);
		&src_rect, DISPMANX_PROTECTION_NONE, &dispman_alpha /*alpha*/, 0/*clamp*/, 0/*transform*/);

	pState->nativewindow.element = dispman_element;
	pState->nativewindow.width = width;			//should be the same as the src resolution before the shift
	pState->nativewindow.height = height;
	vc_dispmanx_update_submit_sync(dispman_update);

	//--------------------------------
	//end platform specific stuff
	//--------------------------------

	// get an EGL display connection
	pState->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(pState->display != EGL_NO_DISPLAY);

	// initialize the EGL display connection
	result = eglInitialize(pState->display, NULL, NULL);
	assert(EGL_FALSE != result);

	// get an appropriate EGL frame buffer configuration
	//http://www.khronos.org/registry/egl/sdk/docs/man/xhtml/eglChooseConfig.html
	result = eglChooseConfig(pState->display, attribute_list, &config, 1, &num_config);
	assert(EGL_FALSE != result);

	// create an EGL rendering context
	pState->context = eglCreateContext(pState->display, config, EGL_NO_CONTEXT, gles_attribute_list);
	assert(pState->context != EGL_NO_CONTEXT);

	pState->surface = eglCreateWindowSurface(pState->display, config, &pState->nativewindow, NULL);
	assert(pState->surface != EGL_NO_SURFACE);

	// retain buffer contents 
	result = eglSurfaceAttrib(pState->display, pState->surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
	assert(EGL_FALSE != result);

	// connect the context to the surface
	result = eglMakeCurrent(pState->display, pState->surface, pState->surface, pState->context);
	assert(EGL_FALSE != result);

	PrintConfigAttributes(pState->display, config);
}

int Create(CPlatform* pPlatform, char* title, int glMajor, int glMinor,  int width, int height, int redBits, int greenBits, int blueBits, int alphaBits,int depthBits, int stencilBits, int nSamples)
{
	STATE* pState = 0;
	pState = (STATE*)malloc(sizeof(STATE));
	pPlatform->m_pData = (void*)pState;

//GFX
	InitialisePlatformGfx(pPlatform, glMajor, glMinor, width, height, redBits, greenBits, blueBits, alphaBits, depthBits, stencilBits, nSamples);

//INPUT

	//setup keyboard and mouse input
	memset(pPlatform->m_keyboard.key, 0, KB_MAX);	
	memset(pState->dev_event, -1, sizeof(pState->dev_event));

	// FIXME do we need to initialize pPlatform->m_mouse? It's global
	// so should be preinitialized to zero already.	

	//setup udev stuff so we can enumerate and monitor for 
	// devices being connected or disconnected.
	// Create the udev object
	pState->udev = udev_new();
	if (!pState->udev) {
		printf("Can't create udev! \n");
		exit(1);
	}

	
	//this is the initial enumeration and registering of input devices.
	// we only do this for "event" inputs
	{
		struct udev_enumerate *enumerate;
		struct udev_list_entry *devices, *dev_list_entry;
		struct udev_device *dev;

		enumerate = udev_enumerate_new(pState->udev);
		udev_enumerate_add_match_subsystem(enumerate, "input");
		udev_enumerate_scan_devices(enumerate);
		devices = udev_enumerate_get_list_entry(enumerate);

		udev_list_entry_foreach(dev_list_entry, devices) {
			const char *path, *devnode;
			path = udev_list_entry_get_name(dev_list_entry);
			dev = udev_device_new_from_syspath(pState->udev, path);
			devnode = udev_device_get_devnode(dev);

			if (devnode != NULL){
				RegisterInputDeviceByName(pState, devnode);				
			}
		}
		udev_enumerate_unref(enumerate);
	}

	//create a monitor for input devices being added or removed
	//the monitor is polled on the Tick Function
	pState->p_device_monitor = udev_monitor_new_from_netlink(pState->udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(pState->p_device_monitor, "input", NULL);
	udev_monitor_enable_receiving(pState->p_device_monitor);
	pState->device_monitor_fd = udev_monitor_get_fd(pState->p_device_monitor);	
	
}

// Mouse keycodes (BTN_LEFT etc defined in linux/input.h)
static const int mouse_button_keycodes[] = { BTN_LEFT, BTN_RIGHT, BTN_MIDDLE };

#define MAX_INPUT_EVENTS 20

void Tick(CPlatform* pPlatform,
		void(*input_callback)(unsigned int code, int pressed),
		void(*mouse_callback)(unsigned int code, int pressed))
{
	char tmp = 0;
	int rd = 0, idx = 0, km_idx;
	struct input_event ie[MAX_INPUT_EVENTS];
	int size = sizeof(struct input_event);
	STATE* pState = (STATE*)pPlatform->m_pData;

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// Keyboard input 
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	//quickly go through and untoggle all the old pressed
	for(rd=0;rd<KB_MAX;rd++)
		BUTTON_UNTOGGLE(pPlatform->m_keyboard.key[rd]);

	int ev_chan;
	for (ev_chan=0; ev_chan<MAXEVHANDLES; ev_chan++)
	{
		idx=0;
		if (pState->dev_event[ev_chan] == -1)
			continue;
		
		rd = read (pState->dev_event[ev_chan], ie, size * MAX_INPUT_EVENTS);
		while(rd>0)
		{
			//printf("Event chan=%d type=%d code=%d value=%d\n", ev_chan, ie[idx].type, ie[idx].code, ie[idx].value);

			if(ie[idx].type == EV_KEY && ie[idx].code < LINUX_KB_MAX)
			{
				km_idx = linux_to_keymap[ie[idx].code]; //get the keymap index
				tmp = pPlatform->m_keyboard.key[km_idx];
				BUTTON_RESET(pPlatform->m_keyboard.key[km_idx]);
				if(ie[idx].value)
				{
					input_callback(ie[idx].code, 1);
					BUTTON_PRESS(pPlatform->m_keyboard.key[km_idx]);
					if(!IS_BUTTON_PRESSED(tmp))
						BUTTON_TOGGLE(pPlatform->m_keyboard.key[km_idx]);
				}
				else
				{
					input_callback(ie[idx].code, 0);
					if(IS_BUTTON_PRESSED(tmp))
						BUTTON_TOGGLE(pPlatform->m_keyboard.key[km_idx]);
				}				
			}
			else if (ie[idx].type == EV_KEY)
			{
				int mb;
				for (mb=0; mb<NUMMOUSEBUTTONS; mb++)
				{
					if (ie[idx].code == mouse_button_keycodes[mb])
					{
						tmp = pPlatform->m_mouse.button[mb];
						BUTTON_RESET(pPlatform->m_mouse.button[mb]);
						if(ie[idx].value)
						{
							// FIXME perhaps modify input_callback to handle this
							mouse_callback(mb, 1);
							BUTTON_PRESS(pPlatform->m_mouse.button[mb]);
							if(!IS_BUTTON_PRESSED(tmp))
								BUTTON_TOGGLE(pPlatform->m_mouse.button[mb]);
						}
						else
						{
							mouse_callback(mb, 0);
							if(IS_BUTTON_PRESSED(tmp))
								BUTTON_TOGGLE(pPlatform->m_mouse.button[mb]);
						}				
					}
				}
			}
			else if(ie[idx].type == EV_REL)
			{
				// Accumulate delta until processed and cleared by IN_MouseMove()
				// called from IN_Move() called from CL_SendCmd() in cl_main.c
				if (ie[idx].code == 0)
				{
					pPlatform->m_mouse.dx += ie[idx].value;
				}
				else if (ie[idx].code == 1)
				{
					pPlatform->m_mouse.dy += ie[idx].value;
				}
			}

			rd -= size;			
			idx++;
		}
	}

	//poll the udev stuff for device changes
	{
		struct udev_device *dev;
		fd_set fds;
		struct timeval tv;
		int ret;

		FD_ZERO(&fds);
		FD_SET(pState->device_monitor_fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		ret = select(pState->device_monitor_fd + 1, &fds, NULL, NULL, &tv);

		/* Check if our file descriptor has received data. */
		if (ret > 0 && FD_ISSET(pState->device_monitor_fd, &fds)) {
			// Make the call to receive the device.
			// select() ensured that this will not block.
			dev = udev_monitor_receive_device(pState->p_device_monitor);
			if (dev) {
				const char *p_devnode = udev_device_get_devnode(dev);
				if (p_devnode){
					if (!strcmp(udev_device_get_action(dev), "remove"))
					{
						ResetInputDeviceByName(pState, p_devnode);
					}
					if (!strcmp(udev_device_get_action(dev), "add"))
					{
						RegisterInputDeviceByName(pState, p_devnode);
					}
					/*printf("Got Device\n");
					printf("   Node: %s\n", udev_device_get_devnode(dev));
					printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));
					printf("   Devtype: %s\n", udev_device_get_devtype(dev));

					printf("   Action: %s\n", udev_device_get_action(dev));
					udev_device_unref(dev);*/
				}
			}
			else 
			{
				printf("No Device from receive_device(). An error occured.\n");
			}
		}
	}
}


void SwapBuffers(CPlatform* pPlatform)
{
	STATE* pState = (STATE*)pPlatform->m_pData;
	eglSwapBuffers(pState->display, pState->surface);	

	//const GLenum discards[] = { GL_DEPTH_EXT, GL_STENCIL_EXT };
	//glDiscardFramebufferEXT(GL_FRAMEBUFFER, 2, discards);
}

void Close(CPlatform* pPlatform)
{
	STATE* pState = (STATE*)pPlatform->m_pData;

	//close input streams
	ResetInputDevices(pState);

	udev_unref(pState->udev);	
	udev_monitor_unref(pState->p_device_monitor);

	eglSwapBuffers(pState->display, pState->surface);
	eglMakeCurrent( pState->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	eglDestroySurface( pState->display, pState->surface );
	eglDestroyContext( pState->display, pState->context );
	eglTerminate( pState->display );
	free(pState);	
}
