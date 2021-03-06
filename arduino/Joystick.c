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
int baseeggstepmult = 2;

bool boxOpened = false;

typedef enum {
	COLLECTING,
	COLLECT_THEN_HATCH,
	HATCHING,
	RELEASING,
	RAIDRESETTING,
	FLY
} Modes;

Modes mode = HATCHING;
int numBoxes = 4;
// Separate globals are used across COLLECTING and HATCHING modes to make them
// individually configurable. When using COLLECT_THEN_HATCH, the hatching
// params will be overridden by the collecting params (overcoming all those
// sneaky human errors in keeping the numbers consistent).

// Globals used during COLLECTING.
int eggsToCollect = 30;
int boxesForward = 0;
// When putting eggs away in COLLECTING mode, we need to keep track of
// where in the box we are. Since nothing is multi-threaded, this is relatively
// safe.
// currentRow will be 0-4, and currentColumn 0-5.
// note to self: if we grab 5 at a time, we don't have to care about row
// TODO: Change collect() to accept row & col to place.
// TODO: Change collect() to return the next row & col & bool for moving to the
// next box.
int currentRow = 0;
int currentColumn = 0;
// Globals used during HATCHING.
// We hatch in columns, which are 5 eggs at a time.
// If using COLLECT_THEN_HATCH, these are overridden to comply with
// COLLECTING args.
// The remainder of eggs (eggsToCollect % 30) won't be hatched.
int boxesToHatch = 8;

static const command sync[] = {
	// Setup controller
	{ NOTHING,  250 },
	{ TRIGGERS,   5 },
	{ NOTHING,  100 },
	{ TRIGGERS,   5 },
	{ NOTHING,  100 },
	{ A,          5 },
	{ NOTHING,   50 }
};

static const command run[] = {
	{ LEFT,     80},
	{ NOTHING,  5},
	{ RIGHT,    70},
	{ NOTHING,  5},
	{ UPRIGHT,  40},
	{ NOTHING,  10}
};


// Assumes menu is already over "Pokemon"
static const command openPC[] = {
	{X, 5},
	{NOTHING, 45},
	{A, 5},
	{NOTHING, 70},
	{R, 5},
	{NOTHING, 70},
	// Puts in "multipurpose" select mode
	{Y, 5},
	{NOTHING, 5},
	// Puts in "multiselect" select mode
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
	//20 cycle
	{SPIN, 2800}
	//40 cycle
	//{SPIN, 4900}
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
	//up0
	{UP, 5},
	{NOTHING, 5},
	//a
	{A, 5},
	{NOTHING, 65},
	{A, 5},
	{NOTHING, 40},
};

static const command bMovement[] = {
	{UP, 5},
	{RIGHT, 5},
	{DOWN, 5},
	{LEFT, 5}
};

static const command nothing[] = {
	{NOTHING, 5},
	{NOTHING, 10},
	{NOTHING, 20},
	{NOTHING, 30},
	{NOTHING, 40},
};

static const command buttons[] = {
	{HOME, 5},
	{A, 5},
	{B, 5},
	{X, 5},
	{Y, 5}
};

