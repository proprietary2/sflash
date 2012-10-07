#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#define DELIM_QUESTION '-'
#define DELIM_ANSWER '+'

typedef struct
{
  fpos_t * pQuestionPositions;
  uint16_t iPositions;
  uint16_t iCurrentPosition;
} PositionsT;

typedef struct
{
  char * szQuestion;
  char * szAnswer;
} QuestionAnswerT;

void setup_positions(PositionsT *, uint16_t, FILE *);
static fpos_t (*get_next_position)(PositionsT *) = NULL;
fpos_t get_random_position(PositionsT *);
fpos_t get_sequential_position(PositionsT *);

uint16_t QA_load(QuestionAnswerT *, PositionsT *, uint16_t, FILE *);

void prompt_loop(PositionsT *, FILE *);
typedef struct 
{
  QuestionAnswerT * this_entry;
} EntryProcessArgsT;
typedef void (*fnEntryProcessT)(EntryProcessArgsT);
fnEntryProcessT parse_answer(QuestionAnswerT *);

typedef struct
{
  char ** list;
  uint16_t len;
} ListProcessorRetT;
ListProcessorRetT process_list(char * restrict);
void list_prompt(EntryProcessArgsT);

void regular_prompt(EntryProcessArgsT);
char ** tokenize_answer(char * restrict);
float compare_words(char **, char **);

static uint64_t ProgramOptions;
static const uint64_t ProgramOptions_Randomize = 0x01;
static const uint64_t ProgramOptions_Perpetual = 0x02;
static uint64_t ProgramOptions_LongestLine = 1024;
static uint32_t ProgramOptions_iMemoryChunk = 512;
static uint16_t ProgramOptions_iMaxWordsInAnswer = 50;
static uint16_t ProgramOptions_iMaxListItems = 50;
static uint16_t ProgramOptions_iPairsToLoadAtOnce = 10;

int main(int argc, char ** argv)
{
  char ** pargv = argv;
  ++pargv;
  
  FILE * file = NULL;
  file = fopen(*pargv, "r");
  if(!file) { puts("Invalid file"); exit(1); }

  get_next_position = &get_random_position;

  while(*pargv != NULL)
  {
    if(!strcmp(*pargv, "--randomize")
      || !strcmp(*pargv, "-r"))
    {
      ProgramOptions |= ProgramOptions_Randomize;
      get_next_position = &get_random_position;
    }
    else if(!strcmp(*pargv, "--perpetual")
      || !strcmp(*pargv, "-p"))
    {
      ProgramOptions |= ProgramOptions_Perpetual;
    }
    else if(!strcmp(*pargv, "--set-max-line-characters"))
    {
      if(!*++pargv)
      {
        puts("Error: no integer given to "
          "``--set-max-line-characters''");
        exit(1);
      }
      ProgramOptions_LongestLine = atoi(*pargv);
    }

    ++pargv;
  }

  PositionsT pos;
  setup_positions(&pos, ProgramOptions_iPairsToLoadAtOnce, file);
  prompt_loop(&pos, file);

  free(pos.pQuestionPositions);

  fclose(file);

  return 0;
}

void setup_positions
(
  PositionsT * dest,
  uint16_t iToSetUp,
  FILE * src
)
/*
  Up to the callee to free pQuestionPositions
*/
{
  fseek(src, 0, SEEK_SET);
  char * buf = malloc(ProgramOptions_iMemoryChunk);
  char * pbuf = buf;
  uint16_t iRead = 0;
  char c = 0;
  dest->iPositions = 0;
  dest->iCurrentPosition = 0;
  dest->pQuestionPositions = malloc(iToSetUp * sizeof(fpos_t));
  do
  {
    iRead = fread(buf, 1, ProgramOptions_iMemoryChunk, src);
    for(uint16_t i = 0; i < iRead; ++i, ++pbuf)
    {
      if(*pbuf == DELIM_QUESTION)
      {
        dest->pQuestionPositions[dest->iPositions++] = ftell(src) + (pbuf - buf);
      }
    }
    pbuf = buf;
  } while(iRead == ProgramOptions_iMemoryChunk);
  free(buf);
}

