/*** includes ***/

/*#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE*/

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

/* CLIPBOARD */
#include "x11paste.c"

/*** defines ***/

#define TEX_EDITOR_VERSION "0.0.1"
#define TEX_TAB_STOP 1

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT = 1001,
  ARROW_UP = 1002,
  ARROW_DOWN = 1003,
  PAGE_UP = 1004,
  PAGE_DOWN = 1005,
  HOME_KEY = 1006,
  END_KEY = 1007,
  DEL_KEY = 1008,
  CTRL_ARROW_LEFT = 1009,
  CTRL_ARROW_RIGHT = 1010,
  CTRL_ARROW_UP = 1011,
  CTRL_ARROW_DOWN = 1012,
  SHIFT_ARROW_LEFT = 1013,
  SHIFT_ARROW_RIGHT = 1014,
  SHIFT_ARROW_UP = 1015,
  SHIFT_ARROW_DOWN = 1016,
  CTRL_SHIFT_ARROW_LEFT = 1017,
  CTRL_SHIFT_ARROW_RIGHT = 1018
};

enum editorHighlight {
  HL_NORMAL   = 0,
  HL_ERROR   = 1,
  HL_MATH     = 2,
  HL_COMMAND  = 3,
  HL_BRACKET  = 4,
  HL_ARGUMENT = 5,
  HL_ALIGN    = 6
};

/*** data ***/

typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_multiline_math;
} erow;

struct snipp {
  char ** search_words;
  char ** output_words;
  int canAutoComplete;
  int idword;
  int idchr;
  int number_of_words;
};

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;
};

struct editorConfig E;

struct highlightClipBoard {
  char* chars;
  int size;
  int* hx;
  int* hy;
  int dir;
};

struct highlightClipBoard H;

/*** prototypes ***/

void editorProcessKeypress(int c);
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt);
void editorInsertChar(int c);
void editorMoveCursor(int key);
void clearHighlight();

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

 if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
   die("tcsetattr");
 }
}

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c == '\x1b') {
    char seq[5];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == ';') {
          if (read(STDIN_FILENO, &seq[3], 1) != 1) return '\x1b';
          if (seq[3] == '5') {
            if (read(STDIN_FILENO, &seq[4], 1) != 1) return '\x1b';
            switch (seq[4]) {
              case 'A': return CTRL_ARROW_UP;
              case 'B': return CTRL_ARROW_DOWN;
              case 'C': return CTRL_ARROW_RIGHT;
              case 'D': return CTRL_ARROW_LEFT;
            }
          }
          if (seq[3] == '2') {
            if (read(STDIN_FILENO, &seq[4], 1) != 1) return '\x1b';
            switch (seq[4]) {
              case 'A': return SHIFT_ARROW_UP;
              case 'B': return SHIFT_ARROW_DOWN;
              case 'C': return SHIFT_ARROW_RIGHT;
              case 'D': return SHIFT_ARROW_LEFT;
            }
          }
          if (seq[3] == '6') {
            if (read(STDIN_FILENO, &seq[4], 1) != 1) return '\x1b';
            switch (seq[4]) {
              case 'A': return ARROW_UP;
              case 'B': return ARROW_DOWN;
              case 'C': return CTRL_SHIFT_ARROW_RIGHT;
              case 'D': return CTRL_SHIFT_ARROW_LEFT;
            }
          }
        }
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** highlightClipBoard ***/

void highlightInsertCharFront(char c) {
  if (H.size < 0 || H.dir < 0) {
    editorSetStatusMessage("An error has occured inside highlightInsertCharFront");
    return;
  }

  H.chars = (char*)realloc(H.chars, sizeof(char) * (H.size + 1));
  memmove(&H.chars[1], &H.chars[0], H.size);
  H.chars[0] = c;

  if (H.size > 0) {
  H.hx = (int*)realloc(H.hx, sizeof(int) * (H.size));
  H.hx[H.size-1] = E.cx-1;
  H.hy = (int*)realloc(H.hy, sizeof(int) * (H.size));
  H.hy[H.size-1] = E.cy;
  }

  H.size++;
  H.dir++;
}

void highlightRemoveCharFromFront() {
  memmove(&H.chars[0], &H.chars[1], H.size);
  //H.chars = (char*)realloc(H.chars, sizeof(char) * (H.size - 1));
  //memmove(&H.hx[0], &H.hx[1], sizeof(int)*H.size);
  H.size -= 1;
  H.dir -= 1;
}

void highlightRemoveCharFromBack() {
  H.chars[H.size-2] = '\0';
  //H.chars = (char*)realloc(H.chars, sizeof(char) * (H.size - 1));
  H.size -= 1;
  H.dir++;
}


void highlightInsertCharBack(char c) {
  if (H.size < 1 || H.dir > 0) {
    editorSetStatusMessage("An error has occured inside highlightInsertCharBack");
    return;
  }
  H.chars = (char*)realloc(H.chars, sizeof(char) * (H.size + 1));
  H.chars[H.size-1] = c;
  H.chars[H.size] = '\0';

  H.hx = (int*)realloc(H.hx, sizeof(int) * (H.size));
  H.hx[H.size-1] = E.cx;
  H.hy = (int*)realloc(H.hy, sizeof(int) * (H.size));
  H.hy[H.size-1] = E.cy;


  H.size++;
  H.dir -= 1;
}

void clearHighlight() {
  H.size = 0;
  H.dir = 0;
  highlightInsertCharFront('\0');
  H.dir = 0;
  H.hx = NULL;
  H.hy = NULL;
}

int highlightMark(int ry, int rx){
  if (H.hx == NULL || H.hy == NULL) {
    return 0;
  }
  for (int i = 0; i < H.size-1; i++) {
    if (H.hx[i] == rx && H.hy[i] == ry) {
      return 1;
    }
  }
  return 0;
}

/*** snippets and auto-complete***/