// Main entry point.
int main(void) {
	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	bool setup = true;
	if (setup) {
		command temp = {TRIGGERS, 50};
		runCommand(temp);
		command temp2 = {NOTHING, 5};
		runCommand(temp2);
		command temp3 = {A, 50};
		runCommand(temp3);
		setup = false;
		if (mode == COLLECTING || mode == COLLECT_THEN_HATCH) {
			command temp4 = { UPRIGHT,    140};
			runCommand(temp4);
		}
		if (mode == COLLECT_THEN_HATCH) {
			// Init the args to hatch().
			boxesToHatch = eggsToCollect / 5;
		}
	}
	if (mode == COLLECTING || mode == COLLECT_THEN_HATCH) {
		int i;
		for (i = 0; i < eggsToCollect; i++) {
			collect();
		}
	}
	// We moved forward in the box during egg collecting.
	// So we have to move back to the box we started at in the PC.
	if (mode == COLLECT_THEN_HATCH) {
		openBoxMultipurpose();

		command doUp = {UP, 5};
		command doNothing = {NOTHING, 10};
		command doLeft = {LEFT, 5};

		runCommand(doUp);
		runCommand(doNothing);
		int i;
		for (i = 0; i < boxesForward; i++) {
			runCommand(doLeft);
			runCommand(doNothing);
		}

		// Mash B to exit the box.
		int b;
		for (b = 0; b < 13; b++) {
			command b1 = {B, 15};
			runCommand(b1);
			command b2 = {NOTHING, 5};
			runCommand(b2);
		}
	}
	if (mode == COLLECT_THEN_HATCH || mode == HATCHING) {
		int i;
		for (i = 0; i < boxesToHatch; i++) {
			hatch();
			// Move to the next box.
			openBox();
			command doNothing = {NOTHING, 10};
			command doUp = {UP, 5};
			command doRight = {RIGHT, 5};
			command doB = {B, 5};

			runCommand(doUp);
			runCommand(doNothing);
			runCommand(doRight);
			runCommand(doNothing);

			// Mash b to exit the box.
			int b;
			for (b = 0; b < 13; b++) {
				runCommand(doB);
				runCommand(doNothing);
			}
		}
	}
/*
	if(mode == FLY) {
		runCommand(buttons[3]);  // x
		runCommand(nothing[20]);
		runCommand(buttons[1]);  // a
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		runCommand(nothing[2]);

		command a1 = {DOWN, 6};
		runCommand(a1);
		runCommand(nothing[1]);
		command a2 = {RIGHT, 10};
		runCommand(a2);
		runCommand(buttons[1]);  // a
		runCommand(nothing[3]);
		runCommand(buttons[1]);  // a
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		command a3 = {PLUS, 5};
		runCommand(a3);
		runCommand(nothing[1]);
		command a4 = {RIGHT, 300};
		runCommand(a4);
		command a5 = {L, 5};
		runCommand(a5);
		// Move from map to pokemon box?
		runCommand(buttons[3]); // x
		runCommand(nothing[20]);
		runCommand(bMovement[0]);  // UP
		runCommand(bMovement[1]);  // RIGHT
		runCommand(buttons[2]);  // B
		runCommand(nothing[2]);
		putPokemonAway(1);
		command a6 = {R, 5};
		runCommand(a6);
		int b;
		for (b = 0; b < 18; b++) {
			command b1 = {B, 15};
			runCommand(b1);
			command b2 = {NOTHING, 5};
			runCommand(b2);
		}
		mode = HATCHING;
	}

	if (mode == HATCHING) {
		int i = 0;
		for (i = 0; i < 11; i++) {
			hatch();
		}
	}

	if (mode == RELEASING) {
		//open box
		int i;
		for (i = 0; i < numBoxes; i++) {
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
		}
	}

//a, a, a, a(wait 30), home
//on home menu: down, right, right, right, right, a
//Hold down for a fair amount of time lets say 40?
//a, down, down, down, down, a
//down, down, a
//right, up, right, right, right, right, right, a, home, home, b(15), a, long wait (70)
	if (mode == RAIDRESETTING) {
		runCommand(buttons[1]);
		runCommand(nothing[2]);
		runCommand(buttons[1]);
		runCommand(nothing[4]);
		runCommand(nothing[1]);
		runCommand(buttons[1]);
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		runCommand(nothing[3]);
		runCommand(buttons[0]);
		runCommand(nothing[2]);
		runCommand(bMovement[2]);
		runCommand(bMovement[1]);
		runCommand(bMovement[1]);
		runCommand(bMovement[1]);
		runCommand(bMovement[1]);
		runCommand(buttons[1]);
		runCommand(nothing[2]);
		command longDown = {DOWN, 80};
		runCommand(longDown);
		runCommand(buttons[1]);
		runCommand(bMovement[2]);
		runCommand(nothing[0]);
		runCommand(bMovement[2]);
		runCommand(nothing[0]);
		runCommand(bMovement[2]);
		runCommand(nothing[0]);
		runCommand(bMovement[2]);
		runCommand(nothing[1]);
		runCommand(buttons[1]);
		runCommand(nothing[2]);
		runCommand(bMovement[2]);
		runCommand(nothing[1]);
		runCommand(bMovement[2]);
		runCommand(nothing[1]);
		runCommand(buttons[1]);
		runCommand(nothing[1]);
		runCommand(bMovement[1]);
		runCommand(bMovement[0]);
		runCommand(bMovement[1]);
		runCommand(bMovement[1]);
		runCommand(bMovement[1]);
		runCommand(bMovement[1]);
		runCommand(bMovement[1]);
		runCommand(buttons[1]);
		runCommand(nothing[2]);
		runCommand(buttons[0]);
		runCommand(nothing[3]);
		runCommand(buttons[0]);
		runCommand(nothing[3]);
		runCommand(buttons[2]);
		runCommand(nothing[4]);
		runCommand(nothing[3]);
		runCommand(buttons[1]);
		runCommand(nothing[3]);
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		runCommand(nothing[4]);
		runCommand(nothing[4]);
	}
*/
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

// collect will walk back and forth along the breeding bridge, and collect
// a single egg from the day care worker.
// To collect multiple eggs, put this in a loop (typically grabbing 5 at a time)
void collect() {
		//Walk left to right
	int a;
	// This for loop is a bit of a slider
	// The fewer passes back and forth, the more quickly you'll get eggs with some
	// error rate in eggs not being ready.
	// The more passes made, gathering will be slower but with a higher
	// success rate.
	for (a = 0; a < 6; a ++) {
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

	// Mash B
	// We do this for 2 reasons:
	// 1 as a safety check against when we talk to the day care lady and
	// an egg wasn't ready for us.
	// 2 to go through all the "Look, you got an egg! :D" flow.
	int b;
	for (b = 0; b < 13; b++) {
		command b1 = {B, 15};
		runCommand(b1);
		command b2 = {NOTHING, 5};
		runCommand(b2);
	}

	// Put this single egg away.
	// We could run 5 times, and put the entire column away. That would be
	// simpler, but potentially introduces a very-rare edge case where a pokemon
	// hatches while we're collecting, so this puts them away immediately.

	openBoxMultipurpose();
	// The box opens with the cursor on first PC block. The egg will
	// be the second party member.
	command doNothing = {NOTHING, 10};
	command o1 = {LEFT, 5};
	runCommand(o1);
	runCommand(doNothing);
	command o2 = {DOWN, 5};
	runCommand(o2);
	runCommand(doNothing);
	command o3 = {A, 5};
	runCommand(o3);
	runCommand(doNothing);
	// With the pokemon picked up, move to the first PC block.
	command o4 = {RIGHT, 5};
	runCommand(o4);
	runCommand(doNothing);
	command o5 = {UP, 5};
	runCommand(o5);
	runCommand(doNothing);

	// Now we're at 0, 0 on our grid, and can move to the exact spot to put the
	// egg down.

	int c;
	for (c = 0; c < currentRow; c++) {
		command c1 = {DOWN, 5};
		runCommand(c1);
		runCommand(doNothing);
	}
	int d;
	for (d = 0; d < currentColumn; d++) {
		command d1 = {RIGHT, 5};
		runCommand(d1);
		runCommand(doNothing);
	}
	command placePokemon = {A, 5};
	runCommand(placePokemon);
	runCommand(doNothing);

	// Now that we've placed the pokemon, we just have to tell our future
	// self where the next available row x col pair is.
	currentRow++;
	if (currentRow > 4) {
		currentColumn++;
		currentRow = 0;
	}
	if (currentColumn > 5) {
		moveToNextBox();
		currentColumn = 0;
		boxesForward++;
	}

	// Cool, we've placed a pokemon, now just need to exit the PC and do it all again.
	int e;
	for (e = 0; e < 13; e++) {
		command b1 = {B, 15};
		runCommand(b1);
		command b2 = {NOTHING, 5};
		runCommand(b2);
	}

// TODO: mode change after # of eggs should be optional
//	mode = FLY;
}

// hatch hatches a column of 5 eggs at a time.
// Each call to hatch will hatch a single box of 30 eggs.
void hatch() {
	command doNothing = {NOTHING, 10};
	command doUp = {UP, 5};
	command doRight = {RIGHT, 5};
	command doLeft = {LEFT, 5};
	command doDown = {DOWN, 5};
	command doA = {A, 5};
	command doB = {B, 5};
	int currCol;

	// Grab the currCol column, put it in the party, then put it back.
	for (currCol = 0; currCol < 6; currCol++) {
		openBox();

		//runCommand(doUp);
		//runCommand(doNothing);

		int c;
		for (c = 0; c < currCol; c++) {
			runCommand(doRight);
			runCommand(doNothing);
		}

		selectColumn();

		// The cursor is now at the top of the column holding a column of eggs.
		int d;
		for (d = 0; d <= currCol; d++) {
			runCommand(doLeft);
			runCommand(doNothing);
		}
		runCommand(doDown);
		runCommand(doNothing);
		runCommand(doA);
		runCommand(doNothing);

		// Mash B to get out of the box.
		int b;
		for (b = 0; b < 13; b++) {
			runCommand(doB);
			runCommand(doNothing);
		}

		// Now for the actual work.
		// Run back and forth for ~2800 inputs.
		// TODO: Is there a way to find #inputs:cycles in game?
		// Since we're usually next to the day care lady, each pass through run[]
		// is 160 inputs.
		int r;
		// A hatch happened at 53 for eevee.
		for (r = 0; r < 55; r++) {
			runCommand(run[0]);
			runCommand(run[1]);
			runCommand(run[2]);
			runCommand(run[3]);
			runCommand(run[4]);
			runCommand(run[5]);
		}

		// More B mashing to get through all the egg hatch dialogue.
		int numEggs;
		for (numEggs = 0; numEggs < 5; numEggs++) {
			int numEggsB;
			// TODO: Can we optimize this time any?
			for (numEggsB = 0; numEggsB < 80; numEggsB++){
				runCommand(doB);
				runCommand(doNothing);
			}
			// Very minor inputs to trigger the egg hatches
			// Since we put them in the box immediately, they should hatch at the
			// same time.
			// duration of 5 doesn't always trigger the next hatch.
			command doLeft20 = {LEFT, 20};
			command doRight20 = {RIGHT, 20};
			runCommand(doLeft20);
			runCommand(doNothing);
			runCommand(doRight20);
			runCommand(doNothing);
		}

		// Now we have a party full of hatched pokemon and need to put them back.
		openBox();
		runCommand(doLeft);
		runCommand(doNothing);
		runCommand(doDown);
		runCommand(doNothing);

		selectColumn();

		runCommand(doRight);
		runCommand(doNothing);
		runCommand(doUp);
		runCommand(doNothing);

		// We're back at 0, 0 and can put the hatched pokemon in their column.
		int e;
		for (e = 0; e < currCol; e++) {
			runCommand(doRight);
			runCommand(doNothing);
		}
		runCommand(doA);
		runCommand(doNothing);

		// Lastly, we mash B to exit the box.
		int f;
		for (f = 0; f < 13; f++) {
			runCommand(doB);
			runCommand(doNothing);
		}
	}
}
/*
void hatch() {
	int numCol;
	for (numCol = 0; numCol < 6; numCol++) {
		openBox();
		if (numCol > 0) {
			//put pokemon away
			runCommand(movePokemon[0]);
			runCommand(movePokemon[1]);
			runCommand(movePokemon[4]);
			runCommand(movePokemon[5]);

			selectColumn();

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
		selectColumn();

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
	openBox();

	//put pokemon away
	runCommand(movePokemon[0]);
	runCommand(movePokemon[1]);
	runCommand(movePokemon[4]);
	runCommand(movePokemon[5]);

	selectColumn();

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
*/
// openBox opens your box in "Multiselect" mode, where an entire column
// of pokemon can be moved at once.
// Assumes menu is over "Pokemon" tab.
void openBox() {
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
}

// openBoxMultipurpose opens your box in "multipurpose" mode,
// where one can move a single pokemon at a time with only
// a single "A" to pick them up.
// Assumes menu is over "Pokemon" tab.
void openBoxMultipurpose() {
	runCommand(openPC[0]);
	runCommand(openPC[1]);
	runCommand(openPC[2]);
	runCommand(openPC[3]);
	runCommand(openPC[4]);
	runCommand(openPC[5]);
	runCommand(openPC[6]);
	runCommand(openPC[7]);
}

// moveToNextBox moves to the next box in the PC.
// Assumes the cursor is currently on the last block of the current box.
void moveToNextBox() {
	command doUp = {UP, 5};
	command doNothing = {NOTHING, 10};
	command doRight = {RIGHT, 5};
	int a;
	for (a = 0; a < 5; a++) {
		runCommand(doUp);
		runCommand(doNothing);
	}
	runCommand(doRight);
	runCommand(doNothing);
}

void selectColumn() {
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
}

void putPokemonAway(int numCol) {
	openBox();
	command a5 = {L, 5};
	runCommand(a5);
	runCommand(nothing[1]);
	runCommand(movePokemon[0]);
	runCommand(movePokemon[1]);
	runCommand(movePokemon[4]);
	runCommand(movePokemon[5]);
	selectColumn();
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

				case HOME:
					ReportData->Button |= SWITCH_HOME;
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

			duration_count++;  // Used to check against move.duration in runCommand
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
