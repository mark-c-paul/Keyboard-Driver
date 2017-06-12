
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define GND -1
#define PI2_BCM2708_PERI_BASE 0x3F000000
#define PI2_GPIO_BASE (PI2_BCM2708_PERI_BASE + 0x200000)
#define BLOCK_SIZE (4*1024)
#define GPPUD (0x94 / 4)
#define GPPUDCLK0 (0x98 / 4)

const int state = 0; //set state to 0 for alphabetical, 1 for standard mapping and 2 for alternate
const int delay = 10; //set the amount of time that must pass between key presses
char running = 1; //controls main loop            
volatile unsigned int *gpio; //register table

struct {
	int pin;
	int key;
} *io, //binds key presses to GPIO pins

alphabeticalKeyMap[] = {
  {2,  KEY_X}, {3,  KEY_A}, {4,  KEY_B}, {5,  KEY_C}, {6,  KEY_D}, {7,  KEY_E}, {8,  KEY_F},
  {9,  KEY_G}, {10, KEY_H}, {11, KEY_I}, {12, KEY_J}, {13, KEY_K}, {14, KEY_L}, {15, KEY_M},
  {16, KEY_N}, {17, KEY_O}, {18, KEY_Y}, {19, KEY_T}, {20, KEY_S}, {21, KEY_U}, {22, KEY_P},
  {23, KEY_V}, {24, KEY_W}, {25, KEY_Q}, {26, KEY_Z}, {27, KEY_R},
  {-1, -1}
},

alternateKeyMap[] = { 
  {2,  KEY_B}, {3,  KEY_1}, {4,  KEY_2}, {5,  KEY_3}, {6,  KEY_4}, {7,  KEY_5}, {8,  KEY_6},
  {9,  KEY_7}, {10, KEY_8}, {11, KEY_9}, {12, KEY_0}, {13, KEY_C}, {14, KEY_MINUS},
  {15, KEY_EQUAL}, {16, KEY_TAB}, {17, KEY_LEFTBRACE}, {18, KEY_SPACE}, {19, KEY_CAPSLOCK},
  {20, KEY_ENTER}, {21, KEY_LEFTCTRL}, {22, KEY_RIGHTBRACE}, {23, KEY_DOT}, {24, KEY_COMMA},
  {25, KEY_SEMICOLON}, {26, KEY_BACKSPACE}, {27, KEY_APOSTROPHE},
  {-1, -1}
},

standardKeyMap[] = {
  {2,  KEY_B}, {3,  KEY_Q}, {4,  KEY_W}, {5,  KEY_E}, {6,  KEY_R}, {7,  KEY_T}, {8,  KEY_Y},
  {9,  KEY_U}, {10, KEY_I}, {11, KEY_O}, {12, KEY_P}, {13, KEY_A}, {14, KEY_S}, {15, KEY_D},
  {16, KEY_F}, {17, KEY_G}, {18, KEY_N}, {19, KEY_Z}, {20, KEY_L}, {21, KEY_X}, {22, KEY_H},
  {23, KEY_C}, {24, KEY_V}, {25, KEY_J}, {26, KEY_M}, {27, KEY_K},
  {-1, -1}
};             

void keyRemap(){  //designates what key mapping to use based on state
  if(state==0){ io = alphabeticalKeyMap;}
  else if(state==1){ io = standardKeyMap;}
  else if(state==2){ io = alternateKeyMap;}
  else{running = 0;}
}

void signalHandler(int n) { //properly closes the program to avoid problems with 
  running = 0;              //the pins
}

int pinConfig(int pin, char *attr, char *value) { //modifies pin header
  char filename[50];
  int  fd, w, len = strlen(value);
  sprintf(filename, "%s/gpio%d/%s", "/sys/class/gpio", pin, attr);
  if((fd = open(filename, O_WRONLY)) < 0) return -1;
  w = write(fd, value, len);
  close(fd);
  return (w != len);
}

void cleanup() { //resets pin header
  char buf[50];
  int  fd, i;
  if((fd = open(buf, O_WRONLY)) >= 0) {
    for(i=0; io[i].pin >= 0; i++) {
		if(io[i].key == GND)
		  pinConfig(io[i].pin, "direction", "in");
		  sprintf(buf, "%d", io[i].pin);
	}
  close(fd);
  }
}

void err(char *msg) {  //pin manipulation requires sudo so forgetting it throws an error
  printf("Try typing: 'sudo ./keyboard' instead\n");
  cleanup();
  exit(1);
}


