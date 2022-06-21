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
     *g_data;   // data/bss pointer

intll *g_text, *le,  // current position in emitted code
      *g_cur_sym,      // currently parsed identifier
      *g_symtab,     // symbol table (simple list of identifiers)
      g_cur_token,       // current token
      g_token_val,     // current token value
      g_token_type,       // current expression type
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

// parse char, gen current token
void parseToken(){
    char *pp;

    while (g_cur_token = *p) {
        ++p;
        if (g_cur_token == '\n') {    // 换行符
            if (src) {
                printf("%lld: %.*s", line, (int)(p - lp), lp);
                lp = p;
                while (le < g_text) {
                    printf("%8.4s", &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
                            "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                            "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[*++le * 5]);

                    if (*le <= ADJ) printf(" %lld\n", *++le);
                    else printf("\n");
                }
            }
            ++line; //设置行数
        } else if ((g_cur_token >= 'a' && g_cur_token <= 'z') || (g_cur_token >= 'A' && g_cur_token <= 'Z') || g_cur_token == '_') {
            pp = p - 1;    // 标识符首字符指针

            //
            while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_') {
                g_cur_token = g_cur_token * 147 + *p;
                ++p;
            }
            g_cur_token = (g_cur_token << 6) + (p - pp);

            //
            g_cur_sym = g_symtab;
            while (g_cur_sym[Tk]) {
                if (g_cur_token == g_cur_sym[Hash] && !memcmp((char *)g_cur_sym[Name], pp, p - pp)) {
                    g_cur_token = g_cur_sym[Tk];
                    return;
                }
                g_cur_sym = g_cur_sym + Idsz;
            }
            g_cur_sym[Name] = (intll)pp;
            g_cur_sym[Hash] = g_cur_token;
            g_cur_token = g_cur_sym[Tk] = Id;
            return;
        } else if (g_cur_token >= '0' && g_cur_token <= '9') {
            if ((g_token_val = g_cur_token - '0')) {
                while (*p >= '0' && *p <= '9') {
                    g_token_val = g_token_val * 10 + *p++ - '0';
                }
            }
            else if (*p == 'x' || *p == 'X') {
                while ((g_cur_token = *++p) && ((g_cur_token >= '0' && g_cur_token <= '9') || (g_cur_token >= 'a' && g_cur_token <= 'f') || (g_cur_token >= 'A' && g_cur_token <= 'F'))) {
                    g_token_val = g_token_val * 16 + (g_cur_token & 15) + (g_cur_token >= 'A' ? 9 : 0);
                }
            }
            else {
                while (*p >= '0' && *p <= '7') {
                    g_token_val = g_token_val * 8 + *p++ - '0';
                }
            }
            g_cur_token = Num;
            return;
        } else {
            switch (g_cur_token) {
                case '#': {  while (*p != 0 && *p != '\n') { ++p; } break; }
                case '/': {
                              if (*p == '/') {              // 注释
                                  ++p;
                                  while (*p != 0 && *p != '\n') ++p;
                              } else {                        //除号
                                  g_cur_token = Div;
                                  return;
                              }
                              break;
                          }
                case '\'':
                case '"': {
                              pp = g_data;
                              while (*p != 0 && *p != g_cur_token) {
                                  if ((g_token_val = *p++) == '\\') {
                                      if ((g_token_val = *p++) == 'n') {
                                          g_token_val = '\n';
                                      }
                                  }
                                  if (g_cur_token == '"') {
                                    *g_data++ = g_token_val;
                                  }
                              }
                              ++p;
                              if (g_cur_token == '"') {
                                  g_token_val = (intll)pp;
                              } else {
                                  g_cur_token = Num;
                              }
                              return;
                          }
                case '=': { if (*p == '=') { ++p; g_cur_token = Eq; } else { g_cur_token = Assign; } return; }
                case '+': { if (*p == '+') { ++p; g_cur_token = Inc; } else { g_cur_token = Add; } return; }
                case '-': { if (*p == '-') { ++p; g_cur_token = Dec; } else g_cur_token = Sub; return; }
                case '!': { if (*p == '=') { ++p; g_cur_token = Ne; } return; }
                case '<': { if (*p == '=') { ++p; g_cur_token = Le; } else if (*p == '<') { ++p; g_cur_token = Shl; } else g_cur_token = Lt; return; }
                case '>': { if (*p == '=') { ++p; g_cur_token = Ge; } else if (*p == '>') { ++p; g_cur_token = Shr; } else g_cur_token = Gt; return; }
                case '|': { if (*p == '|') { ++p; g_cur_token = Lor; } else g_cur_token = Or; return; }
                case '&': { if (*p == '&') { ++p; g_cur_token = Lan; } else g_cur_token = And; return; }
                case '^': { g_cur_token = Xor; return; }
                case '%': { g_cur_token = Mod; return; }
                case '*': { g_cur_token = Mul; return; }
                case '[': { g_cur_token = Brak; return; }
                case '?': { g_cur_token = Cond; return; }
                case '~':
                case ';':
                case '{':
                case '}':
                case '(':
                case ')':
                case ']':
                case ',':
                case ':': { return ; }
                default: { break; }
            }
        }
    }
}

