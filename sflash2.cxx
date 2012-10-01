/*
  SFLASH - SIMPLE FLASHCARDS PROGRAM
  Author: Z. Snyder
  License: Affero General Public License

  sflash approaches flashcards in a simple
  manner, side-stepping tricks in favor of
  the tried and true method of memorization:
  repetition.

  Philosophy: Keep it simple. Minimize bloat
  and feature creep. The primary features will
  always be related to memorization of text.

  Plans:
  + Randomization
  + Flip cards around
  + Support for TeX
  + Support for chemical structures using
  + Graphical mode with FLTK

  KNOWN BUGS
  - Last question (token mode) reads twice

*/

#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <deque>
#include <list>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

using namespace std;

class File
{
public:
  File(const char * filename, const char * mode)
  {
    pFile = NULL;
    pFile = fopen(filename, mode);
    if(!pFile)
    {
      puts("File not found error");
      exit(1);
    }
    line = NULL;
    line = (char *) calloc(longest_line, 1);
    if(!line)
      puts("w0t m8");
    line_count();
  }
  ~File()
  {
    fclose(pFile);
    delete line;
  }
  char * get_line()
  {
    char * ret = fgets(line, longest_line - 1, pFile);
    return ret;
  }
  void set_random_pos_to_line()
  {
    uint32_t iSkipTo = rand() % iLineCount;
    iSkipTo /= 2;
    iSkipTo *= 2;
    
    fseek(pFile, 0, SEEK_SET);
    char c = 0;
    uint64_t iThisLine = 0;
    while((c = fgetc(pFile)) != EOF
      && iThisLine < iSkipTo)
    {
      if(c == '\n')
        iThisLine += 1;
    }
    ungetc(c, pFile);
  }
  void reset_position()
  {
    fseek(pFile, 0, SEEK_SET);
  }
private:
  char * line;
  uint32_t iLineCount;
  static const int longest_line = 512;
  FILE * pFile;

  uint64_t line_count()
  {
    fseek(pFile, 0, SEEK_SET);
    iLineCount = 0;
    char c = 0;
    while((c = fgetc(pFile)) != EOF)
    {
      if(c == '\n')
      {
        ++iLineCount;
      }
    }
    fseek(pFile, 0, SEEK_SET);
    return iLineCount;
  }

};

struct QA
{
  string question;
  string answer;
};

class Parser
{
  friend class Prompt;
public:
  Parser(File * file_)
  {
    file = file_;
    used = false;
  }

  void split_QAs();
  uint32_t iLinesRead;
private:
  File * file;
  bool used;
  vector<QA> vQAs;
};

struct MatchResults
{
  uint16_t iMatches;
  uint16_t iTotalWords;
  float percentage()
  {
    return (float)iMatches/(float)iTotalWords;
  }
};

class Prompt;

typedef void (Prompt::*fnDecision)(QA *);

class AnswerHandler
{
  friend class Prompt;
public:
  AnswerHandler()
  {
    used = false;
  }
  void exec(const string&, fnDecision *);
private:
  bool used;
  string strAnswer;

  static void load_words(list<string>&, const string&);
  MatchResults compare_words(const list<string>&);
  list<string> lstWords;
  
  void construct_list();
  vector<string> vecListItems;

};

class Prompt
{
  friend class AnswerHandler;
public:
  Prompt(File * pFile)
    :ah(), parser(pFile)
  {
    fnWhich = NULL;
  }
  void loop();
  uint32_t lines_read;
private:
  AnswerHandler ah;
  Parser parser;
  fnDecision fnWhich;
  void tokens(QA *);
  void list(QA *);
};

namespace ProgramOptions
{
  static uint32_t options = 0x0;
  static const uint32_t randomize = 0x01;
  static const uint32_t perpetual = 0x02;

  static uint16_t kiLinesToLoad = 10;
  static float fNoRepeatThreshold = 0.50f;
}

