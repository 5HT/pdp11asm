// PDP11 Assembler (c) 15-01-2015 vinxru

#pragma once
#include <string>
#include <map>
#include <list>
#include <vector>
#include <stdint.h>

enum Token { ttEof, ttEol, ttWord, ttInteger, ttOperator, ttString1, ttString2, ttComment };

class Parser {
public:
    class Config {
    public:
        const char** operators;
        const char** remark;
        bool caseSel;
        char altstring;
        bool eol;
        bool cescape;
        bool decimalnumbers; // ����� �� ��������� 10-����, ����� 8-������
    };

  typedef uint32_t num_t;
  static const size_t maxTokenText = 256;
  typedef char TokenText[maxTokenText];

  // ���������
  Config cfg;

  // ��� ������ ������
  std::string fileName; 

  // �����
  std::string source;

  // ������
  const char *firstCursor;
  const char *cursor, *prevCursor, *sigCursor;
  size_t line, prevLine, sigLine;
  size_t col, prevCol, sigCol;

  // ����������� �����
  Token token;
  TokenText tokenText;
  num_t tokenTextSize;
  num_t tokenNum;  

  // ������� �����
  TokenText loadedText;
  num_t loadedTextSize;
  num_t loadedNum;

  //  ������
  Parser();
  void init(const char* text);
  void nextToken();
  void nextToken2();

  // ��������
  struct Label {
    const char* cursor;
    size_t line, col;
    
    inline Label() { cursor=0; line=0; col=0; }
    inline Label(Parser& p) { p.getLabel(*this); }
  };
  void getLabel(Label&);
  void jump(Label&);

  // ������
  void syntaxError(const char* text = 0);

  // ���������
  inline bool ifToken(Token t) { if(token != t) return false; nextToken(); return true; }
  inline void needToken(Token t) { if(token != t) syntaxError(); nextToken(); }
  inline const char* needIdent() { needToken(ttWord); return loadedText; }
  bool ifToken(const char* text);  
  inline void needToken(const char* text) { if(!ifToken(text)) syntaxError(); }
  bool ifToken(const char** a, unsigned &n);
  inline unsigned needToken(const char** a) { unsigned n; if(!ifToken(a, n)) syntaxError(); return n; }

  template<class T> inline bool ifToken(T* a, unsigned& n) {
    for(T* i = a; i->name; i++) {
      if(ifToken(i->name)) {
        n = i - a;
        return true;
      }
    }
    return false;
  }

  //--
  struct MacroStack {
    const char* cursor, *prevCursor;
    Label pl;
    int killMacro;
    std::string fileName;
    std::string buffer;
    const char* prevFirstCursor;
    int disabledMacro;

    MacroStack() { disabledMacro=-1; }
  };

  std::list<MacroStack> macroStack;

  struct Macro {
    std::string id, body;
    bool disabled;
    std::vector<std::string> args;

    Macro() { disabled=false; }
  };

  std::list<Macro> macro;

  void enterMacro(int killMacro, std::string* buf, int disabledMacro, bool dontCallNextToken);
  void leaveMacro();
  void jump(Label& label, bool dontLoadNextToken);
};