// try to parse token
void tryParseToken(intll tk) {
    if (g_cur_token != tk) {
        printf("%lld: open paren expected\n", line);
        exit(-1);
    }
    parseToken();
}

// 表达式, 根据g_token, 设置e
void expr(intll lev) {
    intll t, *d;
    switch (g_cur_token) {
        case 0: { printf("%lld: unexpected eof in expression\n", line); exit(-1); }
        case Num: {
                      *++g_text = IMM;
                      *++g_text = g_token_val;
                      parseToken();
                      g_token_type = INT;
                      break;
                  }
        case '"': {
                      *++g_text = IMM;
                      *++g_text = g_token_val;
                      parseToken();
                      while (g_cur_token == '"') {
                          parseToken();
                      }
                      g_data = (char *)((intll)g_data + sizeof(intll) & -sizeof(intll));
                      g_token_type = PTR;
                      break;
                  }
        case Sizeof: {
                         parseToken();
                         tryParseToken('(');
                         g_token_type = INT;
                         switch (g_cur_token) {
                             case Int: { parseToken(); break; }
                             case Char: { parseToken(); g_token_type = CHAR; break; }
                             default: { break; }
                         }
                         while (g_cur_token == Mul) {
                             parseToken();
                             g_token_type += PTR;
                         }
                         tryParseToken(')');

                         *++g_text = IMM;
                         *++g_text = (g_token_type == CHAR) ? sizeof(char) : sizeof(intll);
                         g_token_type = INT;
                         break;
                     }
        case Id: {   // 变量和函数
                     d = g_cur_sym;
                     parseToken();
                     if (g_cur_token == '(') {
                         parseToken();
                         t = 0;
                         while (g_cur_token != ')') {
                             expr(Assign);
                             *++g_text = PSH;
                             ++t;
                             if (g_cur_token == ',') {
                                 parseToken();
                             }
                         }
                         parseToken();

                         switch (d[Class]) {
                             case Sys: { *++g_text = d[Val]; break; }
                             case Fun: { *++g_text = JSR; *++g_text = d[Val]; break; }
                             default: { printf("%lld: bad function call\n", line); exit(-1); }
                         }

                         if (t>0) {
                             *++g_text = ADJ;
                             *++g_text = t;
                         }
                         g_token_type = d[Type];
                     } else if (d[Class] == Num) {
                         *++g_text = IMM;
                         *++g_text = d[Val];
                         g_token_type = INT;
                     } else {
                         switch (d[Class] ) {
                             case Loc: { *++g_text = LEA; *++g_text = loc - d[Val]; break; }
                             case Glo: { *++g_text = IMM; *++g_text = d[Val]; break; }
                             default: { printf("%lld: undefined variable\n", line); exit(-1);  }
                         }
                         *++g_text = ((g_token_type = d[Type]) == CHAR) ? LC : LI;
                     }
                     break;
                 }
        case '(': {
                      parseToken();
                      if (g_cur_token == Int || g_cur_token == Char) {
                          t = (g_cur_token == Int) ? INT : CHAR;
                          parseToken();
                          while (g_cur_token == Mul) {
                              parseToken();
                              t += PTR;
                          }

                          tryParseToken(')');

                          expr(Inc);
                          g_token_type = t;
                      } else {
                          expr(Assign);
                          tryParseToken(')');
                      }
                      break;
                  }
        case Mul: {
                      parseToken();
                      expr(Inc);
                      if (g_token_type > INT) {
                          g_token_type = g_token_type - PTR;
                      } else {
                          printf("%lld: bad dereference\n", line); exit(-1);
                      }
                      *++g_text = (g_token_type == CHAR) ? LC : LI;
                      break;
                  }
        case And: {
                      parseToken();
                      expr(Inc);
                      if (*g_text == LC || *g_text == LI) --g_text;
                      else { printf("%lld: bad address-of\n", line); exit(-1); }
                      g_token_type = g_token_type + PTR;
                      break;
                  }
        case '!': { parseToken(); expr(Inc); *++g_text = PSH; *++g_text = IMM; *++g_text = 0; *++g_text = EQ; g_token_type = INT; break; }
        case '~': { parseToken(); expr(Inc); *++g_text = PSH; *++g_text = IMM; *++g_text = -1; *++g_text = XOR; g_token_type = INT; break; }
        case Add: { parseToken(); expr(Inc); g_token_type = INT; break; }
        case Sub: {
                      parseToken();
                      *++g_text = IMM;
                      if (g_cur_token == Num) {
                          *++g_text = -g_token_val;
                          parseToken();
                      } else {
                          *++g_text = -1;
                          *++g_text = PSH;
                          expr(Inc);
                          *++g_text = MUL;
                      }
                      g_token_type = INT;
                      break;
                  }
        case Inc:
        case Dec: {
                      t = g_cur_token;
                      parseToken();
                      expr(Inc);
                      switch (*g_text) {
                          case LC: { *g_text = PSH; *++g_text = LC; break; }
                          case LI: { *g_text = PSH; *++g_text = LI; break; }
                          default: { printf("%lld: bad lvalue in pre-increment\n", line); exit(-1); }
                      }
                      *++g_text = PSH;
                      *++g_text = IMM;
                      *++g_text = (g_token_type > PTR) ? sizeof(intll) : sizeof(char);
                      *++g_text = (t == Inc) ? ADD : SUB;
                      *++g_text = (g_token_type == CHAR) ? SC : SI;
                      break;
                  }
        default: {
                     printf("%lld: bad expression\n", line);
                     exit(-1);
                 }
    }

    // "precedence climbing" or "Top Down Operator Precedence" method
    while (g_cur_token >= lev) {
        t = g_token_type;
        switch (g_cur_token) {
            case Assign: {
                             parseToken();
                             if (*g_text == LC || *g_text == LI) *g_text = PSH;
                             else { printf("%lld: bad lvalue in assignment\n", line); exit(-1); }

                             expr(Assign);
                             *++g_text = ((g_token_type = t) == CHAR) ? SC : SI;
                             break;
                         }
            case Cond: { parseToken();
                           *++g_text = BZ; d = ++g_text;
                           expr(Assign);
                           tryParseToken(':');
                           *d = (intll)(g_text + 3);
                           *++g_text = JMP;
                           d = ++g_text;
                           expr(Cond);
                           *d = (intll)(g_text + 1);
                           break;
                       }
            case Lor: { parseToken(); *++g_text = BNZ; d = ++g_text; expr(Lan); *d = (intll)(g_text + 1); g_token_type = INT;break; }
            case Lan: { parseToken(); *++g_text = BZ;  d = ++g_text; expr(Or);  *d = (intll)(g_text + 1); g_token_type = INT;break; }
            case Or:  { parseToken(); *++g_text = PSH; expr(Xor); *++g_text = OR;  g_token_type = INT;break; }
            case Xor: { parseToken(); *++g_text = PSH; expr(And); *++g_text = XOR; g_token_type = INT;break; }
            case And: { parseToken(); *++g_text = PSH; expr(Eq);  *++g_text = AND; g_token_type = INT;break; }
            case Eq:  { parseToken(); *++g_text = PSH; expr(Lt);  *++g_text = EQ;  g_token_type = INT;break; }
            case Ne:  { parseToken(); *++g_text = PSH; expr(Lt);  *++g_text = NE;  g_token_type = INT;break; }
            case Lt:  { parseToken(); *++g_text = PSH; expr(Shl); *++g_text = LT;  g_token_type = INT;break; }
            case Gt:  { parseToken(); *++g_text = PSH; expr(Shl); *++g_text = GT;  g_token_type = INT;break; }
            case Le:  { parseToken(); *++g_text = PSH; expr(Shl); *++g_text = LE;  g_token_type = INT;break; }
            case Ge:  { parseToken(); *++g_text = PSH; expr(Shl); *++g_text = GE;  g_token_type = INT;break; }
            case Shl: { parseToken(); *++g_text = PSH; expr(Add); *++g_text = SHL; g_token_type = INT;break; }
            case Shr: { parseToken(); *++g_text = PSH; expr(Add); *++g_text = SHR; g_token_type = INT;break; }
            case Add: { parseToken(); *++g_text = PSH; expr(Mul);
                          if ((g_token_type = t) > PTR) {
                              *++g_text = PSH;
                              *++g_text = IMM;
                              *++g_text = sizeof(intll);
                              *++g_text = MUL;
                          }
                          *++g_text = ADD;
                          break;
                      }
            case Sub: {
                          parseToken(); ;*++g_text = PSH; expr(Mul);
                          if (t > PTR && t == g_token_type) {
                              *++g_text = SUB;
                              *++g_text = PSH;
                              *++g_text = IMM;
                              *++g_text = sizeof(intll);
                              *++g_text = DIV;
                              g_token_type = INT;
                          } else if ((g_token_type = t) > PTR) {
                              *++g_text = PSH;
                              *++g_text = IMM;
                              *++g_text = sizeof(intll);
                              *++g_text = MUL;
                              *++g_text = SUB; }
                          else {
                              *++g_text = SUB;
                          }
                          break;
                      }
            case Mul: { parseToken(); *++g_text = PSH; expr(Inc); *++g_text = MUL; g_token_type = INT; break; }
            case Div: { parseToken(); *++g_text = PSH; expr(Inc); *++g_text = DIV; g_token_type = INT; break; }
            case Mod: { parseToken(); *++g_text = PSH; expr(Inc); *++g_text = MOD; g_token_type = INT; break; }
            case Inc:
            case Dec: {
                          switch (*g_text) {
                              case LC: { *g_text = PSH; *++g_text = LC; }
                              case LI: { *g_text = PSH; *++g_text = LI; }
                              default: { printf("%lld: bad lvalue in post-increment\n", line); exit(-1); }
                          }

                          *++g_text = PSH;
                          *++g_text = IMM;
                          *++g_text = (g_token_type > PTR) ? sizeof(intll) : sizeof(char);
                          *++g_text = (g_cur_token == Inc) ? ADD : SUB;
                          *++g_text = (g_token_type == CHAR) ? SC : SI;
                          *++g_text = PSH; *++g_text = IMM;
                          *++g_text = (g_token_type > PTR) ? sizeof(intll) : sizeof(char);
                          *++g_text = (g_cur_token == Inc) ? SUB : ADD;
                          parseToken();
                          break;
                      }
            case Brak: {
                           parseToken();
                           *++g_text = PSH;
                           expr(Assign);
                           tryParseToken(']');

                           if (t > PTR) {
                               *++g_text = PSH;
                               *++g_text = IMM;
                               *++g_text = sizeof(intll);
                               *++g_text = MUL;
                           } else if (t < PTR) {
                               printf("%lld: pointer type expected\n", line);
                               exit(-1);
                           }
                           *++g_text = ADD;
                           *++g_text = ((g_token_type = t - PTR) == CHAR) ? LC : LI;
                           break;
                       }
            default: {
                         printf("%lld: compiler error tk=%lld\n", line, g_cur_token);
                         exit(-1);
                     }
        }
    }
}

