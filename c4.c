// c4.c - C in four functions

// char, int, and pointer types
// if, while, return, and expression statements
// just enough features to allow self-compilation and a bit more

// Written by Robert Swierczek

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>

#define intll long long

char *p, *lp, // current position in source code
     *data;   // data/bss pointer

intll *e, *le,  // current position in emitted code
    *id,      // currently parsed identifier
    *sym,     // symbol table (simple list of identifiers)
    g_token,       // current token
    ival,     // current token value
    ty,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    debug;    // print executed instructions

// tokens and classes (operators last and in precedence order)
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// opcodes
enum { LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT };

// types
enum { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

void parseNextToken()
{
  char *pp;

  while ((g_token = *p)) {
    ++p;
    if (g_token == '\n') {
      if (src) {
        printf("%lld: %.*s", line, p - lp, lp);
        lp = p;
        while (le < e) {
          printf("%8.4s", &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
                           "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                           "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[*++le * 5]);
          if (*le <= ADJ) printf(" %lld\n", *++le); else printf("\n");
        }
      }
      ++line;
    }
    else if (g_token == '#') {
      while (*p != 0 && *p != '\n') ++p;
    }
    else if ((g_token >= 'a' && g_token <= 'z') || (g_token >= 'A' && g_token <= 'Z') || g_token == '_') {
      pp = p - 1;
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        g_token = g_token * 147 + *p++;
      g_token = (g_token << 6) + (p - pp);
      id = sym;
      while (id[Tk]) {
        if (g_token == id[Hash] && !memcmp((char *)id[Name], pp, p - pp)) { g_token = id[Tk]; return; }
        id = id + Idsz;
      }
      id[Name] = (intll)pp;
      id[Hash] = g_token;
      g_token = id[Tk] = Id;
      return;
    }
    else if (g_token >= '0' && g_token <= '9') {
      if ((ival = g_token - '0')) {
          while (*p >= '0' && *p <= '9') ival = ival * 10 + *p++ - '0';
      }
      else if (*p == 'x' || *p == 'X') {
        while ((g_token = *++p) && ((g_token >= '0' && g_token <= '9') || (g_token >= 'a' && g_token <= 'f') || (g_token >= 'A' && g_token <= 'F')))
          ival = ival * 16 + (g_token & 15) + (g_token >= 'A' ? 9 : 0);
      }
      else { while (*p >= '0' && *p <= '7') ival = ival * 8 + *p++ - '0'; }
      g_token = Num;
      return;
    }
    else if (g_token == '/') {
      if (*p == '/') {
        ++p;
        while (*p != 0 && *p != '\n') ++p;
      }
      else {
        g_token = Div;
        return;
      }
    }
    else if (g_token == '\'' || g_token == '"') {
      pp = data;
      while (*p != 0 && *p != g_token) {
        if ((ival = *p++) == '\\') {
          if ((ival = *p++) == 'n') ival = '\n';
        }
        if (g_token == '"') *data++ = ival;
      }
      ++p;
      if (g_token == '"') ival = (intll)pp; else g_token = Num;
      return;
    }
    else if (g_token == '=') { if (*p == '=') { ++p; g_token = Eq; } else g_token = Assign; return; }
    else if (g_token == '+') { if (*p == '+') { ++p; g_token = Inc; } else g_token = Add; return; }
    else if (g_token == '-') { if (*p == '-') { ++p; g_token = Dec; } else g_token = Sub; return; }
    else if (g_token == '!') { if (*p == '=') { ++p; g_token = Ne; } return; }
    else if (g_token == '<') { if (*p == '=') { ++p; g_token = Le; } else if (*p == '<') { ++p; g_token = Shl; } else g_token = Lt; return; }
    else if (g_token == '>') { if (*p == '=') { ++p; g_token = Ge; } else if (*p == '>') { ++p; g_token = Shr; } else g_token = Gt; return; }
    else if (g_token == '|') { if (*p == '|') { ++p; g_token = Lor; } else g_token = Or; return; }
    else if (g_token == '&') { if (*p == '&') { ++p; g_token = Lan; } else g_token = And; return; }
    else if (g_token == '^') { g_token = Xor; return; }
    else if (g_token == '%') { g_token = Mod; return; }
    else if (g_token == '*') { g_token = Mul; return; }
    else if (g_token == '[') { g_token = Brak; return; }
    else if (g_token == '?') { g_token = Cond; return; }
    else if (g_token == '~' || g_token == ';' || g_token == '{' || g_token == '}' || g_token == '(' || g_token == ')' || g_token == ']' || g_token == ',' || g_token == ':') return;
  }
}

void expr(intll lev)
{
  intll t, *d;
  switch (g_token) {
      case 0: { printf("%lld: unexpected eof in expression\n", line); exit(-1); }
      case Num: { *++e = IMM; *++e = ival; parseNextToken(); ty = INT; break; }
      case '"': { *++e = IMM; *++e = ival; parseNextToken();
                    while (g_token == '"') parseNextToken();
                    data = (char *)((intll)data + sizeof(intll) & -sizeof(intll)); ty = PTR;
                    break;
                }
      case Sizeof: {
                     parseNextToken();
                     if (g_token == '(') parseNextToken();
                     else { printf("%lld: open paren expected in sizeof\n", line); exit(-1); }
                     ty = INT;
                     if (g_token == Int) parseNextToken();
                     else if (g_token == Char) { parseNextToken(); ty = CHAR; }

                     while (g_token == Mul) { parseNextToken(); ty = ty + PTR; }

                     if (g_token == ')') parseNextToken();
                     else { printf("%lld: close paren expected in sizeof\n", line); exit(-1); }

                     *++e = IMM; *++e = (ty == CHAR) ? sizeof(char) : sizeof(intll);
                     ty = INT;
                     break;
                 }
    case Id: {
                 d = id;
                 parseNextToken();
                 if (g_token == '(') {
                     parseNextToken();
                     t = 0;
                     while (g_token != ')') {
                         expr(Assign);
                         *++e = PSH; ++t;
                         if (g_token == ',') parseNextToken();
                     }
                     parseNextToken();
                     if (d[Class] == Sys) *++e = d[Val];
                     else if (d[Class] == Fun) { *++e = JSR; *++e = d[Val]; }
                     else { printf("%lld: bad function call\n", line); exit(-1); }
                     if (t) { *++e = ADJ; *++e = t; }
                     ty = d[Type];
                 }
                 else if (d[Class] == Num) { *++e = IMM; *++e = d[Val]; ty = INT; }
                 else {
                     if (d[Class] == Loc) { *++e = LEA; *++e = loc - d[Val]; }
                     else if (d[Class] == Glo) { *++e = IMM; *++e = d[Val]; }
                     else { printf("%lld: undefined variable\n", line); exit(-1); }
                     *++e = ((ty = d[Type]) == CHAR) ? LC : LI;
                 }
                 break;
             }
    case '(': {
                  parseNextToken();
                  if (g_token == Int || g_token == Char) {
                      t = (g_token == Int) ? INT : CHAR; parseNextToken();
                      while (g_token == Mul) { parseNextToken(); t = t + PTR; }
                      if (g_token == ')') parseNextToken();
                      else { printf("%lld: bad cast\n", line); exit(-1); }
                      expr(Inc);
                      ty = t;
                  }
                  else {
                      expr(Assign);
                      if (g_token == ')') parseNextToken();
                      else { printf("%lld: close paren expected\n", line); exit(-1); }
                  }
                  break;
              }
    case Mul: {
                  parseNextToken(); expr(Inc);
                  if (ty > INT) ty = ty - PTR;
                  else { printf("%lld: bad dereference\n", line); exit(-1); }
                  *++e = (ty == CHAR) ? LC : LI;
                  break;
              }
    case And: {
                  parseNextToken(); expr(Inc);
                  if (*e == LC || *e == LI) --e;
                  else { printf("%lld: bad address-of\n", line); exit(-1); }
                  ty = ty + PTR;
                  break;
              }
    case '!': { parseNextToken(); expr(Inc); *++e = PSH; *++e = IMM; *++e = 0; *++e = EQ; ty = INT; break; }
    case '~': { parseNextToken(); expr(Inc); *++e = PSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; break; }
    case Add: { parseNextToken(); expr(Inc); ty = INT; break; }
    case Sub: {
                  parseNextToken(); *++e = IMM;
                  if (g_token == Num) { *++e = -ival; parseNextToken(); }
                  else { *++e = -1; *++e = PSH; expr(Inc); *++e = MUL; }
                  ty = INT;
                  break;
              }
    case Inc:
    case Dec: {
                  t = g_token; parseNextToken(); expr(Inc);
                  switch (*e) {
                      case LC: { *e = PSH; *++e = LC; break; }
                      case LI: { *e = PSH; *++e = LI; break; }
                      default: { printf("%lld: bad lvalue in pre-increment\n", line); exit(-1); }
                  }
                  *++e = PSH;
                  *++e = IMM;
                  *++e = (ty > PTR) ? sizeof(intll) : sizeof(char);
                  *++e = (t == Inc) ? ADD : SUB;
                  *++e = (ty == CHAR) ? SC : SI;
                  break;
              }
    default: { printf("%lld: bad expression\n", line); exit(-1); }
}

  while (g_token >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    t = ty;
    switch (g_token) {
        case Assign: {
                         parseNextToken();
                         if (*e == LC || *e == LI) *e = PSH;
                         else { printf("%lld: bad lvalue in assignment\n", line); exit(-1); }
                         expr(Assign); *++e = ((ty = t) == CHAR) ? SC : SI;
                         break;
                     }
        case Cond: {
                       parseNextToken();
                       *++e = BZ; d = ++e;
                       expr(Assign);
                       if (g_token == ':') parseNextToken();
                       else { printf("%lld: conditional missing colon\n", line); exit(-1); }
                       *d = (int)(e + 3); *++e = JMP; d = ++e;
                       expr(Cond);
                       *d = (int)(e + 1);
                       break;
                   }
        case Lor: { parseNextToken(); *++e = BNZ; d = ++e; expr(Lan); *d = (intll)(e + 1); ty = INT;break; }
        case Lan: { parseNextToken(); *++e = BZ;  d = ++e; expr(Or);  *d = (intll)(e + 1); ty = INT;break; }
        case Or:  { parseNextToken(); *++e = PSH; expr(Xor); *++e = OR;  ty = INT;break; }
        case Xor: { parseNextToken(); *++e = PSH; expr(And); *++e = XOR; ty = INT;break; }
        case And: { parseNextToken(); *++e = PSH; expr(Eq);  *++e = AND; ty = INT;break; }
        case Eq:  { parseNextToken(); *++e = PSH; expr(Lt);  *++e = EQ;  ty = INT;break; }
        case Ne:  { parseNextToken(); *++e = PSH; expr(Lt);  *++e = NE;  ty = INT;break; }
        case Lt:  { parseNextToken(); *++e = PSH; expr(Shl); *++e = LT;  ty = INT;break; }
        case Gt:  { parseNextToken(); *++e = PSH; expr(Shl); *++e = GT;  ty = INT;break; }
        case Le:  { parseNextToken(); *++e = PSH; expr(Shl); *++e = LE;  ty = INT;break; }
        case Ge:  { parseNextToken(); *++e = PSH; expr(Shl); *++e = GE;  ty = INT;break; }
        case Shl: { parseNextToken(); *++e = PSH; expr(Add); *++e = SHL; ty = INT;break; }
        case Shr: { parseNextToken(); *++e = PSH; expr(Add); *++e = SHR; ty = INT;break; }
        case Add: { parseNextToken(); *++e = PSH; expr(Mul);
                      if ((ty = t) > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(intll); *++e = MUL;  }
                      *++e = ADD;
                      break;
                  }
        case Sub: { parseNextToken(); *++e = PSH; expr(Mul);
                      if (t > PTR && t == ty) { *++e = SUB; *++e = PSH; *++e = IMM; *++e = sizeof(intll); *++e = DIV; ty = INT; }
                      else if ((ty = t) > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(intll); *++e = MUL; *++e = SUB; }
                      else *++e = SUB;
                      break;
                  }
        case Mul: { parseNextToken(); *++e = PSH; expr(Inc); *++e = MUL; ty = INT; break; }
        case Div: { parseNextToken(); *++e = PSH; expr(Inc); *++e = DIV; ty = INT; break; }
        case Mod: { parseNextToken(); *++e = PSH; expr(Inc); *++e = MOD; ty = INT; break; }
        case Inc:
        case Dec: {
                      if (*e == LC) { *e = PSH; *++e = LC; }
                      else if (*e == LI) { *e = PSH; *++e = LI; }
                      else { printf("%lld: bad lvalue in post-increment\n", line); exit(-1); }
                      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(intll) : sizeof(char);
                      *++e = (g_token == Inc) ? ADD : SUB;
                      *++e = (ty == CHAR) ? SC : SI;
                      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(intll) : sizeof(char);
                      *++e = (g_token == Inc) ? SUB : ADD;
                      parseNextToken();
                      break;
                  }
        case Brak: { parseNextToken(); *++e = PSH; expr(Assign);
                       if (g_token == ']') parseNextToken();
                       else { printf("%lld: close bracket expected\n", line); exit(-1); }
                       if (t > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(intll); *++e = MUL;  }
                       else if (t < PTR) { printf("%lld: pointer type expected\n", line); exit(-1); }
                       *++e = ADD;
                       *++e = ((ty = t - PTR) == CHAR) ? LC : LI;
                       break;
                   }
        default: { printf("%lld: compiler error tk=%lld\n", line, g_token); exit(-1); }
    }
  }
}

// 语句
void stmt()
{
  intll *a, *b;
  switch (g_token) {
      case If: {
                   parseNextToken();
                   if (g_token == '(') parseNextToken();
                   else { printf("%lld: open paren expected\n", line); exit(-1); }
                   expr(Assign);

                   if (g_token == ')') parseNextToken();
                   else { printf("%lld: close paren expected\n", line); exit(-1); }

                   *++e = BZ; b = ++e;
                   stmt();
                   if (g_token == Else) {
                       *b = (intll)(e + 3); *++e = JMP; b = ++e;
                       parseNextToken();
                       stmt();
                   }
                   *b = (intll)(e + 1);
                   break;
               }
      case While: { parseNextToken();
                      a = e + 1;
                      if (g_token == '(') parseNextToken();
                      else { printf("%lld: open paren expected\n", line); exit(-1); }
                      expr(Assign);
                      if (g_token == ')') parseNextToken();
                      else { printf("%lld: close paren expected\n", line); exit(-1); }
                      *++e = BZ; b = ++e;
                      stmt();
                      *++e = JMP; *++e = (intll)a;
                      *b = (intll)(e + 1);
                      break;
                  }
      case Return: { parseNextToken();
                       if (g_token != ';') expr(Assign);
                       *++e = LEV;
                       if (g_token == ';') parseNextToken();
                       else { printf("%lld: semicolon expected\n", line); exit(-1); }
                       break;
                   }
      case '{': { parseNextToken();
                    while (g_token != '}') stmt();
                    parseNextToken();
                    break;
                }
      case ';': { parseNextToken(); break; }
      default: {
                   expr(Assign);
                   if (g_token == ';') parseNextToken();
                   else { printf("%lld: semicolon expected\n", line); exit(-1); }
                   break;
               }
  }
}

//
int main(int argc, char** argv){
  intll fd, bt, ty, poolsz, *idmain;
  intll *pc, *sp, *bp, a, cycle; // vm registers
  intll i, *t; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256*1024; // arbitrary size
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%lld) symbol area\n", poolsz); return -1; }
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%lld) text area\n", poolsz); return -1; }
  if (!(data = malloc(poolsz))) { printf("could not malloc(%lld) data area\n", poolsz); return -1; }
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%lld) stack area\n", poolsz); return -1; }

  memset(sym,  0, poolsz);
  memset(e,    0, poolsz);
  memset(data, 0, poolsz);

  p = "char else enum if int return sizeof while "
      "open read close printf malloc free memset memcmp exit void main";
  i = Char; while (i <= While) { parseNextToken(); id[Tk] = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { parseNextToken(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table
  parseNextToken(); id[Tk] = Char; // handle void type
  parseNextToken(); idmain = id; // keep track of main

  if (!(lp = p = malloc(poolsz))) { 
      printf("could not malloc(%lld) source area\n", poolsz); 
      return -1; 
  }
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %lld\n", i); return -1; }
  p[i] = 0;
  close(fd);

  // parse declarations
  line = 1;
  parseNextToken();
  while (g_token) {
    bt = INT; // basetype
              //
    switch (g_token) {
        case Int: parseNextToken(); break;
        case Char: { parseNextToken(); bt = CHAR; break; }
        case Enum: { parseNextToken();
                       if (g_token != '{') parseNextToken();
                       if (g_token == '{') {
                           parseNextToken();
                           i = 0;
                           while (g_token != '}') {
                               if (g_token != Id) {
                                   printf("%lld: bad enum identifier %lld\n", line, g_token);
                                   return -1;
                               }
                               parseNextToken();
                               if (g_token == Assign) {
                                   parseNextToken();
                                   if (g_token != Num) { printf("%lld: bad enum initializer\n", line); return -1; }
                                   i = ival;
                                   parseNextToken();
                               }
                               id[Class] = Num; id[Type] = INT; id[Val] = i++;
                               if (g_token == ',') parseNextToken();
                           }
                           parseNextToken();
                       }
                       break;
                   }
        default: break;
    }
    while (g_token != ';' && g_token != '}') {
      ty = bt;
      while (g_token == Mul) { parseNextToken(); ty = ty + PTR; }
      if (g_token != Id) { printf("%lld: bad global declaration\n", line); return -1; }
      if (id[Class]) { printf("%lld: duplicate global definition\n", line); return -1; }
      parseNextToken();
      id[Type] = ty;
      if (g_token == '(') { // function
        id[Class] = Fun;
        id[Val] = (intll)(e + 1);
        parseNextToken(); i = 0;
        while (g_token != ')') {
          ty = INT;
          if (g_token == Int) parseNextToken();
          else if (g_token == Char) { parseNextToken(); ty = CHAR; }
          while (g_token == Mul) { parseNextToken(); ty = ty + PTR; }
          if (g_token != Id) { printf("%lld: bad parameter declaration\n", line); return -1; }
          if (id[Class] == Loc) { printf("%lld: duplicate parameter definition\n", line); return -1; }
          id[HClass] = id[Class]; id[Class] = Loc;
          id[HType]  = id[Type];  id[Type] = ty;
          id[HVal]   = id[Val];   id[Val] = i++;
          parseNextToken();
          if (g_token == ',') parseNextToken();
        }
        parseNextToken();
        if (g_token != '{') { printf("%lld: bad function definition\n", line); return -1; }
        loc = ++i;
        parseNextToken();
        while (g_token == Int || g_token == Char) {
          bt = (g_token == Int) ? INT : CHAR;
          parseNextToken();
          while (g_token != ';') {
            ty = bt;
            while (g_token == Mul) { parseNextToken(); ty = ty + PTR; }
            if (g_token != Id) { printf("%lld: bad local declaration\n", line); return -1; }
            if (id[Class] == Loc) { printf("%lld: duplicate local definition\n", line); return -1; }
            id[HClass] = id[Class]; id[Class] = Loc;
            id[HType]  = id[Type];  id[Type] = ty;
            id[HVal]   = id[Val];   id[Val] = ++i;
            parseNextToken();
            if (g_token == ',') parseNextToken();
          }
          parseNextToken();
        }
        *++e = ENT; *++e = i - loc;
        while (g_token != '}') stmt();
        *++e = LEV;
        id = sym; // unwind symbol table locals
        while (id[Tk]) {
          if (id[Class] == Loc) {
            id[Class] = id[HClass];
            id[Type] = id[HType];
            id[Val] = id[HVal];
          }
          id = id + Idsz;
        }
      }
      else {
        id[Class] = Glo;
        id[Val] = (intll)data;
        data = data + sizeof(int);
      }
      if (g_token == ',') parseNextToken();
    }
    parseNextToken();
  }

  if (!(pc = (intll *)idmain[Val])) { printf("main() not defined\n"); return -1; }
  if (src) return 0;

  // setup stack
  bp = sp = (intll *)((intll)sp + poolsz);
  *--sp = EXIT; // call exit if main returns
  *--sp = PSH; t = sp;
  *--sp = argc;
  *--sp = (intll)argv;
  *--sp = (intll)t;

  // run...
  cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (debug) {
      printf("%lld> %.4s", cycle,
        &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
         "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
         "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[i * 5]);
      if (i <= ADJ) printf(" %lld\n", *pc); else printf("\n");
    }
    switch (i) {
        case LEA: a = (intll)(bp + *pc++); break;                             // load local address
        case IMM: a = *pc++; break;
        case  JMP: pc = (intll *)*pc; break;                                   // jump
        case  JSR: { *--sp = (intll)(pc + 1); pc = (intll *)*pc; break; }        // jump to subroutine
        case  BZ:  pc = a ? pc + 1 : (intll *)*pc; break;                     // branch if zero
        case  BNZ: pc = a ? (intll *)*pc : pc + 1;  break;                    // branch if not zero
        case  ENT: { *--sp = (intll)bp; bp = sp; sp = sp - *pc++; break; }     // enter subroutine
        case  ADJ: sp = sp + *pc++; break;               // stack adjust
        case  LEV: { sp = bp; bp = (intll *)*sp++; pc = (intll *)*sp++; break;} // leave subroutine
        case  LI:  a = *(intll *)a;   break;                                  // load int
        case  LC:  a = *(char *)a;    break;                                // load char
        case  SI:  *(intll *)*sp++ = a;  break;                               // store int
        case  SC:  a = *(char *)*sp++ = a;    break;                        // store char
        case  PSH: *--sp = a;           break;                              // push
        case  OR:  a = *sp++ |  a;break;
        case  XOR: a = *sp++ ^  a;break;
        case  AND: a = *sp++ &  a;break;
        case  EQ:  a = *sp++ == a;break;
        case  NE:  a = *sp++ != a;break;
        case  LT:  a = *sp++ <  a;break;
        case  GT:  a = *sp++ >  a;break;
        case  LE:  a = *sp++ <= a;break;
        case  GE:  a = *sp++ >= a;break;
        case  SHL: a = *sp++ << a;break;
        case  SHR: a = *sp++ >> a;break;
        case  ADD: a = *sp++ +  a;break;
        case  SUB: a = *sp++ -  a;break;
        case  MUL: a = *sp++ *  a;break;
        case  DIV: a = *sp++ /  a;break;
        case  MOD: a = *sp++ %  a;break;
        case  OPEN: a = open((char *)sp[1], *sp);break;
        case  READ: a = read(sp[2], (char *)sp[1], *sp);break;
        case  CLOS: a = close(*sp);break;
        case  PRTF: { t = sp + pc[1]; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); break;}
        case  MALC: a = (intll)malloc(*sp);break;
        case  FREE: free((void *)*sp);break;
        case  MSET: a = (int)memset((char *)sp[2], sp[1], *sp);break;
        case  MCMP: a = memcmp((char *)sp[2], (char *)sp[1], *sp); break;
        case  EXIT: { printf("exit(%lld) cycle = %lld\n", *sp, cycle); return *sp; }
        default: { printf("unknown instruction = %lld! cycle = %lld\n", i, cycle); return -1; }
    }
  }
}
