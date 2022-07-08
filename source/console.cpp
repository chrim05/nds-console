#include "console.h"

void NDSConsole::processVirtualKey(int key)
{
  // remapping some special virtual keyboard keys
  switch (key)
  {
    case DVK_LEFT:
      moveCursorIndex(CursorMovingDirection::Left);
      break;
    
    case DVK_RIGHT:
      moveCursorIndex(CursorMovingDirection::Right);
      break;

    case DVK_UP:
      moveRecentBuffer(BufferMovingDirection::Up);
      break;
    
    case DVK_DOWN:
      moveRecentBuffer(BufferMovingDirection::Down);
      break;

    case DVK_BACKSPACE:
      removeChar();
      break;
    
    case DVK_ENTER:
      returnPrompt();
      break;
    
    case DVK_ALT:
    case DVK_CTRL:
    case DVK_SHIFT:
    case DVK_CAPS:
    case DVK_FOLD:
    case DVK_MENU:
      break;
    
    default:
      insertChar(char(key));
      break;
  }
}

void NDSConsole::insertChar(char c)
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

void NDSConsole::removeChar()
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

void NDSConsole::flushPromptBuffer(uint64_t frame, bool printCursor)
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
  for (uint64_t i = 0; i < maxReachedPromptLength + 1 - promptBuffer->length(); i++)
    iprintf(" ");
}

void NDSConsole::moveCursorIndex(CursorMovingDirection direction)
{
  // the cursor index is at the left edge (cannot be moved again)
  if (direction == CursorMovingDirection::Left && promptCursorIndex == 0)
    return;
  
  // the cursor index is at the right edge (cannot be moved again)
  if (direction == CursorMovingDirection::Right && promptCursorIndex == promptBuffer->length())
    return;
  
  promptCursorIndex += uint64_t(direction);
}

void NDSConsole::moveRecentBuffer(BufferMovingDirection direction)
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

void NDSConsole::returnPrompt()
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
  auto result = processCommand(*recentPrompts.at(recentPrompts.size() - 2));
  iprintf("\n\n[result: %s]\n", result.toString().c_str());

  // initializing the new prompt line
  printPromptPrefix();
}

NScript::Node NDSConsole::processCommand(std::string command)
{
  NScript::Parser parser(command);
  return parser.parse();
  // NScript::Evaluator evaluator(parser);
  
  // return evaluator.evaluate();
}

void NDSConsole::printBlinkingCursor(uint64_t frame, bool printCursor)
{
  if (!printCursor)
    return;

  iprintf(frame % 32 <= 16 ? " " : "|");
}