char * key_commands_search_words[] = {(char*)"Delta", (char*)"Gamma", (char*)"Lambda", (char*)"Omega", (char*)"Phi", (char*)"Pi", (char*)"Psi", (char*)"Sigma", (char*)"Theta", (char*)"Upsilon", (char*)"Xi", (char*)"alpha", (char*)"bar", (char*)"begin", (char*)"beta", (char*)"bigbreak", (char*)"bigcap", (char*)"bigcup", (char*)"bigsqcup", (char*)"cap", (char*)"cdot", (char*)"chi", (char*)"cos", (char*)"cup", (char*)"ddot", (char*)"delta", (char*)"dot", (char*)"emptyset", (char*)"end", (char*)"eta", (char*)"exists", (char*)"forall", (char*)"frac", (char*)"gamma", (char*)"geq", (char*)"hat", (char*)"iiiint", (char*)"iiint", (char*)"iint", (char*)"infty", (char*)"int", (char*)"iota", (char*)"kappa", (char*)"lambda", (char*)"land", (char*)"langle", (char*)"left", (char*)"leq", (char*)"lim", (char*)"limits", (char*)"lor", (char*)"mathbb", (char*)"mathcal", (char*)"mathnormal", (char*)"mathrm", (char*)"mp", (char*)"mu", (char*)"noindent", (char*)"not", (char*)"nu", (char*)"omega", (char*)"overline", (char*)"phi", (char*)"pi", (char*)"pm", (char*)"prod", (char*)"psi", (char*)"quad", (char*)"rangle", (char*)"rho", (char*)"right", (char*)"setminus", (char*)"sigma", (char*)"sin", (char*)"sqcup", (char*)"subseteq", (char*)"subsetneq", (char*)"sum", (char*)"supseteq", (char*)"supsetneq", (char*)"tau", (char*)"text", (char*)"textbf", (char*)"textit", (char*)"theta", (char*)"tilde", (char*)"times", (char*)"upsilon", (char*)"varepsilon", (char*)"varphi", (char*)"vec", (char*)"xi", (char*)"zeta", (char*)"in", (char*)"to", (char*)"iff", (char*)"ldots", (char*)"section", (char*)"documentclass", (char*)"usepackage", (char*)"newtheorem", (char*)"pagestyle", (char*)"fancyhf", (char*)"rhead", (char*)"lhead", (char*)"rfoot", (char*)"today", (char*)"thepage", (char*)"theoremstyle", (char*)"unlhd", (char*)"cong", (char*)"circ", (char*)"mapsto", (char*)"vline", (char*)"item", (char*)"sup", (char*)"inf", (char*)"widehat", (char*)"widetilde", (char*)",", (char*)"xrightarrow", (char*)"oplus", (char*)"otimes", (char*)"bigoplus", (char*)"bigotimes", (char*)"bigwedge", (char*)"arrow", (char*)"sim", (char*)"simeq", (char*)"choose", (char*)"title", (char*)"author", (char*)"maketitle", (char*)"geometry",  (char*)"bibitem",  (char*)"cdots",  (char*)"dots",  (char*)"bullet",  (char*)"subsection",  (char*)"wedge",  (char*)"partial",  (char*)"mathfrak",  (char*)"nabla", (char*)"label", (char*)"ref", (char*)"cite", (char*)"sqrt", (char*)"newenvironment", (char*)"renewcommand", (char*)"innerexercise", (char*)"theinnerexercise", (char*)"endinnerexercise", (char*)"log", (char*)"equiv", NULL };

char * key_commands_output_words[] = {(char*)"Delta", (char*)"Gamma", (char*)"Lambda", (char*)"Omega", (char*)"Phi", (char*)"Pi", (char*)"Psi", (char*)"Sigma", (char*)"Theta", (char*)"Upsilon", (char*)"Xi", (char*)"alpha", (char*)"bar{}\x12", (char*)"begin{}\r \r\\end{}\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12", (char*)"beta", (char*)"bigbreak", (char*)"bigcap", (char*)"bigcup", (char*)"bigsqcup", (char*)"cap", (char*)"cdot", (char*)"chi", (char*)"cos", (char*)"cup", (char*)"ddot", (char*)"delta", (char*)"dot", (char*)"emptyset", (char*)"end{}\x12", (char*)"eta", (char*)"exists", (char*)"forall", (char*)"frac{}{}\x12\x12\x12", (char*)"gamma", (char*)"geq", (char*)"hat{}\x12", (char*)"iiiint", (char*)"iiint", (char*)"iint", (char*)"infty", (char*)"int", (char*)"iota", (char*)"kappa", (char*)"lambda", (char*)"land", (char*)"langle", (char*)"left", (char*)"leq", (char*)"lim", (char*)"limits", (char*)"lor", (char*)"mathbb{}\x12", (char*)"mathcal{}\x12", (char*)"mathnormal{}\x12", (char*)"mathrm{}\x12", (char*)"mp", (char*)"mu", (char*)"noindent", (char*)"not", (char*)"nu", (char*)"omega", (char*)"overline{}\x12", (char*)"phi", (char*)"pi", (char*)"pm", (char*)"prod", (char*)"psi", (char*)"quad", (char*)"rangle", (char*)"rho", (char*)"right", (char*)"setminus", (char*)"sigma", (char*)"sin", (char*)"sqcup", (char*)"subseteq", (char*)"subsetneq", (char*)"sum", (char*)"supseteq", (char*)"supsetneq", (char*)"tau", (char*)"text{}\x12", (char*)"textbf{}\x12", (char*)"textit{}\x12", (char*)"theta", (char*)"tilde{}", (char*)"times", (char*)"upsilon", (char*)"varepsilon", (char*)"varphi", (char*)"vec{}\x12", (char*)"xi", (char*)"zeta",(char*)"in", (char*)"to", (char*)"iff", (char*)"ldots", (char*)"section",  (char*)"documentclass{}", (char*)"usepackage{}", (char*)"newtheorem{}", (char*)"pagestyle{}", (char*)"fancyhf{}", (char*)"rhead{}", (char*)"lhead{}", (char*)"rfoot{}", (char*)"today", (char*)"thepage", (char*)"theoremstyle{}", (char*)"unlhd",  (char*)"cong", (char*)"circ", (char*)"mapsto", (char*)"vline", (char*)"item", (char*)"sup", (char*)"inf", (char*)"widehat{}", (char*)"widetilde{}", (char*)",", (char*)"xrightarrow{}\x12", (char*)"oplus", (char*)"otimes", (char*)"bigoplus", (char*)"bigotimes", (char*)"bigwedge", (char*)"arrow{}{}\x12\x12\x12", (char*)"sim", (char*)"simeq", (char*)"choose", (char*)"title{}\x12", (char*)"author{}\x12", (char*)"maketitle", (char*)"geometry",  (char*)"bibitem{}\x12",  (char*)"cdots",  (char*)"dots", (char*)"bullet",  (char*)"subsection{}\x12",  (char*)"wedge",  (char*)"partial", (char*)"mathfrak{}\x12",  (char*)"nabla", (char*)"label{}\x12", (char*)"ref{}\x12", (char*)"cite{}\x12", (char*)"sqrt{}\x12", (char*)"newenvironment{}\x12", (char*)"renewcommand", (char*)"innerexercise", (char*)"theinnerexercise{}\x12", (char*)"endinnerexercise", (char*)"log", (char*)"equiv", NULL };



