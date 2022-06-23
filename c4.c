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

char *g_src_ptr,        // 源码文本当前位置指针
     *g_src_ptr_last,       // current position in source code
     *g_data;   // data/bss pointer

intll *g_text,          // 代码段指针
      *g_text_dump,  // current position in emitted code
      *g_sym_id,      // currently parsed identifier
      *g_symtab,     // symbol table (simple list of identifiers)
      g_token,       // current token
      g_token_val,     // current token value
      g_expr_type,       // current expression type
      loc,      // local variable offset
      line,     // current line number
      src,      // print source and assembly flag
      debug,    // print executed instructions
      *pc,  // program counter pointer
      *sp,  // stack pointer
      *bp,  // stack base pointer
      a,    //
      cycle; // vm registers

// tokens and classes (operators last and in precedence order)
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// opcodes
enum { LEA ,IMM ,JMP ,JSR ,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,
  OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
  OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT };

// expr types
enum { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
enum { Token, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

// parse char p point to as token, gen current token
// 根据当前字符(g_src_ptr所指), 设置g_token, g_token_val
void parseToken(){
  char *last_pos;
  intll hash;

  while ((g_token = *(g_src_ptr++))) {
    if (g_token == '\n') {    // 换行符
      if (src) {    // 输出源码
        printf("%lld: %.*s", line, (int)(g_src_ptr - g_src_ptr_last), g_src_ptr_last);
        g_src_ptr_last = g_src_ptr;
        while (g_text_dump < g_text) {
          printf("%8.4s", &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
              "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
              "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[*++g_text_dump * 5]);

          if (*g_text_dump <= ADJ) printf(" %lld\n", *++g_text_dump);
          else printf("\n");
        }
      }
      ++line; //设置行数
    } else if ((g_token >= 'a' && g_token <= 'z') || (g_token >= 'A' && g_token <= 'Z') || g_token == '_') {
      last_pos = g_src_ptr - 1;    // 标识符首字符指针

      //计算符号hash
      hash = g_token;
      while ((*g_src_ptr >= 'a' && *g_src_ptr <= 'z') || (*g_src_ptr >= 'A' && *g_src_ptr <= 'Z') || (*g_src_ptr >= '0' && *g_src_ptr <= '9') || *g_src_ptr == '_') {
        hash = hash * 147 + *g_src_ptr;
        ++g_src_ptr;
      }

      //查找符号表中的符号
      g_sym_id = g_symtab;
      while (g_sym_id[Token]) {
        // 如果当前token的hash和name和符号表中相等
        if (hash == g_sym_id[Hash] && !memcmp((char *)g_sym_id[Name], last_pos, g_src_ptr - last_pos)) {
          g_token = g_sym_id[Token];   //
          return;
        }
        g_sym_id = g_sym_id + Idsz;   //下一个sym_id
      }

      // 符号表中不存在的
      g_sym_id[Name] = (intll)last_pos;
      g_sym_id[Hash] = hash;
      g_token = g_sym_id[Token] = Id;
      return;
    } else if (g_token >= '0' && g_token <= '9') {
      if ((g_token_val = g_token - '0')) {
        while (*g_src_ptr >= '0' && *g_src_ptr <= '9') {
          g_token_val = g_token_val * 10 + *g_src_ptr++ - '0';
        }
      } else if (*g_src_ptr == 'x' || *g_src_ptr == 'X') {
        while ((g_token = *++g_src_ptr) && ((g_token >= '0' && g_token <= '9') || (g_token >= 'a' && g_token <= 'f') || (g_token >= 'A' && g_token <= 'F'))) {
          g_token_val = g_token_val * 16 + (g_token & 15) + (g_token >= 'A' ? 9 : 0);
        }
      } else {
        while (*g_src_ptr >= '0' && *g_src_ptr <= '7') {
          g_token_val = g_token_val * 8 + *g_src_ptr++ - '0';
        }
      }
      g_token = Num;
      return;
    } else {
      switch (g_token) {
        case '#': {
                    while (*g_src_ptr != 0 && *g_src_ptr != '\n') {
                      ++g_src_ptr;
                    }
                    break;
                  }
        case '/': {
                    if (*g_src_ptr == '/') {              // 注释
                      ++g_src_ptr;
                      while (*g_src_ptr != 0 && *g_src_ptr != '\n') {
                        ++g_src_ptr;
                      }
                    } else {                        //除号
                      g_token = Div;
                      return;
                    }
                    break;
                  }
        case '\'':      // 字符
        case '"': {     // 字符串开始
                    last_pos = g_data;

                    // 处理字符串
                    while (*g_src_ptr != 0 && *g_src_ptr != g_token) {
                      g_token_val = *g_src_ptr++;
                      if (g_token_val == '\\') {    // 处理 \n
                        g_token_val = *g_src_ptr++;
                        if (g_token_val == 'n') {
                          g_token_val = '\n';
                        }
                      }
                      if (g_token == '"') {  //字符串
                        *g_data++ = g_token_val; //将字符串各个字符存入g_data
                      }
                    }

                    ++g_src_ptr;
                    if (g_token == '"') {   //token为字符串, 设置g_token_val 为g_data中字符串的起始位置
                      g_token_val = (intll)last_pos;
                    } else {                // 否则未char, 设置token为Num, token_val为char值
                      g_token = Num;
                    }
                    return;
                  }
        case '=': { if (*g_src_ptr == '=') { ++g_src_ptr; g_token = Eq; } else { g_token = Assign; } return; }
        case '+': { if (*g_src_ptr == '+') { ++g_src_ptr; g_token = Inc; } else { g_token = Add; } return; }
        case '-': { if (*g_src_ptr == '-') { ++g_src_ptr; g_token = Dec; } else g_token = Sub; return; }
        case '!': { if (*g_src_ptr == '=') { ++g_src_ptr; g_token = Ne; } return; }
        case '<': { if (*g_src_ptr == '=') { ++g_src_ptr; g_token = Le; } else if (*g_src_ptr == '<') { ++g_src_ptr; g_token = Shl; } else g_token = Lt; return; }
        case '>': { if (*g_src_ptr == '=') { ++g_src_ptr; g_token = Ge; } else if (*g_src_ptr == '>') { ++g_src_ptr; g_token = Shr; } else g_token = Gt; return; }
        case '|': { if (*g_src_ptr == '|') { ++g_src_ptr; g_token = Lor; } else g_token = Or; return; }
        case '&': { if (*g_src_ptr == '&') { ++g_src_ptr; g_token = Lan; } else g_token = And; return; }
        case '^': { g_token = Xor; return; }
        case '%': { g_token = Mod; return; }
        case '*': { g_token = Mul; return; }
        case '[': { g_token = Brak; return; }
        case '?': { g_token = Cond; return; }
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
  if (g_token != tk) {
    printf("%lld: unexpected token: %lld, tk: %lld\n", line, g_token, tk);
    exit(-1);
  }
  parseToken();
}

// 表达式, 根据g_token, 设置g_text, g_data
void expr(intll lev) {
  intll t, *d;

  // 预处理g_token
  switch (g_token) {
    case 0: {
              printf("%lld: unexpected eof in expression\n", line);
              exit(-1);
            }
    case Num: {
                tryParseToken(Num);
                *++g_text = IMM;
                *++g_text = g_token_val;
                g_expr_type = INT;
                break;
              }
    case '"': {
                *++g_text = IMM;
                *++g_text = g_token_val;
                tryParseToken('"');
                while (g_token == '"') {
                  tryParseToken('"');
                }
                g_data = (char *)(((intll)g_data + sizeof(intll)) & (-sizeof(intll)));
                g_expr_type = PTR;
                break;
              }
    case Sizeof: {
                   tryParseToken(Sizeof);
                   tryParseToken('(');
                   g_expr_type = INT;
                   switch (g_token) {
                     case Int: { tryParseToken(Int); g_expr_type = INT; break; }
                     case Char: { tryParseToken(Char); g_expr_type = CHAR; break; }
                     case Mul: {
                                 while (g_token == Mul) {
                                   tryParseToken(Mul);
                                   g_expr_type += PTR;
                                 }
                                 break;
                               }
                     default: { break; }
                   }
                   tryParseToken(')');

                   *++g_text = IMM;
                   *++g_text = (g_expr_type == CHAR) ? sizeof(char) : sizeof(intll);
                   g_expr_type = INT;
                   break;
                 }
    case Id: {   // 变量和函数
               tryParseToken(Id);
               d = g_sym_id;
               if (g_token == '(') {    //<func_name>(<expr>[,<expr>]);
                 tryParseToken('(');
                 t = 0;
                 // 处理括号中的
                 while (g_token != ')') {
                   expr(Assign);        //
                   *++g_text = PSH;
                   ++t;
                   if (g_token == ',') {
                     tryParseToken(',');
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
                 g_expr_type = d[Type];
               } else if (d[Class] == Num) {  // 数字
                 *++g_text = IMM;
                 *++g_text = d[Val];
                 g_expr_type = INT;
               } else {  // 变量
                 switch (d[Class] ) {
                   case Loc: { *++g_text = LEA; *++g_text = loc - d[Val]; break; }
                   case Glo: { *++g_text = IMM; *++g_text = d[Val]; break; }
                   default: { printf("%lld: undefined variable\n", line); exit(-1);  }
                 }
                 *++g_text = ((g_expr_type = d[Type]) == CHAR) ? LC : LI;
               }
               break;
             }
    case '(': {
                tryParseToken('(');
                if (g_token == Int || g_token == Char) {
                  t = (g_token == Int) ? INT : CHAR;
                  parseToken();
                  while (g_token == Mul) {
                    parseToken();
                    t += PTR;
                  }

                  tryParseToken(')');

                  expr(Inc);
                  g_expr_type = t;
                } else {
                  expr(Assign);
                  tryParseToken(')');
                }
                break;
              }
    case Mul: { // * <expr>
                tryParseToken(Mul);
                expr(Inc);
                if (g_expr_type >= PTR) {    // *p
                  g_expr_type = g_expr_type - PTR;
                } else {
                  printf("%lld: bad dereference\n", line); exit(-1);
                }

                *++g_text = (g_expr_type == CHAR) ? LC : LI;
                break;
              }
    case And: {
                tryParseToken(And);
                expr(Inc);
                if (*g_text == LC || *g_text == LI) {
                  --g_text;
                } else {
                  printf("%lld: bad address-of\n", line);
                  exit(-1);
                }
                g_expr_type = g_expr_type + PTR;
                break;
              }
    case '!': { parseToken(); expr(Inc); *++g_text = PSH; *++g_text = IMM; *++g_text = 0; *++g_text = EQ; g_expr_type = INT; break; }
    case '~': { parseToken(); expr(Inc); *++g_text = PSH; *++g_text = IMM; *++g_text = -1; *++g_text = XOR; g_expr_type = INT; break; }
    case Add: { parseToken(); expr(Inc); g_expr_type = INT; break; }
    case Sub: {
                parseToken();
                *++g_text = IMM;
                if (g_token == Num) {
                  *++g_text = -g_token_val;
                  parseToken();
                } else {
                  *++g_text = -1;
                  *++g_text = PSH;
                  expr(Inc);
                  *++g_text = MUL;
                }
                g_expr_type = INT;
                break;
              }
    case Inc:
    case Dec: {
                t = g_token;
                tryParseToken(t);
                expr(Inc);
                switch (*g_text) {
                  case LC: { *g_text = PSH; *++g_text = LC; break; }
                  case LI: { *g_text = PSH; *++g_text = LI; break; }
                  default: { printf("%lld: bad lvalue in pre-increment\n", line); exit(-1); }
                }
                *++g_text = PSH;
                *++g_text = IMM;
                *++g_text = (g_expr_type > PTR) ? sizeof(intll) : sizeof(char);
                *++g_text = (t == Inc) ? ADD : SUB;
                *++g_text = (g_expr_type == CHAR) ? SC : SI;
                break;
              }
    default: {
               printf("%lld: bad expression\n", line);
               exit(-1);
             }
  }

  // "precedence climbing" or "Top Down Operator Precedence" method
  while (g_token >= lev) {
    t = g_expr_type;
    switch (g_token) {
      case Assign: {    // xx = yy ;
                     tryParseToken(Assign);
                     if (*g_text == LC || *g_text == LI) {
                      *g_text = PSH;
                     } else {
                       printf("%lld: bad lvalue in assignment\n", line);
                       exit(-1);
                     }

                     expr(Assign);
                     *++g_text = ((g_expr_type = t) == CHAR) ? SC : SI;
                     break;
                   }
      case Cond: {
                   parseToken();
                   *++g_text = JZ; d = ++g_text;
                   expr(Assign);
                   tryParseToken(':');
                   *d = (intll)(g_text + 3);
                   *++g_text = JMP;
                   d = ++g_text;
                   expr(Cond);
                   *d = (intll)(g_text + 1);
                   break;
                 }
      case Lor: { parseToken(); *++g_text = JNZ; d = ++g_text; expr(Lan); *d = (intll)(g_text + 1); g_expr_type = INT;break; }
      case Lan: { parseToken(); *++g_text = JZ;  d = ++g_text; expr(Or);  *d = (intll)(g_text + 1); g_expr_type = INT;break; }
      case Or:  { parseToken(); *++g_text = PSH; expr(Xor); *++g_text = OR;  g_expr_type = INT;break; }
      case Xor: { parseToken(); *++g_text = PSH; expr(And); *++g_text = XOR; g_expr_type = INT;break; }
      case And: { parseToken(); *++g_text = PSH; expr(Eq);  *++g_text = AND; g_expr_type = INT;break; }
      case Eq:  { parseToken(); *++g_text = PSH; expr(Lt);  *++g_text = EQ;  g_expr_type = INT;break; }
      case Ne:  { parseToken(); *++g_text = PSH; expr(Lt);  *++g_text = NE;  g_expr_type = INT;break; }
      case Lt:  { parseToken(); *++g_text = PSH; expr(Shl); *++g_text = LT;  g_expr_type = INT;break; }
      case Gt:  { parseToken(); *++g_text = PSH; expr(Shl); *++g_text = GT;  g_expr_type = INT;break; }
      case Le:  { parseToken(); *++g_text = PSH; expr(Shl); *++g_text = LE;  g_expr_type = INT;break; }
      case Ge:  { parseToken(); *++g_text = PSH; expr(Shl); *++g_text = GE;  g_expr_type = INT;break; }
      case Shl: { parseToken(); *++g_text = PSH; expr(Add); *++g_text = SHL; g_expr_type = INT;break; }
      case Shr: { parseToken(); *++g_text = PSH; expr(Add); *++g_text = SHR; g_expr_type = INT;break; }
      case Add: { parseToken(); *++g_text = PSH; expr(Mul);
                  if ((g_expr_type = t) > PTR) {
                    *++g_text = PSH;
                    *++g_text = IMM;
                    *++g_text = sizeof(intll);
                    *++g_text = MUL;
                  }
                  *++g_text = ADD;
                  break;
                }
      case Sub: {
                  parseToken();
                  *++g_text = PSH;
                  expr(Mul);
                  if (t > PTR && t == g_expr_type) {
                    *++g_text = SUB;
                    *++g_text = PSH;
                    *++g_text = IMM;
                    *++g_text = sizeof(intll);
                    *++g_text = DIV;
                    g_expr_type = INT;
                  } else if ((g_expr_type = t) > PTR) {
                    *++g_text = PSH;
                    *++g_text = IMM;
                    *++g_text = sizeof(intll);
                    *++g_text = MUL;
                    *++g_text = SUB;
                  } else {
                    *++g_text = SUB;
                  }
                  break;
                }
      case Mul: { parseToken(); *++g_text = PSH; expr(Inc); *++g_text = MUL; g_expr_type = INT; break; }
      case Div: { parseToken(); *++g_text = PSH; expr(Inc); *++g_text = DIV; g_expr_type = INT; break; }
      case Mod: { parseToken(); *++g_text = PSH; expr(Inc); *++g_text = MOD; g_expr_type = INT; break; }
      case Inc:
      case Dec: {
                  switch (*g_text) {
                    case LC: { *g_text = PSH; *++g_text = LC; break; }
                    case LI: { *g_text = PSH; *++g_text = LI; break; }
                    default: { printf("%lld: bad lvalue in post-increment\n", line); exit(-1); }
                  }

                  *++g_text = PSH;
                  *++g_text = IMM;
                  *++g_text = (g_expr_type > PTR) ? sizeof(intll) : sizeof(char);
                  *++g_text = (g_token == Inc) ? ADD : SUB;
                  *++g_text = (g_expr_type == CHAR) ? SC : SI;
                  *++g_text = PSH;
                  *++g_text = IMM;
                  *++g_text = (g_expr_type > PTR) ? sizeof(intll) : sizeof(char);
                  *++g_text = (g_token == Inc) ? SUB : ADD;
                  parseToken();
                  break;
                }
      case Brak: {   //数组取值   <array_name>[<>] =
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
                   *++g_text = ((g_expr_type = t - PTR) == CHAR) ? LC : LI;
                   break;
                 }
      default: {
                 printf("%lld: compiler error tk=%lld\n", line, g_token);
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
  switch (g_token) {
    case If: {  // if ( <expr> ) <stmt> [ else <stmt> ]
              /*
               *  if (...) <statement> [else <statement>]

                  if (<cond>)                   <cond>
                                                JZ a
                    <true_statement>   ===>     <true_statement>
                  else:                         JMP b
                a:                           a:
                    <false_statement>           <false_statement>
                b:                           b:
               * */
               tryParseToken(If);
               tryParseToken('(');
               expr(Assign);
               tryParseToken(')');

               *++g_text = JZ;
               b = ++g_text;
               stmt();

               // else <stmt>
               if (g_token == Else) {
                 *b = (intll)(g_text + 3);
                 *++g_text = JMP;
                 b = ++g_text;

                 tryParseToken(Else);
                 stmt();
               }

               // b
               *b = (intll)(g_text + 1);
               break;
             }
    case While: { // while ( <expr> ) <stmt>
                  tryParseToken(While);

                  a = g_text + 1;   //address a

                  tryParseToken('(');
                  expr(Assign);
                  tryParseToken(')');

                  *++g_text = JZ;
                  b = ++g_text;
                  stmt();
                  *++g_text = JMP;
                  *++g_text = (intll)a;
                  *b = (intll)(g_text + 1);   //address b
                  break;
                }
    case Return: { //return [<expr>];
                   tryParseToken(Return);
                   if (g_token != ';') {
                     expr(Assign);
                   }
                   *++g_text = LEV;
                   tryParseToken(';');
                   break;
                 }
    case '{': { // { [<stmt>] }
                tryParseToken('{');
                while (g_token != '}') {
                  stmt();
                }
                tryParseToken('}');
                break;
              }
    case ';': {  // ; 空语句
                tryParseToken(';');
                break;
              }
    default: {  // <expr> ;
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

  intll i,
        *t; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256*1024; // arbitrary size
  if (!(g_symtab = malloc(poolsz))) { printf("could not malloc(%lld) symbol area\n", poolsz); return -1; }
  if (!(g_text_dump = g_text = malloc(poolsz))) { printf("could not malloc(%lld) text area\n", poolsz); return -1; }
  if (!(g_data = malloc(poolsz))) { printf("could not malloc(%lld) data area\n", poolsz); return -1; }
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%lld) stack area\n", poolsz); return -1; }

  memset(g_symtab,  0, poolsz);
  memset(g_text,    0, poolsz);
  memset(g_data, 0, poolsz);

  g_src_ptr = "char else enum if int return sizeof while "
    "open read close printf malloc free memset memcmp exit void main";

  // add keywords to symbol table
  i = Char;
  while (i <= While) {
    parseToken();
    g_sym_id[Token] = i++;
  }

  // add library to symbol table
  i = OPEN;
  while (i <= EXIT) {
    parseToken();
    g_sym_id[Class] = Sys;
    g_sym_id[Type] = INT;
    g_sym_id[Val] = i++;
  }
  //
  parseToken();
  g_sym_id[Token] = Char; // handle void type
                          //
  parseToken();
  idmain = g_sym_id; // keep track of main

  if (!(g_src_ptr_last = g_src_ptr = malloc(poolsz))) {
    printf("could not malloc(%lld) source area\n", poolsz);
    return -1;
  }
  if ((i = read(fd, g_src_ptr, poolsz-1)) <= 0) { printf("read() returned %lld\n", i); return -1; }
  g_src_ptr[i] = 0;
  close(fd);

  line = 1;
  // parse declarations
  parseToken();
  while (g_token) {
    bt = INT; // basetype
              //
    switch (g_token) {
      case Int: { tryParseToken(Int); bt = INT; break; }
      case Char: { tryParseToken(Char); bt = CHAR; break; }
      case Enum: {
                   tryParseToken(Enum);
                   if (g_token != '{') {
                     parseToken();
                   }
                   if (g_token == '{') {
                     tryParseToken('{');
                     i = 0;
                     while (g_token != '}') {
                       tryParseToken(Id);
                       if (g_token == Assign) {
                         tryParseToken(Assign);
                         if (g_token != Num) {
                           printf("%lld: bad enum initializer\n", line);
                           return -1;
                         }
                         i = g_token_val;
                         parseToken();
                       }
                       g_sym_id[Class] = Num;
                       g_sym_id[Type] = INT;
                       g_sym_id[Val] = i++;
                       if (g_token == ',') {
                         tryParseToken(',');
                       }
                     }
                     parseToken();  // '}'
                   }
                   break;
                 }
      default: break;
    }

    // 全局声明
    while (g_token != ';' && g_token != '}') {
      ty = bt;

      while (g_token == Mul) {
        tryParseToken(Mul);
        ty = ty + PTR;
      }

      if (g_token != Id) {
        printf("%lld: bad global declaration\n", line);
        return -1;
      }
      if (g_sym_id[Class]) {
        printf("%lld: duplicate global definition\n", line);
        return -1;
      }
      parseToken();
      g_sym_id[Type] = ty;

      if (g_token == '(') { // function
        g_sym_id[Class] = Fun;
        g_sym_id[Val] = (intll)(g_text + 1);
        parseToken();
        i = 0;
        // 函数参数
        while (g_token != ')') {
          ty = INT;

          switch (g_token) {
            case Int: { parseToken(); ty = INT; break; }
            case Char: { parseToken(); ty = CHAR; break; }
            default: { break; }
          }

          while (g_token == Mul) {
            parseToken();
            ty = ty + PTR;
          }

          if (g_token != Id) {
            printf("%lld: bad parameter declaration\n", line);
            return -1;
          }
          if (g_sym_id[Class] == Loc) {
            printf("%lld: duplicate parameter definition\n", line);
            return -1;
          }
          g_sym_id[HClass] = g_sym_id[Class]; g_sym_id[Class] = Loc;
          g_sym_id[HType]  = g_sym_id[Type];  g_sym_id[Type] = ty;
          g_sym_id[HVal]   = g_sym_id[Val];   g_sym_id[Val] = i++;
          parseToken();
          if (g_token == ',') {
            parseToken();
          }
        }
        parseToken();
        if (g_token != '{') {
          printf("%lld: bad function definition\n", line);
          return -1;
        }
        loc = ++i;
        parseToken();
        while (g_token == Int || g_token == Char) {
          bt = (g_token == Int) ? INT : CHAR;
          parseToken();
          while (g_token != ';') {
            ty = bt;
            while (g_token == Mul) {
              parseToken(); ty = ty + PTR;
            }
            if (g_token != Id) {
              printf("%lld: bad local declaration\n", line);
              return -1;
            }
            if (g_sym_id[Class] == Loc) {
              printf("%lld: duplicate local definition\n", line);
              return -1;
            }
            g_sym_id[HClass] = g_sym_id[Class]; g_sym_id[Class] = Loc;
            g_sym_id[HType]  = g_sym_id[Type];  g_sym_id[Type] = ty;
            g_sym_id[HVal]   = g_sym_id[Val];   g_sym_id[Val] = ++i;
            parseToken();
            if (g_token == ',') {
              parseToken();
            }
          }
          parseToken();
        }

        *++g_text = ENT;
        *++g_text = i - loc;
        while (g_token != '}') {
          stmt();
        }
        *++g_text = LEV;

        // unwind symbol table locals
        g_sym_id = g_symtab;
        while (g_sym_id[Token]) {
          if (g_sym_id[Class] == Loc) {
            g_sym_id[Class] = g_sym_id[HClass];
            g_sym_id[Type] = g_sym_id[HType];
            g_sym_id[Val] = g_sym_id[HVal];
          }
          g_sym_id = g_sym_id + Idsz;
        }
      } else {
        g_sym_id[Class] = Glo;
        g_sym_id[Val] = (intll)g_data;
        g_data = g_data + sizeof(int);
      }
      if (g_token == ',') {
        parseToken();
      }
    }
    parseToken();
  }

  pc = (intll *)idmain[Val];
  if (!pc) {
    printf("main() not defined\n");
    return -1;
  }
  if (src) {
    return 0;
  }

  // setup stack
  bp = sp = (intll *)((intll)sp + poolsz);
  *--sp = EXIT; // call exit if main returns
  *--sp = PSH;
  t = sp;
  *--sp = argc;
  *--sp = (intll)argv;
  *--sp = (intll)t;

  // run...
  cycle = 0;
  while (1) {
    i = *(pc++);
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
      case JZ:  { pc = a ? pc + 1 : (intll *)*pc; break; }                     // branch if zero
      case JNZ: { pc = !a ? pc + 1 : (intll *)*pc; break; }                     // branch if zero
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
