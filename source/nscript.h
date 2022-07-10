#pragma once

#include <nds.h>
#include <c++/12.1.0/string>
#include <c++/12.1.0/vector>
#include <c++/12.1.0/functional>
#include <c++/12.1.0/utility>

#include "basics.h"

namespace NScript
{
  class Position
  {
    public: uint64_t startPos;
    public: uint64_t endPos;

    public: Position(uint64_t startPos, uint64_t endPos)
    {
      this->startPos = startPos;
      this->endPos   = endPos;
    }

    public: Position()
    {
      *this = Position(0, 0);
    }

    public: inline uint64_t length()
    {
      return endPos - startPos;
    }
  };

  enum class NodeKind
  {
    Bin,
    Una,
    Call,
    Assign,
    Bad,
    Eof,
    None,
    Num,
    String,
    Identifier,
    Plus  = '+',
    Minus = '-',
    Star  = '*',
    Slash = '/',
    LPar  = '(',
    RPar  = ')',
    Comma = ',',
    Eq    = '=',
  };

  class BinNode;
  class UnaNode;
  class CallNode;
  class AssignNode;
  
  union NodeValue
  {
    public: float64     num;
    public: cstring_t   str;
    public: BinNode*    bin;
    public: UnaNode*    una;
    public: CallNode*   call;
    public: AssignNode* assign;
    public: void_t      none;
  };

  class Node
  {
    public: NodeKind  kind;
    public: NodeValue value;
    public: Position  pos;

    public: Node()
    {
      *this = Node(Position());
    }

    public: Node(Position position)
    {
      *this = Node(NodeKind::Bad, position);
    }

    public: Node(NodeKind kind, NodeValue value, Position pos)
    {
      this->kind  = kind;
      this->value = value;
      this->pos   = pos;
    }

    public: Node(NodeKind kind, Position position)
    {
      *this = Node(kind, (NodeValue) { .none = 0 }, position);
    }

    public: static Node none(Position pos)
    {
      return Node(NodeKind::None, pos);
    }

    public: static std::string kindToString(NodeKind kind)
    {
      switch (kind)
      {
        case NodeKind::Num:         return "num";
        case NodeKind::String:      return "str";
        case NodeKind::Bin:         return "bin";
        case NodeKind::Una:         return "una";
        case NodeKind::Call:        return "call";
        case NodeKind::Assign:      return "assign";
        case NodeKind::None:        return "none";
        case NodeKind::Plus:
        case NodeKind::Minus:
        case NodeKind::Star:
        case NodeKind::LPar:
        case NodeKind::RPar:
        case NodeKind::Comma:
        case NodeKind::Eq:
        case NodeKind::Slash:       return std::string(1, char(kind));
        case NodeKind::Identifier:  return "id";
        case NodeKind::Bad:         return "<bad>";
        case NodeKind::Eof:         return "<eof>";
      }

      panic("unimplemented Node::toString() for some NodeKind");
      return nullptr;
    }

    public: std::string toString();
  };

  class BinNode
  {
    public: Node left;
    public: Node right;
    public: Node op;

    public: BinNode(Node left, Node right, Node op)
    {
      this->left  = left;
      this->right = right;
      this->op    = op;
    }
  };

  class UnaNode
  {
    public: Node term;
    public: Node op;

    public: UnaNode(Node term, Node op)
    {
      this->term = term;
      this->op   = op;
    }
  };

  class CallNode
  {
    public: Node              name;
    public: std::vector<Node> args;

    public: CallNode(Node name, std::vector<Node> args)
    {
      this->name = name;
      this->args = args;
    }
  };

  class AssignNode
  {
    public: Node name;
    public: Node expr;

    public: AssignNode(Node name, Node expr)
    {
      this->name = name;
      this->expr = expr;
    }
  };

  class Error : std::exception
  {
    public: std::vector<std::string> message;
    public: Position                 position;

    public: Error(std::vector<std::string> message, Position position)
    {
      this->message  = message;
      this->position = position;
    }
  };

  class Parser
  {
    private: std::string expression;
    private: uint64_t    exprIndex;
    private: Node        curToken;
    private: Node        prevToken;

    public: Parser(std::string expression)
    {
      this->expression = expression;
      this->exprIndex  = 0;
    }

    public: inline Node parse()
    {
      // fetching the first token
      advance();

      // the main expression
      auto expr = expectExpression();

      // the main expression is not alone in the prompt
      expectTokenAndAdvance(NodeKind::Eof);

      return expr;
    }

    private: inline Node expectExpression()
    {
      // expression     = sub_expression +|- sub_expression ...
      // sub_expression = term           *|/ term           ...
      // term           = id|num|str
      return expectBinaryOrTerm([this] {
        return expectBinaryOrTerm([this] {
          return expectTerm();
        }, { NodeKind::Star, NodeKind::Slash });
      }, { NodeKind::Plus, NodeKind::Minus });
    }

