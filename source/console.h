#pragma once

#include <nds.h>
#include <nds/arm9/console.h>
#include <c++/12.1.0/vector>
#include <c++/12.1.0/string>

#include "basics.h"
#include "nscript.h"

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

class NDSConsole
{
  private: std::string*              promptBuffer;
  private: std::vector<std::string*> recentPrompts;
  private: uint64_t                  recentPromptsIndex;
  private: uint64_t                  promptCursorIndex;
  private: uint64_t                  maxReachedPromptLength;
  private: Keyboard*                 virtualKeyboard;
  private: PrintConsole*             printableConsole;
  private: NScript::Evaluator        evaluator;

  public: NDSConsole(PrintConsole* printableConsole, Keyboard* virtalKeyboard)
  {
    this->promptBuffer           = new std::string();
    this->recentPrompts          = { promptBuffer };
    this->recentPromptsIndex     = 0;
    this->promptCursorIndex      = 0;
    this->maxReachedPromptLength = 0;
    this->virtualKeyboard        = virtualKeyboard;
    this->printableConsole       = printableConsole;
    this->evaluator              = NScript::Evaluator();

    keyboardShow();
  }

  public: void processVirtualKey(int key);

  public: void insertChar(char c);

  public: void removeChar();

  public: void flushPromptBuffer(uint64_t frame, bool printCursor);

  public: void moveCursorIndex(CursorMovingDirection direction);

  public: void moveRecentBuffer(BufferMovingDirection direction);

  public: void returnPrompt();

  public: inline void printPromptPrefix()
  {
    iprintf("\n%s", getPromptPrefix().c_str());
  }

  private: inline std::string getPromptPrefix()
  {
    return "/ $ ";
  }

  private: void printPromptParsingError(NScript::Error e);

  private: NScript::Node processCommand(std::string command);

  private: void printBlinkingCursor(uint64_t frame, bool printCursor);
};