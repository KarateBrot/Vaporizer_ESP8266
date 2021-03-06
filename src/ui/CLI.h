#ifndef COMMAND_LINE_INTERFACE_H
#define COMMAND_LINE_INTERFACE_H

#include "Arduino.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

#include "ascii.h"

struct Command;
typedef std::vector<Command>     CmdList;
typedef std::vector<std::string> StrList;



struct Command {

  std::string name, help;
  uint8_t     nArgs;
  
  std::function<void(void)> run;
};



class CLI {

private:
  std::string _prompt;
  CmdList     _commands;
  Print      *_pr_ptr;

  struct Buffer {
    std::string line, cmd;
    StrList     args;

    void clear(void) { line.clear(); cmd.clear(); args.clear(); }
  }
  _buffer;

  StrList _split  (std::string, const char&);
  void    _parse  (void);
  void    _execute(void);

public:
  CLI(void) {};
  CLI(const std::string&, Print*);
  CLI(const std::string&, Print*, CmdList);
  
  void begin(const std::string&, Print*);
  void begin(const std::string&, Print*, CmdList);

  void fetch(const char&);
  void help (void);
  
  CLI& add(const Command &c) { _commands.emplace_back(c); return *this; }
  
  void operator<<(const char &c) { fetch(c); }

  std::string getArg  (uint8_t)        const;
  int32_t     getArg_i(const uint8_t&) const;
  double      getArg_d(const uint8_t&) const;
  StrList     getArgs (void)           const { return _buffer.args; }
};



#endif // COMMAND_LINE_INTERFACE_H