int main(int argc, char ** argv)
{
  char ** pArgv = argv;
  ++pArgv;

  File my_file(*(pArgv++), "r");
  srand(time(NULL));
  
  while(*pArgv != NULL)
  {
    if(!strcmp(*pArgv, "--randomize") ||
      !strcmp(*pArgv, "--r"))
    {
      ProgramOptions::options |= ProgramOptions::randomize;
    }
    else if(!strcmp(*pArgv, "--perpetual") ||
      !strcmp(*pArgv, "-p"))
    {
      ProgramOptions::options |= ProgramOptions::perpetual;
    }
    else if(!strcmp(*pArgv, "--no-repeat-threshold") ||
      !strcmp(*pArgv, "-t"))
    {
      if(*++pArgv == NULL)
      {
        puts("Invalid command line arguments."
          "--no-repeat-threshold was not given an integer");
        exit(1);
      }
      ProgramOptions::fNoRepeatThreshold = (float) atoi(*pArgv) / 100;
    }
    ++pArgv;
  }

  Prompt prompt(&my_file);
  prompt.loop();

  return 0;
}

void Parser::split_QAs()
{
  string buf;
  
  uint8_t in_what = 0;
  static const uint8_t in_question = 0x01;
  static const uint8_t in_answer = 0x02;
  static const uint8_t question_full = 0x04;
  static const uint8_t answer_full = 0x08;

  if(used)
  {
    vQAs.clear();
  }
  iLinesRead = 0;

  char * test = NULL;
  string strThisLine;
  QA qaTemp;
  
  for(uint16_t i = 0; i < ProgramOptions::kiLinesToLoad; ++i)
  {
    if((test = file->get_line()) == NULL)
    {
      vQAs.push_back(qaTemp);
      break;
    }
    else
      strThisLine.assign(test, strlen(test));

    for(auto iter = begin(strThisLine);
      iter != end(strThisLine); ++iter)
    {
      switch(*iter)
      {
        case '-':
          in_what |= in_question;
          break;
        case '+':
          in_what |= in_answer;
          break;
        default:
        {
          buf.append(1, *iter);
          break;
        }
      }
    }

    if(in_what & question_full && in_what & answer_full)
    {
      vQAs.push_back(qaTemp);
      in_what ^= question_full;
      in_what ^= answer_full;
      qaTemp.question.clear();
      qaTemp.answer.clear();
    }
    if(in_what & in_question)
    {
      if(in_what & question_full)
      {
        cout << "Syntax error: unmatched question/answer pair\nAborting...\n";
        exit(1); //TODO change to exception
      }
      qaTemp.question.assign(buf);
      in_what ^= in_question;
      in_what |= question_full;
    }
    else if(in_what & in_answer)
    {
      if(in_what & answer_full)
      {
        cout << "Syntax error: unmatched question/answer pair\nAborting...\n";
        exit(1); //TODO change to exception
      }
      qaTemp.answer.assign(buf);
      in_what ^= in_answer;
      in_what |= answer_full;
    }
    buf.clear();
    iLinesRead += 1;
  }

  used = true;

  vQAs.push_back(qaTemp);
}

void AnswerHandler::load_words
(
  list<string>& lstWords,
  const string& strAnswer
)
{
  string buf;
  for(auto ch = begin(strAnswer);
    ch != end(strAnswer); ++ch)
  {
    if(*ch == ' ')
    {
      lstWords.push_back(buf);
      buf.clear();
      continue;
    }
    if(*ch == '\n')
      break;
    buf.append(1, *ch);
  }
  lstWords.push_back(buf);
}

MatchResults AnswerHandler::compare_words
(
  const list<string>& lstWordsAgainst
)
{
  MatchResults ret;
  ret.iMatches = 0;

  for(auto real = begin(lstWords);
    real != end(lstWords); ++real)
  {
    for(auto given = begin(lstWordsAgainst);
      given != end(lstWordsAgainst); ++given)
    {
      if(!real->compare(*given))
      {
        ret.iMatches += 1;
      }
    }
  }

  ret.iTotalWords = lstWords.size();

  return ret;
}

