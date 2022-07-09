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

NScript::Node NScript::Parser::nextToken()
{
  // eating all the whitespaces (they have no meaning)
  eatWhitespaces();

  if (eof())
    return Node(NodeKind::Eof, curPos());
  
  auto c = curChar();
  auto t = Node(curPos());

  if (isIdentifierChar(c, true))
    t = convertToKeywordWhenPossible(collectIdentifierToken());
  else if (isNumChar(c, true))
    t = collectNumToken();
  else if (c == '\'')
    t = collectStringToken();
  else if (arrayContains({'+', '-', '*', '/', '(', ')', ','}, c))
    t = Node(NodeKind(c), (NodeValue) { .str = cstringRealloc(std::string(1, c).c_str()) }, curPos());

  exprIndex++;
  return t;
}

NScript::Node NScript::Parser::collectStringToken()
{
  // eating first `'`
  exprIndex++;

  auto startPos = exprIndex - 1;
  auto seq      = collectSequence([this] {
    // any character except `'`, unless it's an escaped character
    return curChar() != '\'' || (curChar(-1) == '\\' && curChar(-2) != '\\');
  });
  auto pos = Position(startPos, exprIndex + 2);

  // eating the last char of string
  // moving to the last `'`
  exprIndex++;

  if (eof())
    throw ParserError({"unclosed string"}, Position(startPos, exprIndex));

  return Node(NodeKind::ConstString, (NodeValue) { .str = cstringRealloc(escapesToEscaped(seq, pos).c_str()) }, pos);
}

NScript::Node NScript::Parser::collectNumToken()
{
  auto startPos = exprIndex;
  auto seq      = collectSequence([this] {
    return isNumChar(curChar(), false);
  });
  auto pos      = Position(startPos, exprIndex + 1);

  // inconsistent numbers like 0.0.1 or 1.2.3 etc
  if (countOccurrences(seq, '.') > 1)
    throw ParserError({"number cannot include more than one dot"}, pos);
  
  // when the user wrote something like 0. or 2. etc
  if (seq[seq.length() - 1] == '.')
    throw ParserError(
      {"number cannot end with a dot (correction: `", seq.substr(0, seq.length() - 1), "`)"},
      pos
    );
  
  auto value = (NodeValue) {
    .num = atof(seq.c_str())
  };

  // when the next char is an identifier, the user wrote something like 123hello or 123_
  if (!eof(+1) && isIdentifierChar(curChar(+1), false))
    throw ParserError(
      {"number cannot include part of identifier (correction: `", seq, " ", std::string(1, curChar(+1)), "...`)"},
      Position(pos.startPos, curPos(+1).endPos)
    );

  return Node(NodeKind::ConstNum, value, pos);
}

NScript::Node NScript::Parser::convertToKeywordWhenPossible(Node token)
{
  return token;

  // if (token.kind != NodeKind::Identifier)
  //  return token;
  
  // if (token.value.str == std::string("true"))
  //   token.kind = NodeKind::True;
  // else if (token.value.str == std::string("false"))
  //   token.kind = NodeKind::False;

  // return token;
}

NScript::Node NScript::Parser::collectIdentifierToken()
{
  auto startPos = exprIndex;
  
  // cstringRealloc is called to have the guarantee that the pointer will not be implicitly deallocated in any case.
  // from https://codeql.github.com/codeql-query-help/cpp/cpp-return-c-str-of-std-string/
  // ```
  //  The pointer is only safe to use while the std::string is still in scope.
  //  When the std::string goes out of scope, its destructor is called and the memory is deallocated, so it is no longer safe to use the pointer.
  // ```
  auto value    = (NodeValue) {
    .str = cstringRealloc(collectSequence([this] {
      return isIdentifierChar(curChar(), false);
    }).c_str())
  };

  return Node(NodeKind::Identifier, value, Position(startPos, exprIndex + 1));
}

std::string NScript::Parser::collectSequence(std::function<bool()> checker)
{
  auto r = std::string();

  // as long as it matches a certain character, adds the latter to the string
  while (!eof() && checker())
  {
    r.push_back(curChar());
    exprIndex++;
  }

  // going back to the last char of sequence
  exprIndex--;

  return r;
}

NScript::Node NScript::Parser::expectBinaryOrTerm(std::function<Node()> expector, std::vector<NodeKind> operators)
{
  auto left = expector();

  // as long as matches one of the required operators, collects the right value and replaces the left one with a BinNode
  while (!eofToken() && arrayContains(operators, curToken.kind))
  {
    auto op = getCurAndAdvance();
    auto right = expector();

    left = Node(NodeKind::Bin, (NodeValue) { .bin = new BinNode(left, right, op) }, Position(left.pos.startPos, right.pos.endPos));
  }

  return left;
}

NScript::Node NScript::Parser::expectTerm()
{
  Node op;
  Node term;

  switch (getCurAndAdvance().kind)
  {
    // simple token
    case NodeKind::Identifier:
    case NodeKind::ConstNum:
    case NodeKind::ConstString:
      term = prevToken;
      break;

    // unary expression = +|- term
    case NodeKind::Plus:
    case NodeKind::Minus:
      op   = prevToken;
      term = expectTerm();
      term = Node(NodeKind::Una, (NodeValue) { .una = new UnaNode(term, op) }, Position(op.pos.startPos, term.pos.endPos));
      break;
    
    case NodeKind::LPar:
      term = expectExpression();
      expectTokenAndAdvance(NodeKind::RPar);
      break;
    
    default:
      throw ParserError({"unexpected token (found `", prevToken.toString(), "`)"}, prevToken.pos);
  }

  if (curToken.kind == NodeKind::LPar)
    term = collectCallNode(term);
  
  return term;
}

NScript::Node NScript::Parser::collectCallNode(Node name)
{
  if (name.kind != NodeKind::Identifier && name.kind != NodeKind::ConstString)
    throw ParserError({"expected string or identifier call name"}, name.pos);
  
  auto startPos = curToken.pos.startPos;
  auto args     = std::vector<Node>();

  // eating first `(`
  advance();

  while (true)
  {
    if (eofToken())
      throw ParserError({"unclosed call parameters list"}, Position(startPos, prevToken.pos.endPos));
    
    if (curToken.kind == NodeKind::RPar)
    {
      // eating last `)`
      advance();
      return Node(NodeKind::Call, (NodeValue) { .call = new CallNode(name, args) }, Position(name.pos.startPos, prevToken.pos.endPos));
    }
    
    // when this is not the first arg
    if (args.size() > 0)
      expectTokenAndAdvance(NodeKind::Comma);
    
    args.push_back(expectExpression());
  }
}

std::string NScript::Parser::escapesToEscaped(std::string s, Position pos)
{
  std::string t;

  for (uint64_t i = 0; i < s.length(); i++)
    if (s[i] == '\\')
    {
      t.push_back(escapeChar(s[i + 1], Position(pos.startPos + i, pos.startPos + i + 1)));

      // skipping the escape code
      i++;
    }
    else
      t.push_back(s[i]);
  
  return t;
}