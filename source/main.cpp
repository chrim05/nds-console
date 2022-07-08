// Compiling for ARM9

#undef ARM7
#undef ARM9

#define ARM9

// Devkitpro headers and ARM9 libc++

#include <nds.h>
#include <stdio.h>

#include "basics.h"
#include "console.h"

// Console for Nintendo DS

int main()
{
  PrintConsole printConsole;
  Keyboard     virtualKeyboard;

  // initialize video
  videoSetMode(MODE_0_2D);
  videoSetModeSub(MODE_0_2D);

  // initialize vram
  vramSetPrimaryBanks(VRAM_A_MAIN_BG, VRAM_B_MAIN_SPRITE, VRAM_C_SUB_BG, VRAM_D_SUB_SPRITE);

  // initialize print console on top screen and virtual keyboard on sub screen
  consoleInit(&printConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 2, 0, true, true);
  keyboardInit(&virtualKeyboard, 0, BgType_Text4bpp, BgSize_T_256x512, 14, 0, false, true);

	NDSConsole console(&printConsole, &virtualKeyboard);

	iprintf("Nintendo DS ARM9 Console\n");
  console.printPromptPrefix();
 
	for (uint64_t frame = 0; true; frame++)
	{
    // reading the pressed letter
    auto keyboardKey = keyboardUpdate();

    // when virtual key is pressed
    if (keyboardKey != -1)
      console.processVirtualKey(keyboardKey);
    
    // updating the key state
    scanKeys();

    // getting the last key state
		auto buttonKey = keysDown();

    // processing the physical button keys
    switch (buttonKey)
    {
      case KEY_LEFT:  console.moveCursorIndex(CursorMovingDirection::Left);  break;
      case KEY_RIGHT: console.moveCursorIndex(CursorMovingDirection::Right); break;
      case KEY_UP:    console.moveRecentBuffer(BufferMovingDirection::Up);   break;
      case KEY_DOWN:  console.moveRecentBuffer(BufferMovingDirection::Down); break;
      case KEY_B:     console.removeChar();                                  break;
      case KEY_A:     console.returnPrompt();                                break;
    }

    console.flushPromptBuffer(frame);
    swiWaitForVBlank();
	}

	return 0;
}