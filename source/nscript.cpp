#include "nscript.h"

std::string NScript::Node::toString()
{
  std::string temp;

  switch (kind)
  {
    case NodeKind::Num:         return cutTrailingZeros(std::to_string(value.num));
    case NodeKind::String:      return "'" + Parser::escapedToEscapes(value.str) + "'";
    case NodeKind::Bin:         return value.bin->left.toString() + " " + value.bin->op.toString() + " " + value.bin->right.toString();
    case NodeKind::Una:         return value.una->op.toString() + value.una->term.toString();
    case NodeKind::Assign:      return value.assign->name.toString() + " = " + value.assign->expr.toString();

    case NodeKind::Call:
      return value.call->name.toString() + "(" + joinArray<Node>(", ", value.call->args, [] (Node arg) { return arg.toString(); }) + ")";

    case NodeKind::Plus:
    case NodeKind::Minus:
    case NodeKind::Star:
    case NodeKind::Slash:
    case NodeKind::LPar:
    case NodeKind::RPar:
    case NodeKind::Comma:
    case NodeKind::Eq:
    case NodeKind::Bad:
    case NodeKind::None:
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
    return Node::eof(curPos());
  
  auto c = curChar();
  auto t = Node();

  // collecting token
  if (isIdentifierChar(c, true))
    t = convertToKeywordWhenPossible(collectIdentifierToken());
  else if (isNumChar(c, true))
    t = collectNumToken();
  else if (c == '\'')
    t = collectStringToken();
  else if (arrayContains({'+', '-', '*', '/', '(', ')', ',', '='}, c))
    t = Node(NodeKind(c), (NodeValue) { .str = cstringRealloc(std::string(1, c).c_str()) }, curPos());
  else
    t = Node::bad(cstringRealloc(std::string(1, c).c_str()), curPos());

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
    throw Error({"unclosed string"}, Position(startPos, exprIndex));

  return Node(NodeKind::String, (NodeValue) { .str = cstringRealloc(escapesToEscaped(seq, pos).c_str()) }, pos);
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
    throw Error({"number cannot include more than one dot"}, pos);
  
  // when the user wrote something like 0. or 2. etc
  if (seq[seq.length() - 1] == '.')
    throw Error(
      {"number cannot end with a dot (correction: `", seq.substr(0, seq.length() - 1), "`)"},
      pos
    );
  
  auto value = (NodeValue) {
    .num = atof(seq.c_str())
  };

  // when the next char is an identifier, the user wrote something like 123hello or 123_
  if (!eof(+1) && isIdentifierChar(curChar(+1), false))
    throw Error(
      {"number cannot include part of identifier (correction: `", seq, " ", std::string(1, curChar(+1)), "...`)"},
      Position(pos.startPos, curPos(+1).endPos)
    );

  return Node(NodeKind::Num, value, pos);
}

NScript::Node NScript::Parser::convertToKeywordWhenPossible(Node token)
{
  if (token.kind != NodeKind::Identifier)
   return token;
  
  if (token.value.str == std::string("none"))
    token.kind = NodeKind::None;

  return token;
}

NScript::Node NScript::Parser::collectIdentifierToken()
{
  auto startPos = exprIndex;
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
  auto op   = Node();
  auto term = Node();

  switch (getCurAndAdvance().kind)
  {
    // simple token
    case NodeKind::Identifier:
    case NodeKind::Num:
    case NodeKind::String:
    case NodeKind::None:
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
      throw Error({"unexpected token (found `", prevToken.toString(), "`)"}, prevToken.pos);
  }

  if (curToken.kind == NodeKind::LPar)
    term = collectCallNode(term);
  else if (curToken.kind == NodeKind::Eq)
    term = collectAssignNode(term);
  
  return term;
}

NScript::Node NScript::Parser::collectAssignNode(Node name)
{
  if (name.kind != NodeKind::Identifier)
    throw Error({"expected an identifier when assigning"}, name.pos);

  // eating `=`
  advance();
  auto expr = expectExpression();

  return Node(NodeKind::Assign, (NodeValue) { .assign = new AssignNode(name, expr) }, Position(name.pos.startPos, expr.pos.endPos));
}

NScript::Node NScript::Parser::collectCallNode(Node name)
{
  if (name.kind != NodeKind::Identifier && name.kind != NodeKind::String)
    throw Error({"expected string or identifier call name"}, name.pos);
  
  auto startPos = curToken.pos.startPos;
  auto args     = std::vector<Node>();

  // eating first `(`
  advance();

  while (true)
  {
    if (eofToken())
      throw Error({"unclosed call parameters list"}, Position(startPos, prevToken.pos.endPos));
    
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

NScript::Node NScript::Evaluator::expectType(Node node, NodeKind type)
{
  if (node.kind != type)
    throw Error({"expected a value with type `", Node::kindToString(type), "` (found `", Node::kindToString(node.kind), "`)"}, node.pos);
  
  return node;
}

void NScript::Evaluator::expectArgsCount(CallNode call, uint64_t count)
{
  if (call.args.size() != count)
    throw Error({"expected `", std::to_string(count), "` args (found `", std::to_string(call.args.size()), "`)"}, call.name.pos);
}

NScript::Node NScript::Evaluator::builtinFloor(CallNode call)
{
  expectArgsCount(call, 1);

  // truncating the float value
  auto expr = expectType(evaluateNode(call.args[0]), NodeKind::Num);
  expr.value.num = uint64_t(expr.value.num);

  return expr;
}

void NScript::Evaluator::builtinPrint(CallNode call)
{
  // printing all arguments without separation and flushing
  for (auto arg : call.args)
    iprintf("%s", arg.toString().c_str());
  
  fflush(stdout);
}

NScript::Node NScript::Evaluator::evaluateCallProcess(CallNode call, Position pos)
{
  auto processPath = cstringRealloc(getFullPath(expectNonEmptyStringAndGetString(call.name), true).c_str());
  auto processArgv = new char*[call.args.size() + 2];

  processArgv[0] = (char*)processPath;

  for (uint64_t i = 1; i < call.args.size(); i++)
    processArgv[i] = (char*)cstringRealloc(expectStringLengthAndGetString(evaluateNode(call.args[i]), [] (uint64_t l) { return true; }).c_str());
  
  processArgv[call.args.size()] = (char*)nullptr;

  return Node(NodeKind::Num, (NodeValue) { .num = float64(execv(processPath, processArgv)) }, pos);
}

NScript::Node NScript::Evaluator::evaluateCall(CallNode call, Position pos)
{
  // when the call's name is a string, searches for a process with that filename
  if (call.name.kind == NodeKind::String)
    return evaluateCallProcess(call, pos);
  
  // otherwise searches for a builtin function with that name
  auto name = std::string(call.name.value.str);

  if (name == "print")
    builtinPrint(call);
  else if (name == "floor")
    return builtinFloor(call);
  else if (name == "cd")
    builtinCd(call);
  else if (name == "clear")
    builtinClear(call);
  else if (name == "shutdown")
    builtinShutdown(call);
  else if (name == "ls")
    builtinLs(call);
  else if (name == "rmdir")
    builtinRmDir(call);
  else if (name == "mkdir")
    builtinMkDir(call);
  else if (name == "rmfile")
    builtinRmFile(call);
  else if (name == "write")
    builtinWrite(call);
  else if (name == "read")
    return builtinRead(call, pos);
  else
    throw Error({"unknown builtin function"}, call.name.pos);
  
  return Node::none(pos);
}

NScript::Node NScript::Evaluator::evaluateAssign(AssignNode assign, Position pos)
{
  auto name = std::string(assign.name.value.str);
  auto expr = evaluateNode(assign.expr);

  for (uint64_t i = 0; i < map.size(); i++)
    if (map[i].key == name)
    {
      // the variable is already declared (overwrites old value)
      map[i].val = expr;
      return Node::none(pos);
    }

  // the variable is not declared yet (appends a new definition)
  map.push_back(KeyPair<std::string, Node>(name, expr));
  return Node::none(pos);
}

NScript::Node NScript::Evaluator::evaluateUna(UnaNode una)
{
  auto term = evaluateNode(una.term);

  // unary can only be applied to numbers
  if (term.kind != NodeKind::Num)
    throw Error({"type `", Node::kindToString(term.kind), "` does not support unary `", Node::kindToString(una.op.kind), "`"}, term.pos);
  
  term.value.num *= una.op.kind == NodeKind::Minus ? -1 : +1;
  return term;
}

cstring_t NScript::Evaluator::evaluateOperationStr(Node op, cstring_t l, cstring_t r)
{
  // string only supports `+` op
  if (op.kind != NodeKind::Plus)
    throw Error({"string does not support bin `", Node::kindToString(op.kind), "`"}, op.pos);

  return cstringRealloc((std::string(l) + r).c_str());
}

float64 NScript::Evaluator::evaluateOperationNum(NodeKind op, float64 l, float64 r, Position rPos)
{
  switch (op)
  {
    case NodeKind::Plus:  return l + r;
    case NodeKind::Minus: return l - r;
    case NodeKind::Star:  return l * r;
    case NodeKind::Slash:
      if (r == 0)
        throw Error({"dividing by 0"}, rPos);

      return l / r;
    
    default: panic("unreachable"); return 0;
  }
}

NScript::Node NScript::Evaluator::evaluateBin(BinNode bin)
{
  auto left  = evaluateNode(bin.left);
  auto right = evaluateNode(bin.right);

  // every bin op can only be applied to values of same type
  if (left.kind != right.kind)
    throw Error(
      {"unkwnon bin `", bin.op.toString(), "` between different types (`", Node::kindToString(left.kind), "` and `", Node::kindToString(right.kind), "`)"},
      bin.op.pos
    );
  
  // recognizing the values' types
  switch (left.kind)
  {
    case NodeKind::Num:
      left.value.num = evaluateOperationNum(bin.op.kind, left.value.num, right.value.num, right.pos);
      break;
    
    case NodeKind::String:
      left.value.str = evaluateOperationStr(bin.op, left.value.str, right.value.str);
      break;

    default:
      throw Error(
        {"type `", Node::kindToString(left.kind), "` does not support bin"},
        bin.op.pos
      );
  }

  // the returning value is gonna have the same pos of the entire bin node
  left.pos.endPos = right.pos.endPos;
  return left;
}

NScript::Node NScript::Evaluator::evaluateIdentifier(Node identifier)
{
  for (const auto& kv : map)
    if (kv.key == std::string(identifier.value.str))
      return kv.val;
  
  throw Error({"unknown variable"}, identifier.pos);
}

NScript::Node NScript::Evaluator::evaluateNode(Node node)
{
  switch (node.kind)
  {
    case NodeKind::Num:
    case NodeKind::String:
    case NodeKind::None:       return node;
    case NodeKind::Bin:        return evaluateBin(*node.value.bin);
    case NodeKind::Una:        return evaluateUna(*node.value.una);
    case NodeKind::Identifier: return evaluateIdentifier(node);
    case NodeKind::Assign:     return evaluateAssign(*node.value.assign, node.pos);
    case NodeKind::Call:       return evaluateCall(*node.value.call, node.pos);
    default:                   panic("unimplemented evaluateNode for some NodeKind"); return Node::none(node.pos);
  }
}

std::string NScript::Evaluator::expectStringLengthAndGetString(Node node, std::function<bool(uint64_t)> f)
{
  auto s = std::string(node.value.str);

  if (!f(s.length()))
    throw Error({"expected a string with a different length"}, node.pos);
  
  return s;
}

void NScript::Evaluator::builtinCd(CallNode call)
{
  expectArgsCount(call, 1);

  // expecting the only 1 arg is a string and expecting it to be a non-empty one
  auto arg            = call.args[0];
  auto dir            = expectNonEmptyStringAndGetString(expectType(evaluateNode(arg), NodeKind::String));
  
  dir = getFullPath(dir, false);

  // opening dir
  auto openedDir = opendir(dir.c_str());

  // reinterpreting the path to get a simplified one
  dir = getRealPath(dir);

  // checking for dir correctly opened
  if (!openedDir)
    throw Error({"unknown dir `", dir, "`"}, arg.pos);
  
  // closing dir, because we will no longer need it (opened just to check that it existed)
  closedir(openedDir);

  // changing dir
  cwd = dir;
}

void NScript::Evaluator::builtinClear(CallNode call)
{
  expectArgsCount(call, 0);
  consoleClear();
}

void NScript::Evaluator::builtinShutdown(CallNode call)
{
  expectArgsCount(call, 0);
  systemShutDown();
}

void NScript::Evaluator::builtinLs(CallNode call)
{
  auto dir = opendir(cwd.c_str());

  // iterating the directory
  while (auto entry = readdir(dir))
    iprintf(
      "%s (%s)\n", entry->d_name,
      // not all file systems support dirent.d_type, when possible prints:
      //  `file`   -> for regular files
      //  `folder` -> for directories
      //  `other`  -> for other elment's types (see https://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.5/html_node/Directory-Entries.html)
      //  `?`      -> for unknown elements (they could be files, folders or other)
      entry->d_type == DT_REG ? "file" : entry->d_type == DT_DIR ? "folder" : entry->d_type == DT_UNKNOWN ? "?" : "other");
  
  closedir(dir);
}

void NScript::Evaluator::builtinRmDir(CallNode call)
{
  expectArgsCount(call, 1);

  auto arg  = call.args[0];
  auto path = getFullPath(expectNonEmptyStringAndGetString(evaluateNode(arg)), false);

  // removing all files and sub folders into directory (rmdir can only remove empty folders)
  removeAllInsideDir(path);

  // removing the empty folder
  if (rmdir(path.c_str()))
    throw Error({"unable to delete folder `", path, "`"}, arg.pos);
}

void NScript::Evaluator::builtinMkDir(CallNode call)
{
  expectArgsCount(call, 1);

  auto arg  = call.args[0];
  auto path = getFullPath(expectNonEmptyStringAndGetString(evaluateNode(arg)), false);

  if (mkdir(path.c_str(), S_IRUSR))
    throw Error({"unable to make folder `", path, "`"}, arg.pos);
}

void NScript::Evaluator::builtinRmFile(CallNode call)
{
  expectArgsCount(call, 1);

  auto arg  = call.args[0];
  auto path = getFullPath(expectNonEmptyStringAndGetString(evaluateNode(arg)), true);

  if (remove(path.c_str()))
    throw Error({"unable to delete file `", path, "`"}, arg.pos);
}

void NScript::Evaluator::builtinWrite(CallNode call)
{
  expectArgsCount(call, 2);

  auto arg     = call.args[0];
  auto arg2    = call.args[1];
  auto path    = getFullPath(expectNonEmptyStringAndGetString(evaluateNode(arg)), true);
  auto content = expectStringLengthAndGetString(evaluateNode(arg2), [] (uint64_t l) { return true; });
  auto file    = fopen(path.c_str(), "wb");

  if (!file)
    throw Error({"unable to make file `", path, "`"}, arg.pos);
  
  fiprintf(file, content.c_str());
  fclose(file);
}

NScript::Node NScript::Evaluator::builtinRead(CallNode call, Position pos)
{
  expectArgsCount(call, 1);

  auto arg     = call.args[0];
  auto path    = getFullPath(expectNonEmptyStringAndGetString(evaluateNode(arg)), true);
  auto content = std::string();
  auto file    = fopen(path.c_str(), "rb");

  if (!file)
    throw Error({"unable to open file `", path, "`"}, arg.pos);

  auto c = getc(file);

  while (c != EOF) {
    content.push_back(c);
    c = getc(file);
  }

  fclose(file);
  return Node(NodeKind::String, (NodeValue) { .str = cstringRealloc(content.c_str()) }, pos);
}

std::string NScript::Evaluator::expectNonEmptyStringAndGetString(Node node)
{
  return expectStringLengthAndGetString(node, [] (uint64_t l) { return l > 0; });
}

std::string NScript::Evaluator::getFullPath(std::string path, bool shouldBeFile)
{
  auto isRelativePath = path[0] != '/';

  // dir must always have a character `/` at the end of the string
  if (!shouldBeFile)
    path = addTrailingSlashToPath(path);

  // when relative, adds the parent's folder's path
  if (isRelativePath)
    path = cwd + path;
  
  return path;
}