struct snipp key_commands = {
  key_commands_search_words,
  key_commands_output_words,
  0,
  0,
  0,
  154, //number of commands.
};

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/=~%<>[];", c) != NULL;
}

int is_end_of_word(int c){
  return isspace(c) || c == '\0' || strchr("\\_.,()+-/*=^|~%<>[]:;!$&{}/?", c) != NULL;
}

/* to be called after we press tab,
if there is a word to be autocompleted */
void autoComplete(snipp snippet){
  for (size_t i = snippet.idchr; i < strlen(snippet.output_words[snippet.idword]); i++) {
    editorProcessKeypress(snippet.output_words[snippet.idword][i]);
  }
  key_commands.canAutoComplete = 0;
  E.statusmsg_time = time(NULL) - 5;
}

void detectSnippet(erow* row){
//  erow row = E.row[E.cy];
  key_commands.canAutoComplete = 0;
  E.statusmsg_time = time(NULL) - 5;
  if (E.cx == 0) {
    return;
  }
  int i = E.cx;

  int len_of_word = 0;

  while (i > 0) {
    i -= 1;
    if (is_end_of_word(row->render[i])) {
      break;
    } else {
      len_of_word++;
    }
  }

  /* check if the word can be autocompleted*/
  if (row->render[i] == '\\' && len_of_word > 1 && (is_end_of_word(row->render[E.cx]) || E.cx == row->size)) {
    int found_word = 0;
    int j = 0;
    while(found_word == 0 && j < key_commands.number_of_words) {
      found_word = 1;
      for (int k = 1; k < len_of_word+1; k++) {
        if (row->render[k+i] != key_commands.search_words[j][k-1]) {
          found_word = 0;
          break;
        }
      }
      j++;
    }
    if (found_word) {
      editorSetStatusMessage("Press tab to autocomplete: %s ",key_commands.output_words[j-1]);
      key_commands.canAutoComplete = 1;
      key_commands.idword = j-1;
      key_commands.idchr = len_of_word;
      /*for (int k = 0; k < len_of_word; k++) {
        row->hl[i+k] = HL_MATH;
      }*/
    }
  }
}

/* Duplicates actions from \begin{} to \end{}*/
void duplicateChar(int c, void (*action)(erow*, int, int)) {
  /* If cx is being preceeded by \begin{word and next character is }*/
  erow* row = &E.row[E.cy];
  int i = E.cx;
  if (row->render[i] == '}') {
    while (i > 0) {
      i -= 1;
      if (is_separator(row->render[i]) || row->render[i] == '\\') {
        break;
      }
    }
    if (row->render[i] == '\\' && row->render[i+1] == 'b' && row->render[i+2] == 'e'
        && row->render[i+3] == 'g' && row->render[i+4] == 'i'
        && row->render[i+5] == 'n' && row->render[i+6] == '{') {
      /* find \end{ then find } and write there*/
      for (int j = E.cy; j < E.numrows; j++) {
        erow *searchrow = &E.row[j];
        char *match = strstr(searchrow->render, "\\end{");
        if (match) {
          //editorSetStatusMessage(match - row->render);
          action(searchrow, match - searchrow->render + E.cx -7 + 4 - i, c);
          break;
        }
      }
    }
  }
}

void duplicateDelete(void (*action)(erow*, int)) {
  /* If cx is being preceeded by \begin{word and next character is }*/
  erow* row = &E.row[E.cy];
  int i = E.cx;
  if (row->render[i] == '}') {
    while (i > 0) {
      i -= 1;
      if (is_separator(row->render[i]) || row->render[i] == '\\') {
        break;
      }
    }
    if (row->render[i] == '\\' && row->render[i+1] == 'b' && row->render[i+2] == 'e'
        && row->render[i+3] == 'g' && row->render[i+4] == 'i'
        && row->render[i+5] == 'n' && row->render[i+6] == '{') {
      /* find \end{ then find } and write there*/
      for (int j = E.cy; j < E.numrows; j++) {
        erow *searchrow = &E.row[j];
        char *match = strstr(searchrow->render, "\\end{");
        if (match) {
          editorSetStatusMessage("Test");
          action(searchrow, match - searchrow->render + E.cx -7 + 5 - i);
          break;
        }
      }
    }
  }
}

/*** syntax highlighting ***/

void spellRowCheck(erow* row) {
  int i = row->size;

  int len_of_word = 1;
  while (i > 0) {
    i -= 1;
    if (is_end_of_word(row->render[i]) || i == 0) {
      //len_of_word++;
      /* check if the word is a command */
      if (row->render[i] == '\\') {
        if (row->render[i+1] == '{' || row->render[i+1] == '}' ||
        (row->render[i+1] == '\\' && !(row->render[i+1] == '\\' && (i > 0 && row->render[i-1] == '\\'))) ||
        (i > 0 && row->render[i-1] == '\\' && !(row->render[i+1] == '\\' && (i > 0 && row->render[i-1] == '\\'))) ) {
      } else {
          int found_word = 0;
          int j = 0;
          while(found_word == 0 && j < key_commands.number_of_words) {
            found_word = 1;
            int k = 1;
            while (!is_end_of_word(row->render[i+k]) && i+k < row->size) {
              if (row->render[k+i] != key_commands.search_words[j][k-1]) {
                found_word = 0;
                break;
              }
              k++;
            }
            if (key_commands.search_words[j][k-1] != '\0' && (i+k != E.cx || row != &E.row[E.cy])) {
              found_word = 0;
            }
            j++;
          }
          if (!found_word) {
            for (int k = 0; k < len_of_word; k++) {
              row->hl[i + k] = HL_ERROR;
            }
          }
        }
      }
      len_of_word = 1;
    } else {
      len_of_word++;
    }
  }
}

void spellCheck() {
  for (int i = 0; i < E.numrows; i++) {
    spellRowCheck(&E.row[i]);
  }
}

int is_end_of_command(int c) {
  return isspace(c) || c == '\0' || strchr("_{}()[]%", c) != NULL;
}