uint16_t QA_load
(
  QuestionAnswerT * dest,
  PositionsT * src,
  uint16_t iToLoad, //Question/Answer pairs to load
  FILE * file
)
/*
  `dest` must have `iToLoad` allocated,
  empty QuestionAnswerT objects

  Returns: pairs actually loaded
*/
{
  uint16_t iLoaded = 0;
  QuestionAnswerT * pdest = dest;
  char * szQuestion = malloc(ProgramOptions_LongestLine);
  char * szAnswer = malloc(ProgramOptions_LongestLine);
  for(uint16_t i = 0; i < iToLoad; ++i)
  {
    fseek(file, get_next_position(src), 0);
r1:
    fgets(szQuestion, ProgramOptions_LongestLine, file);
    if(!strcmp(szQuestion, ""))
      goto r1;
r2:
    fgets(szAnswer, ProgramOptions_LongestLine, file);
    if(!strcmp(szAnswer, ""))
      goto r2;
    assert(szQuestion[0] == '-');
    assert(szAnswer[0] == '+');
    pdest->szQuestion = malloc(strlen(szQuestion) + 1);
    memcpy(pdest->szQuestion, szQuestion, strlen(szQuestion) + 1);
    pdest->szAnswer = malloc(strlen(szAnswer) + 1);
    memcpy(pdest->szAnswer, szAnswer, strlen(szAnswer) + 1);
    ++pdest;
  }
  free(szQuestion);
  free(szAnswer);

  iLoaded = pdest - dest;
  return iLoaded;
}

fpos_t get_random_position
(
  PositionsT * src
)
{
  fpos_t ret = 0;
  ret = rand() % src->iPositions;
}

char ** tokenize_answer
(
  char * restrict answer
)
/*
  Returns a null-terminated array

  Callee must free the returned pointer, and
  free its elements in a stepwise fashion
*/
{
  char * const buf = malloc(strlen(answer));
  char * pbuf = buf;

  char ** ret = malloc(ProgramOptions_iMaxWordsInAnswer);
  uint16_t i = 0;

  while(*answer != 0)
  {
    switch(*answer)
    {
      case ' ': case '-':
      {
        *pbuf = 0;
        //TODO add word filter here
        assert(i < ProgramOptions_iMaxWordsInAnswer);
        ret[i] = malloc(pbuf - buf + 1);
        memcpy(ret[i], buf, pbuf - buf + 1);
        ++i;
        pbuf = buf;

        break;
      }
      case ',': case ';': case '.':
        break;
      default:
      {
        *(pbuf++) = *answer;
        break;
      }
    }
  }
  free(buf);

  assert(i < ProgramOptions_iMaxWordsInAnswer);
  ret[i] = NULL;
  return ret;
}

float compare_words
(
  char ** restrict attempted,
  char ** const correct
)
{
  float ret = 0.0f;
  uint16_t iMatchCount = 0;
  uint16_t iWordCount = 0;

  char ** pCorrect = correct;

  while(*attempted != NULL)
  {
    while(*pCorrect != NULL)
    {
      if(!strcmp(*attempted, *pCorrect))
      {
        ++iMatchCount;
      }
      ++pCorrect;
    }
    pCorrect = correct;
    ++attempted;
    ++iWordCount;
  }

  assert(pCorrect - correct == iWordCount);
  ret = iMatchCount / iWordCount;
  return ret;
}