int main(int argc, char *argv[]) {
	char buf[50];     
	char c;                              
	int fd, i, j;         
	int bitmask;      
	int timeout = -1; 
	int intstate[32]; 
	int extstate[32]; 
	int lastKey = -1;
	unsigned long bitMask, bit; 
	volatile unsigned char shortWait;    
	struct input_event keyEv, synEv; 
	struct pollfd p[32];        

	signal(SIGINT , signalHandler);
	signal(SIGKILL, signalHandler);

    keyRemap();

	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) err("File can't open");

	gpio = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, PI2_GPIO_BASE);
	close(fd);    
         
	if(gpio == MAP_FAILED) err("GPIO mapping failed");

	for(bitmask=i=0; io[i].pin >= 0; i++){
	  if(io[i].key != GND) bitmask |= (1 << io[i].pin);
    }
	gpio[GPPUD] = 2;                    
	for(shortWait=150;--shortWait;);        
	gpio[GPPUDCLK0] = bitmask;              
	for(shortWait=150;--shortWait;);        
	gpio[GPPUD] = 0;                    
	gpio[GPPUDCLK0] = 0;
	(void)munmap((void *)gpio, BLOCK_SIZE); 

	sprintf(buf, "%s/export", "/sys/class/gpio");
	if((fd = open(buf, O_WRONLY)) < 0) 
		err("Can't open GPIO export file");
	for(i=j=0; io[i].pin >= 0; i++) { 
		sprintf(buf, "%d", io[i].pin);
	    if(write(fd, buf, strlen(buf))){};             
		pinConfig(io[i].pin, "active_low", "0"); 
		if(io[i].key == GND) {

			if(pinConfig(io[i].pin, "direction", "out") ||
			   pinConfig(io[i].pin, "value"    , "0"))
				err("Pin config failed (GND)");
		} else {

			if(pinConfig(io[i].pin, "direction", "in") ||
			   pinConfig(io[i].pin, "edge"     , "both"))
				err("Pin config failed");

			sprintf(buf, "%s/gpio%d/value","/sys/class/gpio", io[i].pin);

			if((p[j].fd = open(buf, O_RDONLY)) < 0)
				err("Can't access pin value");
			intstate[j] = 0;
			if((read(p[j].fd, &c, 1) == 1) && (c == '0'))
				intstate[j] = 1;
			extstate[j] = intstate[j];
			p[j].events  = POLLPRI;
			p[j].revents = 0;
			j++;
		}
	} 
	close(fd);

#if 1
	if((fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK)) < 0)
		err("Can't open /dev/uinput");
	if(ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
		err("Can't SET_EVBIT");
	for(i=0; io[i].pin >= 0; i++) {
		if(io[i].key != GND) {
			if(ioctl(fd, UI_SET_KEYBIT, io[i].key) < 0)
				err("Can't SET_KEYBIT");
		}
	}
	struct uinput_user_dev uidev;
	memset(&uidev, 0, sizeof(uidev));
	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "retrogame");
	uidev.id.bustype = BUS_USB;
	uidev.id.vendor  = 0x1;
	uidev.id.product = 0x1;
	uidev.id.version = 1;
	if(write(fd, &uidev, sizeof(uidev)) < 0)
		err("write failed");
	if(ioctl(fd, UI_DEV_CREATE) < 0)
		err("DEV_CREATE failed");
#else 
	if((fd = open("/dev/input/event0", O_WRONLY | O_NONBLOCK)) < 0)
		err("Can't open /dev/input/event0");
#endif

	memset(&keyEv, 0, sizeof(keyEv));
	keyEv.type  = EV_KEY;
	memset(&synEv, 0, sizeof(synEv));
	synEv.type  = EV_SYN;
	synEv.code  = SYN_REPORT;
	synEv.value = 0;


  while(running) { 
   if(poll(p, j, timeout) > 0) { 
    for(i=0; i<j; i++) {
      if(p[i].revents) { 
        lseek(p[i].fd, 0, SEEK_SET);
        if(read(p[i].fd, &c, 1)){};
		if(c == '0')      intstate[i] = 1;
        else if(c == '1') intstate[i] = 0;
		p[i].revents = 0; 
       }
     }
     timeout = delay; 
     c = 0;            
    } 
    else if(timeout == delay) { 
     bitMask = 0L;
     bit = 1L;
     for(c=i=j=0; io[i].pin >= 0; i++, bit<<=1) {
       if(io[i].key != GND) {
		 if(intstate[j] != extstate[j]) {
			extstate[j] = intstate[j];
			keyEv.code  = io[i].key;
			keyEv.value = intstate[j];
			if(write(fd, &keyEv,
			  sizeof(keyEv))){};
			  c = 1; 
			  if(intstate[j]) { 
				lastKey = i;
			  }
              else { 
				lastKey = timeout = -1;
			  }
			}
			j++;
			if(intstate[i]) bitMask |= bit;
		  }
	   }
     }  
	else if(lastKey >= 0) { 
	   if(timeout > 30) timeout -= 5; 
	   c = 1; 
	   keyEv.code  = io[lastKey].key;
	   keyEv.value = 2; 
	   if(write(fd, &keyEv, sizeof(keyEv))){};
	}
	if(c) if(write(fd, &synEv, sizeof(synEv))){};
   }

	ioctl(fd, UI_DEV_DESTROY); // Destroy and
	close(fd);                 // close uinput
	cleanup();                 // Un-export pins

	puts("Done.");

	return 0;
}