int is_bracket(int c) {
    return strchr("[]{}&", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
  row->hl = (unsigned char*)realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);

  int in_mathmode = 0;
  int in_displaymode = 0;
  int in_command = 0;
  int in_curly_bracket = 0;
  int in_align = 0;
  int in_align_star = 0;
  if (row->idx > 0) {
    if (E.row[row->idx - 1].hl_multiline_math % 2 == 0) {
      in_align = 1;
    }
    if (E.row[row->idx - 1].hl_multiline_math % 3 == 0) {
      in_align_star = 1;
    }
    if (E.row[row->idx - 1].hl_multiline_math % 5 == 0) {
      in_mathmode = 1;
    }
    if (E.row[row->idx - 1].hl_multiline_math % 7 == 0) {
      in_displaymode = 1;
    }
  }

  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
//    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : (int)HL_NORMAL;

    if (in_command) {
      if (is_end_of_command(c)) {
        in_command = 0;
      } else {
        row->hl[i] = HL_COMMAND;
      }
    } else if (!in_mathmode && c=='\\') {
      row->hl[i] = HL_COMMAND;
      in_command = 1;
      continue;
    }

    if (in_curly_bracket) {
      if (c == (char)'}') {
        in_curly_bracket -= 1;
      } else {
        row->hl[i] = HL_ARGUMENT;
      }
    } else if (!in_mathmode && c == (char)'{') {
      in_curly_bracket += 1;
    }

    if (in_align) {
      if ( i+10 < row->rsize ) {
        if (row->render[i] == '\\' && row->render[i+1] == 'e' && row->render[i+2] == 'n' &&
            row->render[i+3] == 'd' && row->render[i+4] == '{' && row->render[i+5] == 'a' &&
            row->render[i+6] == 'l' && row->render[i+7] == 'i' && row->render[i+8] == 'g' &&
            row->render[i+9] == 'n' && row->render[i+10] == '}') {
          in_align = 0;
          row->hl[i] = HL_COMMAND;
          row->hl[i+1] = HL_COMMAND;
          row->hl[i+2] = HL_COMMAND;
          row->hl[i+3] = HL_COMMAND;
          row->hl[i+4] = HL_BRACKET;
          row->hl[i+5] = HL_ARGUMENT;
          row->hl[i+6] = HL_ARGUMENT;
          row->hl[i+7] = HL_ARGUMENT;
          row->hl[i+8] = HL_ARGUMENT;
          row->hl[i+9] = HL_ARGUMENT;
          row->hl[i+10] = HL_BRACKET;
          i += 10;
          continue;
        } else {
          row->hl[i] = HL_ALIGN;
        }
      } else {
        /* Color the rest of the row*/
        row->hl[i] = HL_ALIGN;
      }
    } else {
      if (c == (char)'}' && i < 14) {
        if (row->render[i-1] == 'n' && row->render[i-2] == 'g' && row->render[i-3] == 'i' &&
            row->render[i-4] == 'l' && row->render[i-5] == 'a' && row->render[i-6] == '{' &&
            row->render[i-7] == 'n' && row->render[i-8] == 'i' && row->render[i-9] == 'g' &&
            row->render[i-10] == 'e' && row->render[i-11] == 'b' && row->render[i-12] == (char)'\\') {
          in_align = 1;
        }
      }
    }

    if (in_align_star) {
      if ( i+11 < row->rsize ) {
        if (row->render[i] == '\\' && row->render[i+1] == 'e' && row->render[i+2] == 'n' &&
            row->render[i+3] == 'd' && row->render[i+4] == '{' && row->render[i+5] == 'a' &&
            row->render[i+6] == 'l' && row->render[i+7] == 'i' && row->render[i+8] == 'g' &&
            row->render[i+9] == 'n' && row->render[i+10] == '*' && row->render[i+11] == '}') {
          in_align_star = 0;
          row->hl[i] = HL_COMMAND;
          row->hl[i+1] = HL_COMMAND;
          row->hl[i+2] = HL_COMMAND;
          row->hl[i+3] = HL_COMMAND;
          row->hl[i+4] = HL_BRACKET;
          row->hl[i+5] = HL_ARGUMENT;
          row->hl[i+6] = HL_ARGUMENT;
          row->hl[i+7] = HL_ARGUMENT;
          row->hl[i+8] = HL_ARGUMENT;
          row->hl[i+9] = HL_ARGUMENT;
          row->hl[i+10] = HL_ARGUMENT;
          row->hl[i+11] = HL_BRACKET;
          i += 11;
          continue;
        } else {
          row->hl[i] = HL_ALIGN;
        }
      } else {
        /* Color the rest of the row*/
        row->hl[i] = HL_ALIGN;
      }
    } else {
      if (c == (char)'}' && i < 15) {
        if (row->render[i-1] == '*' && row->render[i-2] == 'n' && row->render[i-3] == 'g' && row->render[i-4] == 'i' &&
            row->render[i-5] == 'l' && row->render[i-6] == 'a' && row->render[i-7] == '{' &&
            row->render[i-8] == 'n' && row->render[i-9] == 'i' && row->render[i-10] == 'g' &&
            row->render[i-11] == 'e' && row->render[i-12] == 'b' && row->render[i-13] == (char)'\\') {
          in_align_star = 1;
        }
      }
    }

    if (in_displaymode) {
      row->hl[i] = HL_MATH;
      if (c == '\\' && i + 1 < row->rsize) {
        row->hl[i + 1] = HL_MATH;
        i++;
      } else if (c == (char)'$' && (i + 1 < row->rsize && row->render[i+1] == (char)'$')) {
        row->hl[i + 1] = HL_MATH;
        i++;
        in_displaymode = 0;
      } else if (c == (char)'$' && (i + 1 == row->rsize || row->render[i+1] != (char)'$')) {
        row->hl[i] = HL_ERROR;
      }
    } else {
      if (c==(char)'$' && (i + 1 < row->rsize && row->render[i+1] == (char)'$')) {
        if (i == 0 || row->render[i-1] != (char)'\\') {
          row->hl[i] = HL_MATH;
          row->hl[i+1] = HL_MATH;
          i++;
          in_displaymode = 1;
        }
      }
    }

    if (in_mathmode) {
      row->hl[i] = HL_MATH;
      if (c == '\\' && i + 1 < row->rsize) {
        row->hl[i + 1] = HL_MATH;
        i += 1;
      } else if (c == (char)'$') {
        in_mathmode = 0;
      }
    } else if (!in_displaymode){
      if (c==(char)'$' && (i == row->rsize || row->render[i+1] != (char)'$')) {
        if (i == 0 || (row->render[i-1] != (char)'\\' && row->render[i-1] != (char)'$')) {
          row->hl[i] = HL_MATH;
          in_mathmode = 1;
        }
      }
    }

    if (is_bracket(c)) {
      row->hl[i] = HL_BRACKET;
    }

    i++;
  }

  row->hl_multiline_math = (6 * in_displaymode + 1)* (4 * in_mathmode + 1)
                            * (1 * in_align + 1) * (2 * in_align_star + 1);
  //int changed = (row->hl_multiline_math != i(4 * in_mathmode + 1) * (1 * in_align + 1) * (2 * in_align_star + 1));
  if (row->idx + 1 < E.numrows) {
    editorUpdateSyntax(&E.row[row->idx + 1]);
  }
  //spellRowCheck(row);
}