/*
 * 语句
 * 可识别一下语句:
 *   if ( <expr> ) <statement> [else <statement>]
     while ( <expr> ) <statement>
            { <statement> }
     return xxx;
     { [stmt] }
     <empty statement>;
     expression; (expression end with semicolon)
 * */
void stmt() {
    intll *a, *b;
    switch (g_cur_token) {
        case If: {
                     parseToken();

                     tryParseToken('(');
                     expr(Assign);
                     tryParseToken(')');

                     *++g_text = BZ;
                     b = ++g_text;

                     stmt();

                     if (g_cur_token == Else) {
                         *b = (intll)(g_text + 3);
                         *++g_text = JMP;
                         b = ++g_text;

                         parseToken();
                         stmt();
                     }
                     *b = (intll)(g_text + 1);
                     break;
                 }
        case While: {
                        parseToken();

                        a = g_text + 1;   //address a

                        tryParseToken('(');
                        expr(Assign);
                        tryParseToken(')');

                        *++g_text = BZ;
                        b = ++g_text;
                        stmt();
                        *++g_text = JMP;
                        *++g_text = (intll)a;
                        *b = (intll)(g_text + 1);   //address b
                        break;
                    }
        case Return: {
                         parseToken();
                         if (g_cur_token != ';') {
                             expr(Assign);
                         }
                         *++g_text = LEV;
                         tryParseToken(';');
                         break;
                     }
        case '{': {
                      parseToken();
                      while (g_cur_token != '}') {
                          stmt();
                      }
                      parseToken();
                      break;
                  }
        case ';': { parseToken(); break; }
        default: {
                     expr(Assign);
                     tryParseToken(';');
                     break;
                 }
    }
}