ListProcessorRetT process_list
(
  char * restrict src
)
/*
  Returns a double pointer to memory that must be freed
  in a stepwise fashion

  The returned double pointer is null-terminated
  at both levels
*/
{
  uint8_t in_what = 0;
  static const uint8_t in_entry = 0x01;

  char * const buf = malloc(ProgramOptions_iMemoryChunk);
  char * pbuf = buf;

  char ** ret = malloc(ProgramOptions_iMaxListItems);
  uint16_t i = 0;

  for(; *src; ++src)
  {
    switch(*src)
    {
      case '{':
        break;
      case ',': case '}':
      {
        in_what -= in_entry;
        *pbuf = 0;
        assert(i < ProgramOptions_iMaxListItems);
        ret[i] = malloc(pbuf - buf + 1);
        memcpy(ret[i], buf, pbuf - buf + 1);
        ++i;
        pbuf = buf;
      }
      default:
      {
        in_what |= in_entry;
        *(pbuf++) = *src;
      }
    }
  }
  
  ret[i] = NULL;

  ListProcessorRetT r1;
  r1.list = ret;
  r1.len = i;
  return r1;
}

void prompt_loop
(
  PositionsT * pos,
  FILE * file
)
{
  QuestionAnswerT * const qas = malloc(ProgramOptions_iPairsToLoadAtOnce * sizeof(QuestionAnswerT));
  uint16_t iActuallyLoadedPairs = 0;
  fnEntryProcessT fnPrompt = NULL;
  EntryProcessArgsT args;

  do
  {
    iActuallyLoadedPairs = QA_load(qas, pos, ProgramOptions_iPairsToLoadAtOnce, file);
    for(uint16_t i = 0; i < iActuallyLoadedPairs; ++i)
    {
      fnPrompt = parse_answer(qas + i);
      args.this_entry = qas + i;
      fnPrompt(args);
    }
  } while(iActuallyLoadedPairs == ProgramOptions_iPairsToLoadAtOnce);
}

fnEntryProcessT parse_answer
(
  QuestionAnswerT * qa
)
{
  char * c = qa->szAnswer;
  fnEntryProcessT ret = NULL;
  for(; *c; ++c)
  {
    switch(*c)
    {
      case '{':
      {
        ret = list_prompt;
        goto end;
      }
      case ' ':
        break;
      default:
      {
        ret = regular_prompt;
        goto end;
      }
    }
  }
  //TODO make this a longjmp
  puts("Fatal parsing error");
  exit(1);
end:
  return ret;
}

void list_prompt
(
  EntryProcessArgsT src
)
{
  char * buf = malloc(ProgramOptions_iMemoryChunk);
  printf("Q: %s\n> [list input]\n", src.this_entry->szQuestion);
  ListProcessorRetT result = process_list(src.this_entry->szAnswer);
  char ** plist = result.list;
  for(uint16_t i = 0; i < result.len; ++i)
  {
attempt:
    puts("  -> ");
    gets(buf);
    for(plist = result.list; *plist != NULL; ++plist)
    {
      if(!strcmp(buf, *plist))
      {
        puts("Correct!");
        goto keep_going;
      }
    }
    puts("No list item found. Try again.");
    goto attempt;
keep_going:
    ;
  }
  free(buf);
  for(char ** plist = result.list; plist != NULL; ++plist)
  {
    free(*plist);
  }
  free(plist);
}

void regular_prompt
(
  EntryProcessArgsT src
)
{
  char * buf = malloc(ProgramOptions_iMemoryChunk);
  printf("Q: %s\n> ", src.this_entry->szQuestion);
  char ** RealAnswerTokens = tokenize_answer(src.this_entry->szAnswer);
  char ** GivenAnswerTokens = NULL;
  float fResults = 0.0f;

  gets(buf);
  GivenAnswerTokens = tokenize_answer(buf);
  fResults = compare_words(GivenAnswerTokens, RealAnswerTokens);
  printf("Ratio correct: %2f.\n", fResults);

  for(char ** p = RealAnswerTokens; *p != NULL; ++p)
  {
    free(*p);
  }
  free(RealAnswerTokens);

  for(char ** p = GivenAnswerTokens; *p != NULL; ++p)
  {
    free(*p);
  }
  free(GivenAnswerTokens);

  free(buf);
}