int editorSyntaxToColor(int hl) {
  switch (hl) {
    case HL_ALIGN:
    case HL_MATH: return 32;
    case HL_BRACKET: return 35;
    case HL_COMMAND: return 36;
    case HL_ARGUMENT: return 33;
    case HL_ERROR: return 31;
    default: return 37;
  }
}

/*** row operations ***/

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TEX_TAB_STOP - 1) - (rx % TEX_TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;

  free(row->render);
  row->render = (char*)malloc(row->size + tabs*(TEX_TAB_STOP - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % TEX_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;

  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;

  E.row = (erow*)realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  for (int j = at + 1; j <= E.numrows; j++) E.row[j].idx++;

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = (char*)malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_multiline_math = 0;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  for (int j = at; j < E.numrows - 1; j++) E.row[j].idx--;
  E.numrows--;
  if (at < E.numrows) {
    editorUpdateSyntax(&E.row[at]);
  }
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = (char*)realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = (char*)realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}


/*** editor operations ***/

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, (char*)"", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
  /* duplicates in next \begin if*/
  duplicateChar(c, editorRowInsertChar);
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, (char*)"", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  if (E.cy == E.numrows) return;
  if (E.cx == 0 && E.cy == 0) return;
  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
  duplicateDelete(editorRowDelChar);

}

/*** file i/o ***/
char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;
  char *buf = (char*)malloc(totlen);
  char *p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

char figureNames[255];

void readFigureNames() {
  char command[100];
  strcpy(command, "unzip -p ");
  strcat(command, E.filename);
  strcat(command, " figurenames.dat");

  FILE* fp = popen(command, "r");
  if (!fp) die("fopen");

  strcpy(figureNames, "");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    strcat(figureNames, line);
  }
  free(line);
  pclose(fp);
}

void addFigureName(char* figurename) {
  strcat(figureNames, figurename);
  strcat(figureNames, "\n");
}

void writeFigureNames(){
  int len = strlen(figureNames);
  int fd = open("/tmp/TeX-edit/figurenames.dat", O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, figureNames, len) == len) {
        close(fd);
      }
    }
  }
}

FILE* getTexFile(char* filename){
  char command[100];
  strcpy(command, "unzip -p ");
  strcat(command, filename);
  strcat(command, " main.tex");
  return popen(command, "r");
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);
 
  // If filename does not exists, create the file from a template.
  char command[555];
  strcpy(command, "if [ ! -f "); 
  strcat(command, filename);
  strcat(command, " ]; then ");
  strcat(command, "cp /usr/local/src/TeX-edit/template.pdf ");
  strcat(command, filename);
  strcat(command, "; fi");
  system(command);

  FILE *fp = getTexFile(filename);
  if (!fp) die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  pclose(fp);
  E.dirty = 0;
}

void openTemplate() {
  char templatepath[] = "/usr/local/src/TeX-edit/template.pdf";
  FILE *fp = getTexFile(templatepath);
  if (!fp) die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  pclose(fp);
  E.dirty = 0;
}

void compileOnly() {
  char command[200];
  strcpy(command, "TeX-edit-compile ");
  strcat(command, E.filename);
  strcat(command, " >/dev/null 2>&1");
  system(command);
}

void viewTeXLog() {
  char command[100];
  strcpy(command, "unzip -p ");
  strcat(command, E.filename);
  strcat(command, " main.log");
  FILE *fp = popen(command, "r");
  if (!fp) die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    if (line[0] == '!') {
      editorSetStatusMessage(line);
      editorRefreshScreen();
      int c = editorReadKey();
      while ((linelen = getline(&line, &linecap, fp)) != -1 && c != 13) {
        editorSetStatusMessage(line);
        editorRefreshScreen();
        c = editorReadKey();
      }
      editorSetStatusMessage("");
      free(line);
      pclose(fp);
      return;
    }
  }
  editorSetStatusMessage("No compile errors found.");
  free(line);
  pclose(fp);
  return;
}

void readTeXLog() {
  FILE *fp = popen("cat /tmp/TeX-edit/main.log", "r");
  if (!fp) die("fopen");
  char *line = NULL;
  char *line2 = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  ssize_t linelen2;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    if (strncmp(line, "LaTeX Warning: ", 14) == 0) {
      line[linelen - 1] = '\0';
      editorSetStatusMessage(line);
      free(line);
      pclose(fp);
      return;
    }
    if (strncmp(line, "! Missing ", 9) == 0) {
      line[linelen - 1] = ' ';
      if ((linelen2 = getline(&line2, &linecap, fp)) != -1) {
        line2[linelen2 - 1] = '\0';
        strcat(line, line2);
        editorSetStatusMessage(line);
      } else {
              editorSetStatusMessage("Unknown Compiler Error");
      }
      free(line);
      pclose(fp);
      return;
    }
    if (strncmp(line, "! Extra ", 7) == 0) {
      line[linelen - 1] = ' ';
      if ((linelen2 = getline(&line2, &linecap, fp)) != -1) {
        line2[linelen2 - 1] = '\0';
        strcat(line, line2);
        editorSetStatusMessage(line);
      } else {
        editorSetStatusMessage("Unknown Compiler Error");
      }
      free(line);
      pclose(fp);
      return;
    }
    if (strncmp(line, "! Emergency stop.", 14) == 0) {
      line[linelen - 1] = ' ';
      if ((linelen2 = getline(&line2, &linecap, fp)) != -1) {
        line2[linelen2 - 1] = '\0';
        strcat(line, line2);
        editorSetStatusMessage(line);
    } else {
      editorSetStatusMessage("Unknown Compiler Error");
    }
      free(line);
      pclose(fp);
      return;
    }
    if (strncmp(line, "!  ==> Fatal error occurred, no output PDF file produced!", 30) == 0) {
      line[linelen - 1] = '\0';
      editorSetStatusMessage(line);
      free(line);
      pclose(fp);
      return;
    }
  }
  editorSetStatusMessage("Compiled succesfully.");
  free(line);
  pclose(fp);
}