void AnswerHandler::construct_list()
{
  string buf;

  if(used)
    vecListItems.clear();

  static const uint8_t in_item = 0x01;
  uint8_t status = 0x0;

  for(auto ch = begin(strAnswer);
    ch != end(strAnswer); ++ch)
  {
    switch(*ch)
    {
    case '{':
      break;
    case ' ':
    {
      if(status & in_item)
      {
        goto add_character;
      }
      break;
    }
    case ',':
    {
commit:
      vecListItems.push_back(buf);
      buf.clear();
      status ^= in_item;

      break;
    }
    case '}':
    {
      status ^= in_item;
      goto commit;
    }
    default:
    {
      status |= in_item;
add_character:
      buf.append(1, *ch);
      break;
    }
    }
  }
  used = true;
}

void AnswerHandler::exec
(
  const string& strUserAnswer,
  fnDecision * fnWhich
)
{
  strAnswer = strUserAnswer;

  for(auto ch = begin(strAnswer);
    ch != end(strAnswer); ++ch)
  {
    if(*ch == ' ')
      continue;
    if(*ch == '{')
    {
      construct_list();
      *fnWhich = &Prompt::list;
      break;
    }
    load_words(lstWords, strAnswer);
    *fnWhich = &Prompt::tokens;
    break;
  }
}

void Prompt::loop()
{
continue_looping:
  do
  {
    if(ProgramOptions::options & ProgramOptions::randomize)
    {
      parser.file->set_random_pos_to_line();
    }
    parser.split_QAs();
    for(auto qa = begin(parser.vQAs);
      qa != end(parser.vQAs); ++qa)
    {
      ah.exec(qa->answer, &fnWhich);
      (this->*fnWhich)(&*qa);
    }
  } while(parser.iLinesRead == ProgramOptions::kiLinesToLoad);
  
  if(ProgramOptions::options & ProgramOptions::perpetual)
  {
    parser.file->reset_position();
    goto continue_looping;
  }
}

void Prompt::tokens
(
  QA * qa
)
{
  cout << "Q: " << qa->question << "\n"
    << "> ";
  string strUserAnswer;
  MatchResults res;
  std::list<string> lstUserAnswer;

attempt:
  getline(cin, strUserAnswer);
  AnswerHandler::load_words(lstUserAnswer, strUserAnswer);

  //TODO a punctuation filter
  //TODO a word filter removing "the"s

  res = ah.compare_words(lstUserAnswer);

  cout << res.iMatches << "/" << res.iTotalWords <<
    " == " << res.percentage() << "  " << qa->answer << "\n";
  if(res.percentage() < ProgramOptions::fNoRepeatThreshold)
  {
    cout << "Try again.\n> ";
    lstUserAnswer.clear();
    strUserAnswer.clear();
    goto attempt;
  }
  ah.lstWords.clear(); //becuase this function controls when we're through with the real answer words
}

void Prompt::list
(
  QA * qa
)
{
  cout << "Q: " << qa->question << "\n"
    << "> [list input]\n";
  string strUserAnswer;
  deque<vector<string>::iterator> dequePreviouslyCorrect;

  for(uint16_t successful_answers = 0;
    successful_answers < ah.vecListItems.size(); ++successful_answers)
  {
attempt:
    cout << "  -> ";
    getline(cin, strUserAnswer);
    //escape hatches: "???" and "!!!"
    if(strUserAnswer == "???")
    {
      for(auto i = begin(ah.vecListItems); i != end(ah.vecListItems);
        ++i)
      {
        cout << *i << "\n";
      }
      goto attempt;
    }
    else if(strUserAnswer == "!!!")
    {
      for(auto i = begin(ah.vecListItems); i != end(ah.vecListItems);
        ++i)
      {
        cout << *i << "\n";
      }
      goto end;
    }

    for(auto this_item_revisited = begin(ah.vecListItems);
      this_item_revisited != end(ah.vecListItems);
      ++this_item_revisited)
    {
      if(!strUserAnswer.compare(*this_item_revisited))
      {
        if(find_if(begin(dequePreviouslyCorrect),
          end(dequePreviouslyCorrect),
          [&](vector<string>::iterator it)
          {
            return it == this_item_revisited;
          }) != end(dequePreviouslyCorrect))
        {
          cout << "Already said that\n";
          goto retry;
        }
        cout << "Correct\n";
        dequePreviouslyCorrect.push_back(this_item_revisited);
        goto next_iteration;
      }
    }
    cout << "Try again\n";
retry:
    goto attempt;
next_iteration:
    ;
  }
end:
  ;
}