    private: inline Node expectTokenAndAdvance(NodeKind kind)
    {
      if (curToken.kind != kind)
        throw Error({"expected `", Node::kindToString(kind), "` (found `", curToken.toString(), "`)"}, curToken.pos);
      
      advance();

      return prevToken;
    }

    private: template<typename T> inline bool arrayContains(std::vector<T> array, T elem)
    {
      for (const auto& e : array)
        if (e == elem)
          return true;

      return false;
    }

    private: Node expectBinaryOrTerm(std::function<Node()> expector, std::vector<NodeKind> operators);

    private: inline Node advance()
    {
      prevToken = curToken;
      curToken  = nextToken();

      return curToken;
    }

    private: inline char curChar(uint64_t count = 0)
    {
      return expression[exprIndex + count];
    }

    private: inline Position curPos(uint64_t count = 0)
    {
      return Position(exprIndex + count, exprIndex + count + 1);
    }

    private: inline bool eofToken()
    {
      return curToken.kind == NodeKind::Eof;
    }

    private: inline bool eof(uint64_t count = 0)
    {
      return exprIndex + count >= expression.length();
    }

    private: static inline bool isWhitespace(char c)
    {
      return c == ' ' || c == '\t' || c == '\n';
    }

    private: static inline bool isAlpha(char c)
    {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    private: static inline bool isNumChar(char c, bool isFirstChar)
    {
      // allows to match also dots when the char is not the first of the number
      return (!isFirstChar && c == '.') || (c >= '0' && c <= '9');
    }

    private: static inline bool isIdentifierChar(char c, bool isFirstChar)
    {
      // returns true when matches a character like ('a'|'A')..('z'|'Z')
      // allows to match also numbers and underscores when the char is not the first of the id
      return (!isFirstChar && (c == '_' || isNumChar(c, true))) || isAlpha(c);
    }

    private: inline void eatWhitespaces()
    {
      while (!eof() && isWhitespace(curChar()))
        exprIndex++;
    }

    private: inline Node getCurAndAdvance()
    {
      advance();

      return prevToken;
    }
    
    private: inline uint64_t countOccurrences(std::string s, char toCheck)
    {
      uint64_t t = 0;

      for (const auto& c : s)
        t += !!(c == toCheck);
      
      return t;
    }

    private: inline char escapeChar(char c, Position pos)
    {
      switch (c)
      {
        case '\\': return '\\';
        case '\'': return '\'';
        case 'v':  return '\v';
        case 'n':  return '\n';
        case 't':  return '\t';
        case '0':  return '\0';
        default:   throw Error({"unknown escaped char `\\", std::string(1, c), "`"}, pos);
      }
    }

    private: static inline std::string escapedToEscapeOrNothing(char c)
    {
      switch (c)
      {
        case '\\': return "\\\\";
        case '\'': return "\\'";
        case '\v': return "\\v";
        case '\n': return "\\n";
        case '\t': return "\\t";
        case '\0': return "\\0";
        default:   return std::string(1, c);
      }
    }

    public: inline static std::string escapedToEscapes(std::string s)
    {
      std::string t;

      for (const auto& c : s)
        t.append(escapedToEscapeOrNothing(c));
      
      return t;
    }

    private: Node collectCallNode(Node name);

    private: Node expectTerm();

    private: std::string collectSequence(std::function<bool()> checker);

    private: Node collectIdentifierToken();

    private: Node convertToKeywordWhenPossible(Node token);

    private: Node collectNumToken();

    private: std::string escapesToEscaped(std::string s, Position pos);
    
    private: Node collectStringToken();

    private: Node nextToken();

    private: Node collectAssignNode(Node name);
  };

  class Evaluator
  {
    private: std::vector<KeyPair<std::string, Node>> map;

    public: Evaluator()
    {
      this->map = std::vector<KeyPair<std::string, Node>>();
    }

    public: Node evaluateNode(Node node);

    private: Node evaluateIdentifier(Node identifier);

    private: Node evaluateBin(BinNode bin);

    private: float64 evaluateOperationNum(NodeKind op, float64 l, float64 r, Position rPos);

    private: cstring_t evaluateOperationStr(Node op, cstring_t l, cstring_t r);

    private: Node evaluateUna(UnaNode una);

    private: Node evaluateAssign(AssignNode assign, Position pos);

    private: Node evaluateCall(CallNode call, Position pos);

    private: Node evaluateCallProcess(CallNode call, Position pos);

    private: Node builtinPrint(CallNode call, Position pos);

    private: Node builtinFloor(CallNode call);

    private: void expectArgsCount(CallNode call, uint64_t count);

    private: Node expectType(Node node, NodeKind type, Position pos);
  };
}