void editorSave() {
  char command[255];

  if (E.filename == NULL) {
    E.filename = editorPrompt((char*)"Save as: %s");
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      system("touch /tmp/TeX-edit/main.tex");
      return;
    }
  } else {
    strcpy(command, "unzip -o ");
    strcat(command, E.filename);
    strcat(command, "-d /tmp/TeX-edit/ >/dev/null 2>&1");
    system(command);
    strcpy(command, "cp ");
    strcat(command, E.filename);
    strcat(command, " /tmp/TeX-edit/main.pdf >/dev/null 2>&1");
    system(command);
  }

  //Overwrites /tmp/TeX-edit/figurenames.dat
  writeFigureNames();

  //Overwrites /tmp/TeX-edit/main.tex with current file.
  int len;
  char *buf = editorRowsToString(&len);
  int fd = open("/tmp/TeX-edit/main.tex", O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        editorSetStatusMessage("%d bytes written to disk", len);
        system("pdflatex --shell-escape -output-directory=/tmp/TeX-edit/ /tmp/TeX-edit/main.tex >/dev/null 2>&1");

        /* Check for errors in main.log */
        readTeXLog();


        char command2[2000];
        char command3[2000];
        char listoffig[2000];
        strcpy(command2, "zip -j /tmp/TeX-edit/main.zip /tmp/TeX-edit/main.tex /tmp/TeX-edit/main.aux /tmp/TeX-edit/main.log /tmp/TeX-edit/figurenames.dat");
        strcpy(listoffig, " ");
        char *curfig = figureNames;
        while (curfig) {
          char * nextfig = strchr(curfig, '\n');
          if (nextfig) *nextfig = '\0';
          strcat(listoffig, "/tmp/TeX-edit/");
          strcat(listoffig, curfig);
          strcat(listoffig, " ");
          if (nextfig) *nextfig = '\n';
          curfig = nextfig ? (nextfig+1) : NULL;
        }
        strcat(command2, listoffig);
        strcat(command2, ">/dev/null 2>&1");
        system(command2);
        strcpy(command3, "rm /tmp/TeX-edit/main.log /tmp/TeX-edit/main.aux /tmp/TeX-edit/main.tex /tmp/TeX-edit/figurenames.dat");
        strcat(command3, listoffig);
        strcat(command3, " >/dev/null 2>&1");
        system(command3);
        system("rm -r /tmp/TeX-edit/svg-inkscape/ >/dev/null 2>&1");


        FILE* pipeout;
        strcpy(command, "python3 /usr/local/src/TeX-edit/polyglot/truepolyglot pdfzip --pdffile /tmp/TeX-edit/main.pdf --zipfile /tmp/TeX-edit/main.zip ");
        strcat(command, E.filename);
        strcat(command, " >/dev/null 2>&1");
        pipeout = popen(command, "r"); /* TODO: Add error handling + solve issue with stdout flickering on screen */
        pclose(pipeout);
        system("rm /tmp/TeX-edit/main.pdf /tmp/TeX-edit/main.zip");
        E.dirty = 0;

        return;
      }
    }
    close(fd);
  }
  free(buf);
  system("rm /tmp/TeX-edit/main.log /tmp/TeX-edit/main.aux /tmp/TeX-edit/main.tex figurenames.dat");
  system("rm -r /tmp/TeX-edit/svg-inkscape/ >/dev/null 2>&1");

  //Remove .svg files in figurename.

  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

void openInkscape() {
  //Create empty inkscape file.
  char command[255];
  char *filenameSVG;
  filenameSVG = editorPrompt((char*)"(Remember to usepackage{svg}) Name of figure: %s");
  strcpy(command, "inkscape --actions='file-new:/usr/share/inkscape/templates/LaTeX_Beamer.svg;export-filename:");
  strcat(command, filenameSVG);
  strcat(command, ";export-do'");
  strcat(command, " >/dev/null 2>&1");
  system(command);

  //Write the latex code to include a figure
  char texCode[255];
  strcpy(texCode, "\\includesvg{");
  strcat(texCode, filenameSVG);
  strcat(texCode, "}");
  for (size_t i = 0; i < strlen(texCode); i++) {
    editorProcessKeypress(texCode[i]);
  }

  //Open the new file in inkscape.
  char command2[255];
  strcpy(command2, "inkscape ");
  strcat(command2, filenameSVG);
  strcat(command2, " >/dev/null 2>&1");
  system(command2);

  addFigureName(filenameSVG);
}

void openPDF() {
  char command[255];

  strcpy(command, "xdg-open ");
  strcat(command, E.filename);
  strcat(command, " >/dev/null 2>&1");
  system(command);
}

/*** append buffer ***/
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new_char = (char*)realloc(ab->b, ab->len + len);
  if (new_char == NULL) return;
  memcpy(&new_char[ab->len], s, len);
  ab->b = new_char;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "TeX editor -- version %s", TEX_EDITOR_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      char *c = &E.row[filerow].render[E.coloff];
      unsigned char *hl = &E.row[filerow].hl[E.coloff];
      int current_color = -1;
      int j;
      for (j = 0; j < len; j++) {
        if (iscntrl(c[j])) {
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);
          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
            abAppend(ab, buf, clen);
          }
        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          if (highlightMark(filerow, E.coloff + j) ) {
            abAppend(ab, "\x1b[7m", 4);
            abAppend(ab, &c[j], 1);
            abAppend(ab, "\x1b[m", 3);
            if (current_color != -1) {
              char buf[16];
              int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
              abAppend(ab, buf, clen);
            }
          } else {
            abAppend(ab, &c[j], 1);
          }
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }
          if (highlightMark(filerow, E.coloff + j) ) {
            abAppend(ab, "\x1b[7m", 4);
            abAppend(ab, &c[j], 1);
            abAppend(ab, "\x1b[m", 3);
            if (current_color != -1) {
              char buf[16];
              int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
              abAppend(ab, buf, clen);
            }
          } else {
            abAppend(ab, &c[j], 1);
          }
        }
      }
      abAppend(ab, "\x1b[39m", 5);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows,
    E.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
    E.cy + 1, E.numrows);
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {

  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                            (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}


/*** input ***/

void editorPaste(){
  display = XOpenDisplay(0);
  int N = DefaultScreen(display);
  window = XCreateSimpleWindow(display, RootWindow(display, N), 0, 0, 1, 1, 0,
    BlackPixel(display, N), WhitePixel(display, N)
  );
  char* c = XPaste();
  int i = 0;
  while (c[i] != '\0') {
    editorProcessKeypress(c[i]);
    i++;
  }
  //printf("%s\n", XPaste());
}

void editorCopy(char* chars) {
  int count = 0;
  int size = strlen(chars);
  for (int i = 0; i < size; i++) {
    if (chars[i] == '\\') {
      count++;
    }
    if (chars[i] == 39) {
      count = count + 4;
    }
  }
  char* temp = (char*)malloc(sizeof(char) * (size + count + 2));
  int j = 0;
  for (int i = 0; i < size; i++) {
    temp[i + j] = chars[i];
    if (chars[i] == '\\') {
      j++;
      temp[i + j] = chars[i];
    }
    if (chars[i] == 39) {
      j++;
      temp[i + j] = '"';
      j++;
      temp[i + j] = 39;
      j++;
      temp[i + j] = '"';
      j++;
      temp[i + j] = 39;
    }
  }
  temp[size+count] = '\0';

  char* command = (char*)malloc(sizeof(char) * (strlen(temp) + 35));
  strcpy(command, "echo '");
  strcat(command, temp);
  strcat(command, "' | xsel --clipboard --input");
  if (system(NULL)) {
    int k = system(command);
    if (k == 0) {
      editorSetStatusMessage("Copy succesfull.");
    }
  } else {
    editorSetStatusMessage("I/O error occured.");
  }
  /*
  free(temp);
  free(command);*/
  return;
}

char *editorPrompt(char *prompt) {
  size_t bufsize = 128;
  char *buf = (char*)malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      free(buf);
      return NULL;
    } else if (c == '\r') {
        editorSetStatusMessage("");
        return buf;
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = (char*)realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
  }
}

