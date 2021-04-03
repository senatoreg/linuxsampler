/*
 * Copyright (c) 2014-2020 Christian Schoenebeck
 *
 * http://www.linuxsampler.org
 *
 * This file is part of LinuxSampler and released under the same terms.
 * See README file for details.
 */

// types shared between auto generated lexer and parser ...

#ifndef LS_INSTRSCRIPTSPARSER_SHARED_H
#define LS_INSTRSCRIPTSPARSER_SHARED_H

#include <stdio.h>
#include "tree.h"

#if AC_APPLE_UNIVERSAL_BUILD
# include "parser.tab.h"
#else
# include "parser.h"
#endif

#include "../common/global_private.h"
    
struct _YYSTYPE {
    union {
        LinuxSampler::vmint iValue;
        LinuxSampler::vmfloat fValue;
        // Intentionally using C-strings instead of std::string for parser's
        // string tokens, because any destructor based class had a negative
        // impact on parser performance in benchmarks. Reason for this is that
        // text tokens are the most common one while parsing, plus the parser
        // copies the YYSTYPE struct a lot (i.e. when shifting). For C-strings
        // coming directly from Flex, we don't have to free them. For C-strings
        // allocated by us, use yyextra->autoFreeAfterParse(s);
        char* sValue;
        struct {
            LinuxSampler::vmint iValue;
            LinuxSampler::MetricPrefix_t prefix[2];
            LinuxSampler::StdUnit_t unit;
        } iUnitValue;
        struct {
            LinuxSampler::vmfloat fValue;
            LinuxSampler::MetricPrefix_t prefix[2];
            LinuxSampler::StdUnit_t unit;
        } fUnitValue;
    };
    LinuxSampler::EventHandlersRef nEventHandlers;
    LinuxSampler::EventHandlerRef nEventHandler;
    LinuxSampler::StatementsRef nStatements;
    LinuxSampler::StatementRef nStatement;
    LinuxSampler::FunctionCallRef nFunctionCall;
    LinuxSampler::ArgsRef nArgs;
    LinuxSampler::ExpressionRef nExpression;
    LinuxSampler::CaseBranch nCaseBranch;
    LinuxSampler::CaseBranches nCaseBranches;
    LinuxSampler::Qualifier_t varQualifier;
};
#define YYSTYPE _YYSTYPE
#define yystype YYSTYPE     ///< For backward compatibility.
#ifndef YYSTYPE_IS_DECLARED
# define YYSTYPE_IS_DECLARED ///< We tell the lexer / parser that we use our own data structure as defined above.
#endif

// custom Bison location type to support raw byte positions
struct _YYLTYPE {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    int first_byte;
    int length_bytes;
};
#define YYLTYPE _YYLTYPE
#define YYLTYPE_IS_DECLARED 1

// override Bison's default location passing to support raw byte positions
#define YYLLOC_DEFAULT(Cur, Rhs, N)                         \
do                                                          \
  if (N)                                                    \
    {                                                       \
      (Cur).first_line   = YYRHSLOC(Rhs, 1).first_line;     \
      (Cur).first_column = YYRHSLOC(Rhs, 1).first_column;   \
      (Cur).last_line    = YYRHSLOC(Rhs, N).last_line;      \
      (Cur).last_column  = YYRHSLOC(Rhs, N).last_column;    \
      (Cur).first_byte   = YYRHSLOC(Rhs, 1).first_byte;     \
      (Cur).length_bytes = (YYRHSLOC(Rhs, N).first_byte  -  \
                            YYRHSLOC(Rhs, 1).first_byte) +  \
                            YYRHSLOC(Rhs, N).length_bytes;  \
    }                                                       \
  else                                                      \
    {                                                       \
      (Cur).first_line   = (Cur).last_line   =              \
        YYRHSLOC(Rhs, 0).last_line;                         \
      (Cur).first_column = (Cur).last_column =              \
        YYRHSLOC(Rhs, 0).last_column;                       \
      (Cur).first_byte   = YYRHSLOC(Rhs, 0).first_byte;     \
      (Cur).length_bytes = YYRHSLOC(Rhs, 0).length_bytes;   \
    }                                                       \
while (0)

// Force YYCOPY() to use copy by value.
//
// By default YYCOPY() is using __builtin_memcpy, which is slightly problematic
// with our YYSTYPE (see above) since it has dynamic objects as member variables
// and hence __builtin_memcpy would overwrite their vpointer. In practice though
// this is more of a theoretical fix and probably just silences compiler
// warnings. So in practice __builtin_memcpy would probably not cause any
// misbehaviours, because it is expected that Bison generated parsers only use
// YYCOPY() to relocate the parser's stack (that is moving objects in memory),
// but not for really creating duplicates of any objects.
//
// In my benchmarks I did not encounter any measurable performance difference by
// this change, so shutting up the compiler wins for now.
#define YYCOPY(To, From, Count)                 \
    do {                                        \
        for (YYSIZE_T i = 0; i < (Count); ++i)  \
            (To)[i] = (From)[i];                \
    } while (YYID (0));                         \

#endif // LS_INSTRSCRIPTSPARSER_SHARED_H