//
int main(int argc, char** argv){
    intll fd,           // source fd
          bt,           //
          ty,           // token
          poolsz,       //
          *idmain;
    intll *pc,  // program counter pointer
          *sp,  // stack pointer
          *bp,  // stack base pointer
          a,    //
          cycle; // vm registers

    intll i, *t; // temps

    --argc; ++argv;
    if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
    if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
    if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; }

    if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

    poolsz = 256*1024; // arbitrary size
    if (!(g_symtab = malloc(poolsz))) { printf("could not malloc(%lld) symbol area\n", poolsz); return -1; }
    if (!(le = g_text = malloc(poolsz))) { printf("could not malloc(%lld) text area\n", poolsz); return -1; }
    if (!(g_data = malloc(poolsz))) { printf("could not malloc(%lld) data area\n", poolsz); return -1; }
    if (!(sp = malloc(poolsz))) { printf("could not malloc(%lld) stack area\n", poolsz); return -1; }

    memset(g_symtab,  0, poolsz);
    memset(g_text,    0, poolsz);
    memset(g_data, 0, poolsz);

    p = "char else enum if int return sizeof while "
        "open read close printf malloc free memset memcmp exit void main";
    i = Char;
    while (i <= While) {   // add keywords to symbol table
        parseToken();
        g_cur_sym[Tk] = i++;
    }

    i = OPEN;
    while (i <= EXIT) {  // add library to symbol table
        parseToken();
        g_cur_sym[Class] = Sys;
        g_cur_sym[Type] = INT;
        g_cur_sym[Val] = i++;
    }
                                                                                                  //
    parseToken();
    g_cur_sym[Tk] = Char; // handle void type
                          //
    parseToken();
    idmain = g_cur_sym; // keep track of main

    if (!(lp = p = malloc(poolsz))) {
        printf("could not malloc(%lld) source area\n", poolsz);
        return -1;
    }
    if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %lld\n", i); return -1; }
    p[i] = 0;
    close(fd);

    // parse declarations
    line = 1;
    parseToken();
    while (g_cur_token) {
        bt = INT; // basetype
        switch (g_cur_token) {
            case Int: { parseToken(); bt = INT; break; }
            case Char: { parseToken(); bt = CHAR; break; }
            case Enum: { parseToken();
                           if (g_cur_token != '{') parseToken();
                           if (g_cur_token == '{') {
                               parseToken();
                               i = 0;
                               while (g_cur_token != '}') {
                                   if (g_cur_token != Id) {
                                       printf("%lld: bad enum identifier %lld\n", line, g_cur_token);
                                       return -1;
                                   }
                                   parseToken();
                                   if (g_cur_token == Assign) {
                                       parseToken();
                                       if (g_cur_token != Num) { printf("%lld: bad enum initializer\n", line); return -1; }
                                       i = g_token_val;
                                       parseToken();
                                   }
                                   g_cur_sym[Class] = Num; g_cur_sym[Type] = INT; g_cur_sym[Val] = i++;
                                   if (g_cur_token == ',') parseToken();
                               }
                               parseToken();
                           }
                           break;
                       }
            default: break;
        }

        while (g_cur_token != ';' && g_cur_token != '}') {
            ty = bt;
            while (g_cur_token == Mul) { parseToken(); ty = ty + PTR; }
            if (g_cur_token != Id) { printf("%lld: bad global declaration\n", line); return -1; }
            if (g_cur_sym[Class]) { printf("%lld: duplicate global definition\n", line); return -1; }
            parseToken();
            g_cur_sym[Type] = ty;
            if (g_cur_token == '(') { // function
                g_cur_sym[Class] = Fun;
                g_cur_sym[Val] = (intll)(g_text + 1);
                parseToken(); i = 0;
                while (g_cur_token != ')') {
                    ty = INT;
                    if (g_cur_token == Int) parseToken();
                    else if (g_cur_token == Char) { parseToken(); ty = CHAR; }

                    while (g_cur_token == Mul) { parseToken(); ty = ty + PTR; }

                    if (g_cur_token != Id) { printf("%lld: bad parameter declaration\n", line); return -1; }
                    if (g_cur_sym[Class] == Loc) { printf("%lld: duplicate parameter definition\n", line); return -1; }
                    g_cur_sym[HClass] = g_cur_sym[Class]; g_cur_sym[Class] = Loc;
                    g_cur_sym[HType]  = g_cur_sym[Type];  g_cur_sym[Type] = ty;
                    g_cur_sym[HVal]   = g_cur_sym[Val];   g_cur_sym[Val] = i++;
                    parseToken();
                    if (g_cur_token == ',') parseToken();
                }
                parseToken();
                if (g_cur_token != '{') { printf("%lld: bad function definition\n", line); return -1; }
                loc = ++i;
                parseToken();
                while (g_cur_token == Int || g_cur_token == Char) {
                    bt = (g_cur_token == Int) ? INT : CHAR;
                    parseToken();
                    while (g_cur_token != ';') {
                        ty = bt;
                        while (g_cur_token == Mul) { parseToken(); ty = ty + PTR; }
                        if (g_cur_token != Id) { printf("%lld: bad local declaration\n", line); return -1; }
                        if (g_cur_sym[Class] == Loc) { printf("%lld: duplicate local definition\n", line); return -1; }
                        g_cur_sym[HClass] = g_cur_sym[Class]; g_cur_sym[Class] = Loc;
                        g_cur_sym[HType]  = g_cur_sym[Type];  g_cur_sym[Type] = ty;
                        g_cur_sym[HVal]   = g_cur_sym[Val];   g_cur_sym[Val] = ++i;
                        parseToken();
                        if (g_cur_token == ',') parseToken();
                    }
                    parseToken();
                }
                *++g_text = ENT;
                *++g_text = i - loc;
                while (g_cur_token != '}') stmt();
                *++g_text = LEV;
                g_cur_sym = g_symtab; // unwind symbol table locals
                while (g_cur_sym[Tk]) {
                    if (g_cur_sym[Class] == Loc) {
                        g_cur_sym[Class] = g_cur_sym[HClass];
                        g_cur_sym[Type] = g_cur_sym[HType];
                        g_cur_sym[Val] = g_cur_sym[HVal];
                    }
                    g_cur_sym = g_cur_sym + Idsz;
                }
            } else {
                g_cur_sym[Class] = Glo;
                g_cur_sym[Val] = (intll)g_data;
                g_data = g_data + sizeof(int);
            }
            if (g_cur_token == ',') parseToken();
        }
        parseToken();
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
        i = *pc++;
        ++cycle;
        if (debug) {
            printf("%lld> %.4s", cycle,
                    &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
                    "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                    "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[i * 5]);
            if (i <= ADJ) printf(" %lld\n", *pc); else printf("\n");
        }
        switch (i) {
            case LEA: { a = (intll)(bp + *pc++); break; }                             // load local address
            case IMM: { a = *pc++; break; }
            case JMP: { pc = (intll *)*pc; break; }                                   // jump
            case JSR: { *--sp = (intll)(pc + 1); pc = (intll *)*pc; break; }        // jump to subroutine
            case BZ:  { pc = a ? pc + 1 : (intll *)*pc; break; }                     // branch if zero
            case BNZ: { pc = !a ? pc + 1 : (intll *)*pc; break; }                     // branch if zero
            case ENT: { *--sp = (intll)bp; bp = sp; sp = sp - *pc++; break; }     // enter subroutine
            case ADJ: { sp = sp + *pc++; break; }               // stack adjust
            case LEV: { sp = bp; bp = (intll *)*sp++; pc = (intll *)*sp++; break;} // leave subroutine
            case LI:  { a = *(intll *)a;   break; }                                  // load int
            case LC:  { a = *(char *)a;    break; }                               // load char
            case SI:  { *(intll *)*sp++ = a;  break; }                               // store int
            case SC:  { a = *(char *)*sp++ = a;    break; }                        // store char
            case PSH: { *--sp = a; break; }                              // push
            case OR:  { a = *sp++ |  a; break;}
            case XOR: { a = *sp++ ^  a; break;}
            case AND: { a = *sp++ &  a; break;}
            case EQ:  { a = *sp++ == a; break;}
            case NE:  { a = *sp++ != a; break;}
            case LT:  { a = *sp++ <  a; break;}
            case GT:  { a = *sp++ >  a; break;}
            case LE:  { a = *sp++ <= a; break;}
            case GE:  { a = *sp++ >= a; break;}
            case SHL: { a = *sp++ << a; break;}
            case SHR: { a = *sp++ >> a; break;}
            case ADD: { a = *sp++ +  a; break;}
            case SUB: { a = *sp++ -  a; break;}
            case MUL: { a = *sp++ *  a; break;}
            case DIV: { a = *sp++ /  a; break;}
            case MOD: { a = *sp++ %  a; break;}
            case OPEN:{ a = open((char *)sp[1], *sp); break; }
            case READ:{ a = read(sp[2], (char *)sp[1], *sp); break; }
            case CLOS:{ a = close(*sp);break; }
            case PRTF:{ t = sp + pc[1]; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); break;}
            case MALC:{ a = (intll)malloc(*sp);break; }
            case FREE:{ free((void *)*sp); break; }
            case MSET:{ a = (intll)memset((char *)sp[2], sp[1], *sp);break; }
            case MCMP:{ a = memcmp((char *)sp[2], (char *)sp[1], *sp); break; }
            case EXIT:{ printf("exit(%lld) cycle = %lld\n", *sp, cycle); return *sp; }
            default: { printf("unknown instruction = %lld! cycle = %lld\n", i, cycle); return -1; }
        }
    }
}