void swapRow(int a , int b){
  /* Temporary variables */
  int tempsize = E.row[a].size;
  char* tempchars = E.row[a].chars;

  /* Paste E.row[E.cy] into E.row[E.cy-1]*/
  E.row[a].size = E.row[b].size;
  E.row[a].chars = (char*)malloc(E.row[b].size + 1);
  memcpy(E.row[a].chars, E.row[b].chars, E.row[b].size);
  E.row[a].chars[E.row[b].size] = '\0';

  E.row[a].rsize = 0;
  E.row[a].render = NULL;
  E.row[a].hl = NULL;
  E.row[a].hl_multiline_math = 0;
  editorUpdateRow(&E.row[a]);

  /* Paste old E.row[E.cy-1] into E.row[E.cy]*/
  E.row[b].size = tempsize;
  E.row[b].chars = (char*)malloc(tempsize + 1);
  memcpy(E.row[b].chars, tempchars, tempsize);
  E.row[b].chars[tempsize] = '\0';

  E.row[b].rsize = 0;
  E.row[b].render = NULL;
  E.row[b].hl = NULL;
  E.row[b].hl_multiline_math = 0;
  editorUpdateRow(&E.row[b]);
  }

void editorShiftMoveCursor(int key) {
  switch (key) {
    case SHIFT_ARROW_LEFT:
      if (H.dir < 0) {
        highlightRemoveCharFromBack();
      } else if (E.cx <= 0) {
        if (E.cy > 0) {
          highlightInsertCharFront('\r');
        }
      } else if(E.cx - 1 < E.row[E.cy].size) {
        highlightInsertCharFront(E.row[E.cy].render[E.cx-1]);
      }
      editorMoveCursor(ARROW_LEFT);
      break;

    case SHIFT_ARROW_RIGHT:
      if (H.dir > 0) {
        highlightRemoveCharFromFront();
      } else if (E.cx >= E.row[E.cy].size) {
        highlightInsertCharBack('\r');
      } else if(E.cx >= 0) {
        highlightInsertCharBack(E.row[E.cy].render[E.cx]);
      }
      editorMoveCursor(ARROW_RIGHT);
      break;

    case SHIFT_ARROW_UP:
      if (E.cy == 0) {
        while (E.cx > 0) {
          editorShiftMoveCursor(SHIFT_ARROW_LEFT);
        }
      } else if (E.cy <= E.numrows) {
        int temp = E.cx;
        while (E.cx > 0) {
          editorShiftMoveCursor(SHIFT_ARROW_LEFT);
        }
        editorShiftMoveCursor(SHIFT_ARROW_LEFT);
        while (E.cx > temp) {
          editorShiftMoveCursor(SHIFT_ARROW_LEFT);
        }
      }
      break;

      case SHIFT_ARROW_DOWN:
        if (E.cy+1 == E.numrows) {
          while (E.cx < E.row[E.cy].size) {
            editorShiftMoveCursor(SHIFT_ARROW_RIGHT);
          }
        } else if ((E.cy >= 0) && (E.cy + 1 < E.numrows)) {
          int temp = E.cx;
          while (E.cx < E.row[E.cy].size) {
            editorShiftMoveCursor(SHIFT_ARROW_RIGHT);
          }
          editorShiftMoveCursor(SHIFT_ARROW_RIGHT);
          while (E.cx < E.row[E.cy].size && E.cx < temp) {
            editorShiftMoveCursor(SHIFT_ARROW_RIGHT);
          }
        }
        break;

    default:
    {
      editorSetStatusMessage("An error has occured. Wrong use of editorShiftMoveCursor.");
      break; 
    }
  }
}

void editorCtrlMoveCursor(int key) {
  switch (key) {
    case CTRL_ARROW_LEFT:
    {
      editorProcessKeypress(ARROW_LEFT);
      while (E.cx > 0 && E.row[E.cy].render[E.cx-1] == ' ') {
        editorProcessKeypress(ARROW_LEFT);
      }
      while (E.cx > 0 && E.row[E.cy].render[E.cx-1] != ' ') {
        editorProcessKeypress(ARROW_LEFT);
      }
      break;
    }
    case CTRL_ARROW_RIGHT:
    {
      editorProcessKeypress(ARROW_RIGHT);
      while (E.cx < E.row[E.cy].size && E.row[E.cy].render[E.cx] == ' ') {
        editorProcessKeypress(ARROW_RIGHT);
      }
      while (E.cx < E.row[E.cy].size && E.row[E.cy].render[E.cx] != ' ') {
        editorProcessKeypress(ARROW_RIGHT);
      }
      break;
    }
    case CTRL_ARROW_UP:
    {
      if (E.cy > 0 && E.cy <= E.numrows) {
        if (E.cy == E.numrows) {
          editorInsertNewline();
          editorMoveCursor(ARROW_UP);
        }

        swapRow(E.cy, E.cy-1);
        E.cy -= 1;
      }
      break;
    }
    case CTRL_ARROW_DOWN:
    {
      if (E.cy < E.numrows) {
        if (E.cy == E.numrows - 1) {
          int ctemp = E.cx;
          E.cx = 0;
          editorInsertNewline();
          E.cx = ctemp;
        } else if (0 <= E.cy){
          swapRow(E.cy, E.cy+1);
          E.cy++;
        }
      }
      break;
    }
    default:
    {
      editorSetStatusMessage("An error has occured. Wrong use of editorCtrlMoveCursor.");
      break;
    }
  }
  E.dirty++;
}

