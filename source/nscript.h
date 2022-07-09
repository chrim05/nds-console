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
    Bad,
    Eof,
    ConstNum,
    ConstString,
    Identifier,
    Plus  = '+',
    Minus = '-',
    Star  = '*',
    Slash = '/',
    LPar  = '(',
    RPar  = ')',
    Comma = ',',
  };

  class BinNode;
  class UnaNode;
  class CallNode;
  
  union NodeValue
  {
    public: float64     num;
    public: cstring_t   str;
    public: BinNode*    bin;
    public: UnaNode*    una;
    public: CallNode*   call;
    public: void_t      bad;
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
      *this = Node(kind, (NodeValue) { .bad = 0 }, position);
    }

    public: static std::string kindToString(NodeKind kind)
    {
      switch (kind)
      {
        case NodeKind::ConstNum:    return "num";
        case NodeKind::ConstString: return "str";
        case NodeKind::Bin:         return "bin";
        case NodeKind::Una:         return "una";
        case NodeKind::Call:        return "call";
        case NodeKind::Plus:
        case NodeKind::Minus:
        case NodeKind::Star:
        case NodeKind::LPar:
        case NodeKind::RPar:
        case NodeKind::Comma:
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

  class ParserError : std::exception
  {
    public: std::vector<std::string> message;
    public: Position                 position;

    public: ParserError(std::vector<std::string> message, Position position)
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

    public: Node parse()
    {
      // fetching the first token
      advance();

      // the main expression
      auto expr = expectExpression();

      // the main expression is not alone in the prompt
      expectTokenAndAdvance(NodeKind::Eof);

      return expr;
    }

    private: Node expectExpression()
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

    private: Node expectTokenAndAdvance(NodeKind kind)
    {
      if (curToken.kind != kind)
        throw ParserError({"expected `", Node::kindToString(kind), "` (found `", curToken.toString(), "`)"}, curToken.pos);
      
      advance();

      return prevToken;
    }

    private: Node collectCallNode(Node name)
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

    private: Node expectTerm()
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

    private: template<typename T> bool arrayContains(std::vector<T> array, T elem)
    {
      for (const auto& e : array)
        if (e == elem)
          return true;

      return false;
    }

    private: Node expectBinaryOrTerm(std::function<Node()> expector, std::vector<NodeKind> operators)
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

    private: Node advance()
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

    private: Node collectIdentifierToken()
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

    private: uint64_t countOccurrences(std::string s, char toCheck)
    {
      uint64_t t = 0;

      for (const auto& c : s)
        t += !!(c == toCheck);
      
      return t;
    }

    private: Node collectNumToken()
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
        default:   throw ParserError({"unknown escaped char `\\", std::string(1, c), "`"}, pos);
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

    private: std::string escapesToEscaped(std::string s, Position pos)
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
    
    public: static std::string escapedToEscapes(std::string s)
    {
      std::string t;

      for (const auto& c : s)
        t.append(escapedToEscapeOrNothing(c));
      
      return t;
    }

    private: Node collectStringToken()
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

    private: Node nextToken()
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
  };
}