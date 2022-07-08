#pragma once

// Compiling for ARM9

#undef ARM7
#undef ARM9

#define ARM9

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
  };

  enum class NodeKind
  {
    ConstNum,
    ConstString,
    Identifier,
    Plus  = '+',
    Minus = '-',
    Star  = '*',
    Slash = '/',
    Bin,
    Bad,
    Eof,
  };

  class BinNode;
  
  union NodeValue
  {
    public: float64     num;
    public: cstring_t   str;
    public: BinNode*    bin;
    public: void_t      bad;
  };

  class Node
  {
    public: NodeKind  kind;
    public: NodeValue value;
    public: Position  pos;

    public: Node()
    {
      *this = Node(NodeKind::Bad);
    }

    public: Node(NodeKind kind, NodeValue value, Position pos)
    {
      this->kind  = kind;
      this->value = value;
      this->pos   = pos;
    }

    public: Node(NodeKind kind)
    {
      *this = Node(kind, (NodeValue) { .bad = 0 }, Position());
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

    public: Node parse()
    {
      // fetching the first token
      advance();

      // expression     = sub_expression +|- sub_expression ...
      // sub_expression = term           *|/ term           ...
      // term           = id|num|str
      return expectExpression();
    }

    private: Node expectExpression()
    {
      return expectBinaryOrTerm([this] {
        return expectBinaryOrTerm([this] {
          return expectTerm();
        }, { NodeKind::Star, NodeKind::Slash });
      }, { NodeKind::Plus, NodeKind::Minus });
    }

    private: Node expectTokenAndAdvance(NodeKind kind)
    {
      if (curToken.kind != kind)
        panic("parser error: expected another token");
      
      advance();

      return prevToken;
    }

    private: Node expectTerm()
    {
      return expectTokenAndAdvance(NodeKind::Identifier);
    }

    private: template<typename T> bool arrayContains(std::vector<T> array, T elem)
    {
      for (const auto& e : array)
        if (e == elem)
          return true;

      return false;
    }

    private: Node expectBinaryOrTerm(std::function<Node()> expector, std::initializer_list<NodeKind> operators)
    {
      auto left = expector();

      while (!eof() && arrayContains(std::vector<NodeKind>(operators), curToken.kind))
      {
        auto op = getCurAndAdvance();
        auto right = expector();

        left = Node(NodeKind::Bin, (NodeValue) { .bin = new BinNode(left, right, op) }, Position(left.pos.startPos, right.pos.endPos));
      }

      return left;
    }

    private: Node advance()
    {
      prevToken = curToken;
      curToken  = nextToken();

      return curToken;
    }

    private: inline char curChar()
    {
      return expression[exprIndex];
    }

    private: inline Position curPos()
    {
      return Position(exprIndex, exprIndex + 1);
    }

    private: inline bool eof()
    {
      return exprIndex >= expression.length();
    }

    private: static inline bool isWhitespace(char c)
    {
      return c == ' ' || c == '\t' || c == '\n';
    }

    private: static inline bool isAlpha(char c)
    {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    private: static inline bool isNum(char c)
    {
      return c >= '0' && c <= '9';
    }

    private: static inline bool isIdentifierChar(char c, bool isFirstChar)
    {
      return (!isFirstChar && (c == '_' || isNum(c))) || isAlpha(c);
    }

    private: void eatWhitespaces()
    {
      while (!eof() && isWhitespace(curChar()))
        exprIndex++;
    }

    private: Node getCurAndAdvance()
    {
      advance();

      return prevToken;
    }
    
    private: std::string collectSequence(std::function<bool()> checker)
    {
      auto r = std::string();

      while (checker())
      {
        r.push_back(curChar());
        exprIndex++;
      }

      // going back to the last char of sequence
      exprIndex--;

      return r;
    }

    private: Node collectIdentifierToken()
    {
      auto startPos = exprIndex;
      auto value    = (NodeValue) {
        .str = cstringRealloc(collectSequence([this] {
          return isIdentifierChar(curChar(), false);
        }).c_str())
      };

      return Node(NodeKind::Identifier, value, Position(startPos, exprIndex));
    }

    private: Node convertToKeywordWhenPossible(Node token)
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

    private: Node collectNumToken()
    {
      auto startPos = exprIndex;
      auto value    = (NodeValue) {
        .num = atof(cstringRealloc(collectSequence([this] {
          return isNum(curChar());
        }).c_str()))
      };

      return Node(NodeKind::Identifier, value, Position(startPos, exprIndex));
    }

    private: Node nextToken()
    {
      // eating all the whitespaces (they have no meaning)
      eatWhitespaces();

      if (eof())
        return Node(NodeKind::Eof);
      
      auto c = curChar();
      auto t = Node();

      if (isIdentifierChar(c, true))
        t = collectIdentifierToken();
      else if (isNum(c))
        t = collectNumToken();
      else if (c == '+')
        t = Node(NodeKind::Plus, (NodeValue) { .str = "+" }, curPos());
      else if (c == '-')
        t = Node(NodeKind::Minus, (NodeValue) { .str = "-" }, curPos());
      else if (c == '*')
        t = Node(NodeKind::Star, (NodeValue) { .str = "*" }, curPos());
      else if (c == '/')
        t = Node(NodeKind::Slash, (NodeValue) { .str = "/" }, curPos());

      exprIndex++;
      return t;
    }
  };
}