void editorCtrlShiftMoveCursor(int key) {
  switch (key) {
    case CTRL_SHIFT_ARROW_LEFT:
    {
      editorShiftMoveCursor(SHIFT_ARROW_LEFT);
      while (E.cx > 0 && E.row[E.cy].render[E.cx-1] == ' ') {
        editorShiftMoveCursor(SHIFT_ARROW_LEFT);
      }
      while (E.cx > 0 && E.row[E.cy].render[E.cx-1] != ' ') {
        editorShiftMoveCursor(SHIFT_ARROW_LEFT);
      }
      break;
    }
    case CTRL_SHIFT_ARROW_RIGHT:
    {
      editorShiftMoveCursor(SHIFT_ARROW_RIGHT);
      while (E.cx < E.row[E.cy].size && E.row[E.cy].render[E.cx] == ' ') {
        editorShiftMoveCursor(SHIFT_ARROW_RIGHT);
      }
      while (E.cx < E.row[E.cy].size && E.row[E.cy].render[E.cx] != ' ') {
        editorShiftMoveCursor(SHIFT_ARROW_RIGHT);
      }
      break;
    }
    default:
    {
      editorSetStatusMessage("An error has occured. Wrong use of editorCtrlShiftMoveCursor.");
      break;
    }
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy  < E.numrows ) {
        E.cy++;
      }
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress(int c) {
  if (c < 0) {
    return;
  }
  switch (c) {
    case '\r':
      editorInsertNewline();
      spellCheck();
      break;

    case CTRL_KEY('A'):
      break;

    case CTRL_KEY('B'):
      break;

    case CTRL_KEY('C'):
      if (H.size < 2) {
        editorSetStatusMessage("Nothing to copy.");
      } else  {
        editorCopy(H.chars);
      }
      break;

    case CTRL_KEY('D'):
      clearHighlight();
      break;

    case CTRL_KEY('E'):
      compileOnly();
      break;

    case CTRL_KEY('F'):
      break;

    case CTRL_KEY('G'):
      break;

    case CTRL_KEY('J'):
      /* Leave empty since Ctrl-J is sometimes pasted in on Ctrl-C Ctrl-V*/
      break;

    case CTRL_KEY('K'):
      openInkscape();
      break;

    case CTRL_KEY('L'):
    viewTeXLog();
      break;

    case CTRL_KEY('N'):
      break;

    case CTRL_KEY('O'):
      openPDF();
      break;

    case CTRL_KEY('P'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('Q'):
      if (E.dirty == 0) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
      } else {
        editorSetStatusMessage("File has unsaved changes. Press Ctrl-P to force quit or Ctrl-S to save.");
      }
      break;

    case CTRL_KEY('R'):
      editorMoveCursor(ARROW_LEFT);
      break;

    case CTRL_KEY('S'):
      editorSave();
      break;

    case CTRL_KEY('T'):
      break;

    case CTRL_KEY('U'):
      break;

    case CTRL_KEY('V'):
      editorPaste();
      break;

    case CTRL_KEY('W'):
      break;

    case CTRL_KEY('X'):
      if (H.size < 2) {
        editorSetStatusMessage("Nothing to copy.");
      } else  {
        editorCopy(H.chars);
      }
      editorProcessKeypress(BACKSPACE);
      break;

    case CTRL_KEY('Y'):
      break;

    case CTRL_KEY('Z'):
      break;

    case CTRL_ARROW_UP:
    case CTRL_ARROW_DOWN:
    case CTRL_ARROW_LEFT:
    case CTRL_ARROW_RIGHT:
      editorCtrlMoveCursor(c);
      clearHighlight();
      spellCheck();
      break;

    case SHIFT_ARROW_UP:
    case SHIFT_ARROW_DOWN:
    case SHIFT_ARROW_LEFT:
    case SHIFT_ARROW_RIGHT:
      editorShiftMoveCursor(c);
      break;

    case CTRL_SHIFT_ARROW_LEFT:
    case CTRL_SHIFT_ARROW_RIGHT:
      editorCtrlShiftMoveCursor(c);
      break;

    case '\t':
      if (key_commands.canAutoComplete) {
        autoComplete(key_commands);
      } else {
        editorInsertChar(c);
      }
      spellCheck();
      clearHighlight();
      break;

    case HOME_KEY:
      E.cx = 0;
      clearHighlight();
      break;

    case END_KEY:
      if (E.cy < E.numrows) {
        E.cx = E.row[E.cy].size;
        clearHighlight();
      }
      break;

    case BACKSPACE:
    case CTRL_KEY('H'):
    case DEL_KEY:
      if (H.size == 1) {
        if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
      } else if (H.size > 1) {
        if (H.dir < 0) {
          for (int i = 0; i < H.size - 1; i++) {
            editorDelChar();
          }
        } else if (H.dir > 0) {
          for (int i = 0; i < H.size - 1; i++) {
            editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
          }
        }
      }
      clearHighlight();
      detectSnippet(&E.row[E.cy]);
      spellCheck();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      clearHighlight();
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      detectSnippet(&E.row[E.cy]);
      spellCheck();
      clearHighlight();
      break;

    case '\x1b':
      break;
    case 29:
      break;

    default:
      editorInsertChar(c);
      detectSnippet(&E.row[E.cy]);
      spellCheck();
      clearHighlight();
      break;
  }
}

/*** init ***/
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;

  H.size = 0;
  highlightInsertCharFront('\0');
  H.hx = NULL;
  H.hy = NULL;
  H.dir = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 2;
}

void handleResize(int unused) {
  (void)unused;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 2;
  editorRefreshScreen();

}

int main(int argc, char *argv[]) {

  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
    spellCheck();
    readFigureNames();
  } else {
    openTemplate();
  }
  
  system("mkdir /tmp/TeX-edit >/dev/null 2>&1");
  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

  signal(SIGWINCH, handleResize);

  while (1) {
    editorRefreshScreen();
    int c = editorReadKey();
    editorProcessKeypress(c);
  }

  return 0;

}
