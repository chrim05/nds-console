#include "nscript.h"

std::string NScript::Node::toString()
{
  std::string temp;

  switch (kind)
  {
    case NodeKind::ConstNum:    return cutTrailingZeros(std::to_string(value.num));
    case NodeKind::ConstString: return "'" + Parser::escapedToEscapes(value.str) + "'";
    case NodeKind::Bin:         return value.bin->left.toString() + " " + value.bin->op.toString() + " " + value.bin->right.toString();
    case NodeKind::Una:         return value.una->op.toString() + value.una->term.toString();

    case NodeKind::Call:
      for (uint64_t i = 0; i < value.call->args.size(); i++)
      {
        // when this is not the first arg
        if (i > 0)
          temp.append(", ");

        temp.append(value.call->args[i].toString());
      }

      return value.call->name.toString() + "(" + temp + ")";

    case NodeKind::Plus:
    case NodeKind::Minus:
    case NodeKind::Star:
    case NodeKind::Slash:
    case NodeKind::LPar:
    case NodeKind::RPar:
    case NodeKind::Comma:
    case NodeKind::Bad:
    case NodeKind::Identifier:  return value.str;
    case NodeKind::Eof:         return "<eof>";
  }

  panic("unimplemented Node::toString() for some NodeKind");
  return nullptr;
}