#include "console.h"

void NDSConsole::processVirtualKey(int key)
{
  // remapping some special virtual keyboard keys
  switch (key)
  {
    case DVK_LEFT:
      moveCursorIndex(MovingDirection2D::LeftOrUp);
      break;
    
    case DVK_RIGHT:
      moveCursorIndex(MovingDirection2D::RightOrDown);
      break;

    case DVK_UP:
      moveRecentBuffer(MovingDirection2D::LeftOrUp);
      break;
    
    case DVK_DOWN:
      moveRecentBuffer(MovingDirection2D::RightOrDown);
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
  promptBuffer->erase(promptBuffer->begin() + --promptCursorIndex);
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

void NDSConsole::moveCursorIndex(MovingDirection2D direction)
{
  // the cursor index is at the left edge (cannot be moved again)
  if (direction == MovingDirection2D::LeftOrUp && promptCursorIndex == 0)
    return;
  
  // the cursor index is at the right edge (cannot be moved again)
  if (direction == MovingDirection2D::RightOrDown && promptCursorIndex == promptBuffer->length())
    return;
  
  promptCursorIndex += uint64_t(direction);
}

void NDSConsole::moveRecentBuffer(MovingDirection2D direction)
{
  // the reecent buffer index is at the upper edge (cannot be moved again)
  if (direction == MovingDirection2D::LeftOrUp && recentPromptsIndex == 0)
    return;
  
  // the recenet buffer index is at the down edge (cannot be moved again)
  if (direction == MovingDirection2D::RightOrDown && recentPromptsIndex == recentPrompts.size() - 1)
    return;
  
  // updating the recent prompts index and setting up the new prompt buffer
  recentPromptsIndex     += uint64_t(direction);
  this->promptBuffer      = recentPrompts[recentPromptsIndex];
  this->promptCursorIndex = promptBuffer->length();
}

void NDSConsole::scrollScreen(MovingDirection2D direction)
{
  // unimplemented yet
}

void NDSConsole::returnPrompt()
{
  // when the prompt buffer is empty there's no need to process the prompted command
  if (promptBuffer->empty())
    return;
  
  // removing the old prompt buffer whether it's empty (returnPrompt worked because promptBuffer was set to another recent prompt)
  if (recentPrompts[recentPrompts.size() - 1]->empty())
    recentPrompts.pop_back();

  // reprinting the current prompt buffer without the cursor
  flushPromptBuffer(1, false);

  // going to the next line for the prompted command output
  iprintf("\n");

  try
  {
    // processing the prompted command
    auto result = processCommand(*promptBuffer);

    // when the expression returns `none` it's not shown up
    if (result.kind != NScript::NodeKind::None)
      iprintf("\n%s\n", result.toString().c_str());
  }
  catch (const NScript::Error& e)
  {
    printPromptParsingError(e);
  }

  // setting up the new prompt buffer
  // the old one is already saved on the top of recentPrompts
  this->promptBuffer      = new std::string();
  this->promptCursorIndex = 0;
  
  // saving the new prompt buffer on the top of recentPrompts
  recentPromptsIndex      = recentPrompts.size();
  recentPrompts.push_back(promptBuffer);

  // initializing the new prompt line
  printPromptPrefix();
}

void NDSConsole::printPromptParsingError(NScript::Error e)
{
  auto promptLength = getPromptPrefix().length();

  // padding the error underlining
  for (uint64_t i = 0; i < promptLength + e.position.startPos; i++)
    iprintf(" ");

  
  // underlining the wrong part
  for (uint64_t i = 0; i < e.position.length(); i++)
    iprintf("-");
  
  // printing the error message
  iprintf("\n\nerror: ");
  for (const auto& m : e.message)
    iprintf("%s", m.c_str());
  
  iprintf("\n");
}

NScript::Node NDSConsole::processCommand(std::string command)
{
  NScript::Parser parser(command);
  
  return evaluator.evaluateNode(parser.parse());
}

void NDSConsole::printBlinkingCursor(uint64_t frame, bool printCursor)
{
  if (!printCursor)
    return;

  iprintf(frame % 32 <= 16 ? " " : "|");
}