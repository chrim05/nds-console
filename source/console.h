#pragma once

#include <nds/arm9/console.h>
#include <c++/12.1.0/vector>
#include <c++/12.1.0/string>

#include "basics.h"

#define RED_COL   "\e[1;31m"
#define RESET_COL "\e[0m"

enum class BufferMovingDirection
{
  Up   = -1,
  Down =  1,
};

enum class CursorMovingDirection
{
  Left  = -1,
  Right =  1,
};

enum class SpecialVirtualKey
{
  Left      = -20,
  Right     = -18,
  Up        = -17,
  Down      = -19,
  Backspace =   8,
  Return    =  10,
};

class NDSConsole
{
  private: std::string*              promptBuffer;
  private: std::vector<std::string*> recentPrompts;
  private: uint64_t                  recentPromptsIndex;
  private: uint64_t                  promptCursorIndex;
  private: uint64_t                  maxReachedPromptLength;
  private: Keyboard*                 virtualKeyboard;
  private: PrintConsole*             printableConsole;

  public: NDSConsole(PrintConsole* printableConsole, Keyboard* virtalKeyboard)
  {
    this->promptBuffer           = new std::string();
    this->recentPrompts          = { promptBuffer };
    this->recentPromptsIndex     = 0;
    this->promptCursorIndex      = 0;
    this->maxReachedPromptLength = 0;
    this->virtualKeyboard        = virtualKeyboard;
    this->printableConsole       = printableConsole;

    keyboardShow();
  }

  public: void processVirtualKey(int key)
  {
    // remapping some special virtual keyboard keys
    switch (SpecialVirtualKey(key))
    {
      case SpecialVirtualKey::Left:
        moveCursorIndex(CursorMovingDirection::Left);
        break;
      
      case SpecialVirtualKey::Right:
        moveCursorIndex(CursorMovingDirection::Right);
        break;

      case SpecialVirtualKey::Up:
        moveRecentBuffer(BufferMovingDirection::Up);
        break;
      
      case SpecialVirtualKey::Down:
        moveRecentBuffer(BufferMovingDirection::Down);
        break;

      case SpecialVirtualKey::Backspace:
        removeChar();
        break;
      
      case SpecialVirtualKey::Return:
        returnPrompt();
        break;
      
      default:
        insertChar(char(key));
        break;
    }
  }

  public: void insertChar(char c)
  {
    // the letter has to be added at the top of the string
    if (promptCursorIndex == promptBuffer->length())
    {
      promptBuffer->push_back(c);
      promptCursorIndex++;
    }
    // the letter has to be inserted inside the string
    else
      promptBuffer->insert(promptBuffer->begin() + promptCursorIndex++, c);
    
    // setting the max reached prompt length whether the current prompt length is greater
    if (promptBuffer->length() > maxReachedPromptLength)
      maxReachedPromptLength = promptBuffer->length();
  }

  public: void removeChar()
  {
    // when the prompt buffer is empty there's no need to remove any char
    if (promptCursorIndex == 0)
      return;
    
    // the letter to remove is on the top of the string
    if (promptCursorIndex == promptBuffer->length())
    {
      promptBuffer->pop_back();
      promptCursorIndex--;
      return;
    }

    // the letter to remove is inside the string
    promptBuffer->erase(promptBuffer->begin() + promptCursorIndex--);
  }

  public: void flushPromptBuffer(uint64_t frame, bool printCursor = true)
  {
    // going back at the end of the prompt prefix
    printableConsole->cursorX = getPromptPrefix().length();

    // printing the buffer and the cursor
    for (uint64_t i = 0; i < promptBuffer->length(); i++)
    {
      // printing the cursor when it's inside the string
      if (i == promptCursorIndex)
        printBlinkingCursor(frame, printCursor);

      iprintf("%c", promptBuffer->at(i));
    }

    // printing the cursor when is on the top of the string
    if (promptCursorIndex == promptBuffer->length())
      printBlinkingCursor(frame, printCursor);

    // replacing the overflowed letters with spaces
    for (uint64_t i = 0; i < maxReachedPromptLength - promptBuffer->length(); i++)
      iprintf(" ");
  }

  public: void moveCursorIndex(CursorMovingDirection direction)
  {
    // the cursor index is at the left edge (cannot be moved again)
    if (direction == CursorMovingDirection::Left && promptCursorIndex == 0)
      return;
    
    // the cursor index is at the right edge (cannot be moved again)
    if (direction == CursorMovingDirection::Right && promptCursorIndex == promptBuffer->length())
      return;
    
    promptCursorIndex += uint64_t(direction);
  }

  public: void moveRecentBuffer(BufferMovingDirection direction)
  {
    // the reecent buffer index is at the upper edge (cannot be moved again)
    if (direction == BufferMovingDirection::Up && recentPromptsIndex == 0)
      return;
    
    // the recenet buffer index is at the down edge (cannot be moved again)
    if (direction == BufferMovingDirection::Down && recentPromptsIndex == recentPrompts.size() - 1)
      return;
    
    // updating the recent prompts index and setting up the new prompt buffer
    recentPromptsIndex     += uint64_t(direction);
    this->promptBuffer      = recentPrompts[recentPromptsIndex];
    this->promptCursorIndex = promptBuffer->length();
  }

  public: void printPromptPrefix()
  {
    iprintf("\n%s", getPromptPrefix().c_str());
  }

  public: void returnPrompt()
  {
    // when the prompt buffer is empty there's no need to process the prompted command
    if (promptBuffer->length() == 0)
      return;

    // reprinting the current prompt buffer without the cursor
    flushPromptBuffer(1, false);

    // setting up the new prompt buffer
    // the old one is already saved on the top of recentPrompts
    this->promptBuffer      = new std::string();
    this->promptCursorIndex = 0;
    
    recentPromptsIndex     += 1;
    recentPrompts.push_back(promptBuffer);

    // going to the next line for the prompted command output
    iprintf("\n");

    // processing the prompted command
    processPrompt();

    // initializing the new prompt line
    printPromptPrefix();
  }

  private: void processPrompt()
  {
    iprintf("[system: %d]", system("cd /"));
  }

  private: std::string getPromptPrefix()
  {
    return "/ $ ";
  }

  private: void printBlinkingCursor(uint64_t frame, bool printCursor)
  {
    if (!printCursor)
      return;

    iprintf(frame % 32 <= 16 ? " " : "|");
  }
};