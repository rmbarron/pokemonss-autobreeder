/*
Nintendo Switch Fightstick - Proof-of-Concept

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

// make && sudo dfu-programmer atmega16u2 erase && sudo dfu-programmer atmega16u2 flash Joystick.hex

// make && ./teensy_loader_cli -mmcu=atmega32u4 -w Joystick.hex

#include "Joystick.h"
#include <time.h>
#include <stdio.h>

#define ECHOES 2
int echoes = 0;
USB_JoystickReport_Input_t last_report;

int report_count = 0;
int xpos = 0;
int ypos = 0;
int duration_count = 0;
int portsval = 0;
int collectCycle = 0; 
int numReleased = 0; 
int boxCycle = 0; 

bool boxOpened = false; 

typedef enum {
	COLLECTING, 
	HATCHING, 
	RELEASING
} Modes; 

Modes mode = HATCHING;
int eggChecks = 300; 
int numBoxes = 4; 

static const command sync[] = {
	// Setup controller
	{ NOTHING,  250 },
	{ TRIGGERS,   5 },
	{ NOTHING,  100 },
	{ TRIGGERS,   5 },
	{ NOTHING,  100 },
	{ A,          5 },
	{ NOTHING,   50 },
};

static const command run[] = {
	{ LEFT,     80},
	{ NOTHING,  5},
	{ RIGHT,    70},
	{ NOTHING,  5},
	{ UPRIGHT,  40},
	{ NOTHING,  10}
};

// //Hatching: 
static const command openPC[] = {
	{X, 5},
	{NOTHING, 45},
	{A, 5}, 
	{NOTHING, 70},
	{R, 5}, 
	{NOTHING, 70},
	{Y, 5}, 
	{NOTHING, 5},
	{Y, 5}, 
	{NOTHING, 5}
};

//Note move to the correct column first
static const command grabColumn[] = {
	{A, 5}, 
	{NOTHING, 5},
	{DOWN, 5}, 
	{NOTHING, 5},
	{DOWN, 5},
	{NOTHING, 5},
	{DOWN, 5}, 
	{NOTHING, 5},
	{DOWN, 5},
	{NOTHING, 5},
	{A, 5},
	{NOTHING, 5}
};
//Allows drops column after 

static const command spin[] = {
	{SPIN, 2800}
};

//move left a certain number of times first if needed 
static const command movePokemon[] = {
	//Move left
	{LEFT, 5},
	{NOTHING, 5},

	//Move right 
	{RIGHT, 5},
	{NOTHING, 5}, 

	//Places eggs down 
	{DOWN, 5},
	{NOTHING, 5},
	{A, 5},
	{NOTHING, 5}
};

static const command release[] = {
	//Release pokemon 
	//a
	{A, 5}, 
	{NOTHING, 10}, 
	//up
	{UP, 5}, 
	{NOTHING, 5}, 
	//up
	{UP, 5}, 
	{NOTHING, 5}, 
	//a
	{A, 5}, 
	{NOTHING, 40}, 
	//up
	{UP, 5}, 
	{NOTHING, 5}, 
	//a	
	{A, 5}, 
	{NOTHING, 65}, 
	{A, 5}, 
	{NOTHING, 40}, 
};

// Main entry point.
int main(void) {
	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	bool setup = true; 
	for(;;) {
		if (setup) {
			command temp = {TRIGGERS, 50};
			runCommand(temp);
			command temp2 = {NOTHING, 5};
			runCommand(temp2);
			command temp3 = {A, 50};
			runCommand(temp3);
			setup = false; 
			if (mode == COLLECTING) {
				command temp4 = { UPRIGHT,    140};
				runCommand(temp4);
			}
		}
		if (mode == COLLECTING) {
			//Walk left to right 
			int a; 
			for (a = 0; a < 3; a ++) {
				runCommand(run[0]);
				runCommand(run[1]);
				runCommand(run[2]);
				runCommand(run[3]);
				runCommand(run[4]);
				runCommand(run[5]);
			}

			//Talk to day care lady
			command a1 = {A, 5};
			runCommand(a1);
			command a2 = {NOTHING, 40};
			runCommand(a2);
			command a3 = {A, 5};
			runCommand(a3);
			command a4 = {NOTHING, 50};
			runCommand(a4);

			//MASH B
			int b; 
			for (b = 0; b < 13; b++) {
				command b1 = {B, 15};
				runCommand(b1);
				command b2 = {NOTHING, 5};
				runCommand(b2);
			}
		}

		if (mode == HATCHING) {
			int numCol;
			for (numCol = 0; numCol < 6; numCol++) {
				//open pc 
				runCommand(openPC[0]); 
				runCommand(openPC[1]); 
				runCommand(openPC[2]); 
				runCommand(openPC[3]); 
				runCommand(openPC[4]); 
				runCommand(openPC[5]); 
				runCommand(openPC[6]);
				runCommand(openPC[7]); 
				runCommand(openPC[8]);
				runCommand(openPC[9]);   
				if (numCol > 0) {
					//put pokemon away 
					runCommand(movePokemon[0]);
					runCommand(movePokemon[1]);
					runCommand(movePokemon[4]);
					runCommand(movePokemon[5]); 

					runCommand(grabColumn[0]);
					runCommand(grabColumn[1]);
					runCommand(grabColumn[2]);
					runCommand(grabColumn[3]);
					runCommand(grabColumn[4]);
					runCommand(grabColumn[5]);
					runCommand(grabColumn[6]);
					runCommand(grabColumn[7]);
					runCommand(grabColumn[8]);
					runCommand(grabColumn[9]);
					runCommand(grabColumn[10]);
					runCommand(grabColumn[11]);
					
					//move to appropriate column 
					int currcol; 
					for (currcol = 0; currcol < numCol; currcol++) {
						runCommand(movePokemon[2]);
						runCommand(movePokemon[3]);
					}
					command up = {UP, 5};
					command up2 = {NOTHING, 5};
					runCommand(up);
					runCommand(up2);
					runCommand(grabColumn[0]);
					runCommand(grabColumn[1]);
				
					//Move to the right by 1 
					runCommand(movePokemon[2]);
					runCommand(movePokemon[3]);
				}
				//Grab first set of eggs 
				runCommand(grabColumn[0]);
				runCommand(grabColumn[1]);
				runCommand(grabColumn[2]);
				runCommand(grabColumn[3]);
				runCommand(grabColumn[4]);
				runCommand(grabColumn[5]);
				runCommand(grabColumn[6]);
				runCommand(grabColumn[7]);
				runCommand(grabColumn[8]);
				runCommand(grabColumn[9]);
				runCommand(grabColumn[10]);
				runCommand(grabColumn[11]);

				//Move them to party 
				int currcol2; 
				for (currcol2 = 0; currcol2 <= numCol; currcol2++) {
					runCommand(movePokemon[0]);
					runCommand(movePokemon[1]);
				}
				runCommand(movePokemon[4]);
				runCommand(movePokemon[5]);
				runCommand(movePokemon[6]);
				runCommand(movePokemon[7]);

				//Mash B
				int b; 
				for (b = 0; b < 18; b++) {
					command b1 = {B, 15};
					runCommand(b1);
					command b2 = {NOTHING, 5};
					runCommand(b2);
				}

				//Hatch eggs 
				runCommand(spin[0]);
				int numEggs;
				for (numEggs = 0; numEggs < 5; numEggs++) {
					int c; 
					for (c = 0; c < 37; c++) {
						command b1 = {B, 15};
						runCommand(b1);
						command b2 = {NOTHING, 5};
						runCommand(b2);
					}
					command shortspin = {SPIN, 20}; 
					runCommand(shortspin);
				}
			}
			//open pc 
			runCommand(openPC[0]); 
			runCommand(openPC[1]); 
			runCommand(openPC[2]); 
			runCommand(openPC[3]); 
			runCommand(openPC[4]); 
			runCommand(openPC[5]); 
			runCommand(openPC[6]);
			runCommand(openPC[7]); 
			runCommand(openPC[8]);
			runCommand(openPC[9]);

			//put pokemon away 
			runCommand(movePokemon[0]);
			runCommand(movePokemon[1]);
			runCommand(movePokemon[4]);
			runCommand(movePokemon[5]); 

			runCommand(grabColumn[0]);
			runCommand(grabColumn[1]);
			runCommand(grabColumn[2]);
			runCommand(grabColumn[3]);
			runCommand(grabColumn[4]);
			runCommand(grabColumn[5]);
			runCommand(grabColumn[6]);
			runCommand(grabColumn[7]);
			runCommand(grabColumn[8]);
			runCommand(grabColumn[9]);
			runCommand(grabColumn[10]);
			runCommand(grabColumn[11]);
			
			//move to appropriate column 
			int currcol; 
			for (currcol = 0; currcol < 6; currcol++) {
				runCommand(movePokemon[2]);
				runCommand(movePokemon[3]);
			}
			command up = {UP, 5};
			command up2 = {NOTHING, 5};
			runCommand(up);
			runCommand(up2);
			runCommand(grabColumn[0]);
			runCommand(grabColumn[1]);
		
			runCommand(up);
			runCommand(up2);
			runCommand(movePokemon[2]);
			runCommand(movePokemon[3]);

			int d; 
			for (d = 0; d < 20; d++) {
				command b1 = {B, 15};
				runCommand(b1);
				command b2 = {NOTHING, 5};
				runCommand(b2);
			}
		}

		if (mode == RELEASING && numReleased < numBoxes) {
			//open box 
			if (boxOpened == false) {
				runCommand(openPC[0]); 
				runCommand(openPC[1]); 
				runCommand(openPC[2]); 
				runCommand(openPC[3]); 
				runCommand(openPC[4]); 
				runCommand(openPC[5]);
				boxOpened = true;  
			}
			int col; 
			for (col = 0; col < 6; col++) {
				int row; 
				for (row = 0; row < 5; row++) {
					//release 
					runCommand(release[0]);
					runCommand(release[1]);
					runCommand(release[2]);
					runCommand(release[3]);
					runCommand(release[4]);
					runCommand(release[5]);
					runCommand(release[6]);
					runCommand(release[7]);
					runCommand(release[8]);
					runCommand(release[9]);
					runCommand(release[10]);
					runCommand(release[11]);
					runCommand(release[12]);
					runCommand(release[13]);
					//go down 

					runCommand(movePokemon[4]);
					runCommand(movePokemon[5]); 
				}
				//go back to top and move over 
					runCommand(movePokemon[4]);
					runCommand(movePokemon[5]); 
					runCommand(movePokemon[4]);
					runCommand(movePokemon[5]); 
					runCommand(movePokemon[2]);
					runCommand(movePokemon[3]); 		
			}
			//Nextbox: 
			command NextBox = {R, 5}; 
			command pause = {NOTHING, 5};
			runCommand(movePokemon[2]);
			runCommand(movePokemon[3]); 
			runCommand(NextBox);
			runCommand(pause); 

			numReleased++; 	
		}
	}
}

void runCommandList(command moves[]) {
	int a = 0; 
	for (a = 0; a < (sizeof(moves) / sizeof(moves[0])); a++) {
		runCommand(moves[a]);
	}
}
void runCommand(command move) {
		duration_count = 0; 
		while(duration_count < move.duration) {
			// We need to run our task to process and deliver data for our IN and OUT endpoints.
			HID_Task(move);
			// We also need to run the main USB management task.
			USB_USBTask();
		}
		
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
	// We need to disable watchdog if enabled by bootloader/fuses.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// We need to disable clock division before initializing the USB hardware.
	clock_prescale_set(clock_div_1);
	// We can then initialize our hardware and peripherals, including the USB stack.

	#ifdef ALERT_WHEN_DONE
	// Both PORTD and PORTB will be used for the optional LED flashing and buzzer.
	#warning LED and Buzzer functionality enabled. All pins on both PORTB and \
PORTD will toggle when printing is done.
	DDRD  = 0xFF; //Teensy uses PORTD
	PORTD =  0x0;
                  //We'll just flash all pins on both ports since the UNO R3
	DDRB  = 0xFF; //uses PORTB. Micro can use either or, but both give us 2 LEDs
	PORTB =  0x0; //The ATmega328P on the UNO will be resetting, so unplug it?
	#endif
	// The USB stack should be initialized last.
	USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
	// We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
	// We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	// We setup the HID report endpoints.
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

	// We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
	// We can handle two control requests: a GetReport and a SetReport.

	// Not used here, it looks like we don't receive control request from the Switch.
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(command move) {
	// If the device isn't connected and properly configured, we can't do anything here.
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;

	// We'll start with the OUT endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
	// We'll check to see if we received something on the OUT endpoint.
	if (Endpoint_IsOUTReceived())
	{
		// If we did, and the packet has data, we'll react to it.
		if (Endpoint_IsReadWriteAllowed())
		{
			// We'll create a place to store our data received from the host.
			USB_JoystickReport_Output_t JoystickOutputData;
			// We'll then take in that data, setting it up in our storage.
			while(Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL) != ENDPOINT_RWSTREAM_NoError);
			// At this point, we can react to this data.

			// However, since we're not doing anything with this data, we abandon it.
		}
		// Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
		Endpoint_ClearOUT();
	}

	// We'll then move on to the IN endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
	// We first check to see if the host is ready to accept data.
	if (Endpoint_IsINReady())
	{
		// We'll create an empty report.
		USB_JoystickReport_Input_t JoystickInputData;
		// We'll then populate this report with what we want to send to the host.
		GetNextReport(&JoystickInputData, move);
		// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
		while(Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL) != ENDPOINT_RWSTREAM_NoError);
		// We then send an IN packet on this endpoint.
		Endpoint_ClearIN();
	}
}

typedef enum {
	SYNC_CONTROLLER,
	SYNC_POSITION,
	BREATHE,
	PROCESS,
	CLEANUP,
	DONE
} State_t;
State_t state = SYNC_CONTROLLER;Endpoint_Write_Stream;

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t* const ReportData, command move) {

	// Prepare an empty report
	memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
	ReportData->LX = STICK_CENTER;
	ReportData->LY = STICK_CENTER;
	ReportData->RX = STICK_CENTER;
	ReportData->RY = STICK_CENTER;
	ReportData->HAT = HAT_CENTER;

	// Repeat ECHOES times the last report
	if (echoes > 0)
	{
		memcpy(ReportData, &last_report, sizeof(USB_JoystickReport_Input_t));
		echoes--;
		return;
	}

	// States and moves management
	switch (state)
	{
		case SYNC_CONTROLLER:
			state = BREATHE;
			break;

		case SYNC_POSITION:
			ReportData->Button = 0;
			ReportData->LX = STICK_CENTER;
			ReportData->LY = STICK_CENTER;
			ReportData->RX = STICK_CENTER;
			ReportData->RY = STICK_CENTER;
			ReportData->HAT = HAT_CENTER;


			state = BREATHE;
			break;

		case BREATHE:
			state = PROCESS;
			break;

		case PROCESS:

			switch (move.button)
			{

				case UP:
					ReportData->LY = STICK_MIN;				
					break;
				case UPRIGHT:
					ReportData->LY = STICK_MIN;
					ReportData->LX = STICK_MAX;				
					break;
				
				case LEFT:
					ReportData->LX = STICK_MIN;				
					break;

				case DOWN:
					ReportData->LY = STICK_MAX;				
					break;

				case RIGHT:
					ReportData->LX = STICK_MAX;				
					break;

				case PLUS:
					ReportData->Button |= SWITCH_PLUS;
					break;

				case MINUS:
					ReportData->Button |= SWITCH_MINUS;
					break;

				case A:
					ReportData->Button |= SWITCH_A;
					break;

				case B:
					ReportData->Button |= SWITCH_B;
					break;

				case X:
					ReportData->Button |= SWITCH_X;
					break;

				case Y:
					ReportData->Button |= SWITCH_Y;
					break;

				case R:
					ReportData->Button |= SWITCH_R;
					break;

				case L:
					ReportData->Button |= SWITCH_L;
					break;

				case THROW:
					ReportData->LY = STICK_MIN;				
					ReportData->Button |= SWITCH_R;
					break;

				case TRIGGERS:
					ReportData->Button |= SWITCH_L | SWITCH_R;
					break;

				case SPIN: 
					ReportData->LX = STICK_MIN;		
					ReportData->RX = STICK_MAX;
					break; 		

				default:
					ReportData->LX = STICK_CENTER;
					ReportData->LY = STICK_CENTER;
					ReportData->RX = STICK_CENTER;
					ReportData->RY = STICK_CENTER;
					ReportData->HAT = HAT_CENTER;
					break;
			}

			duration_count++;
			break;

		case CLEANUP:
			state = DONE;
			break;

		case DONE:
			#ifdef ALERT_WHEN_DONE
			portsval = ~portsval;
			PORTD = portsval; //flash LED(s) and sound buzzer if attached
			PORTB = portsval;
			_delay_ms(250);
			#endif
			return;
	}


	// Prepare to echo this report
	memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t));
	echoes = ECHOES;

}
