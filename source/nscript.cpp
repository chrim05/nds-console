#include "nscript.h"

std::string NScript::Node::toString()
{
  switch (kind)
  {
    case NodeKind::ConstNum:    return std::to_string(value.num);
    case NodeKind::ConstString: return "'" + std::string(value.str) + "'";
    case NodeKind::Identifier:  return value.str;
    case NodeKind::Bin:         return value.bin->left.toString() + " " + value.bin->op.toString() + " " + value.bin->right.toString();
    case NodeKind::Plus:        return "+";
    case NodeKind::Minus:       return "-";
    case NodeKind::Star:        return "*";
    case NodeKind::Slash:       return "/";
    case NodeKind::Bad:         return "?";
    case NodeKind::Eof:         return "!";
  }

  panic("unimplemented Node::toString() for some NodeKind");
  return nullptr;
}