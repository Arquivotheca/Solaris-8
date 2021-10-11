/*
** Dell Unix setcolor utility.
** craig jones - 12/21/88
**
** color and ANSI output string definitions.
*/
#ident	"@(#)colors.h	1.2 - 89/07/31"

#define BLACK	   0
#define BLUE	   1
#define GREEN	   2
#define CYAN	   3
#define RED	   4
#define MAGENTA    5
#define BROWN	   6
#define WHITE	   7
#define GRAY	   8|BLACK
#define LBLUE	   8|BLUE
#define LGREEN	   8|GREEN
#define LCYAN	   8|CYAN
#define LRED	   8|RED
#define LMAGENTA   8|MAGENTA
#define YELLOW	   8|BROWN
#define LWHITE	   8|WHITE

#define SET_FG	   "\033[=%dF\033[m"
#define SET_BG	   "\033[=%dG\033[m"
#define SET_BOTH   "\033[=%dF\033[=%dG\033[m"

#define SET_RFG    "\033[=%dH\033[m"
#define SET_RBG    "\033[=%dI\033[m"
#define SET_RBOTH  "\033[=%dH\033[=%dI\033[m"

#define SET_GFG    "\033[=%dJ\033[m"
#define SET_GBG    "\033[=%dK\033[m"
#define SET_GBOTH  "\033[=%dJ\033[=%dK\033[m"

#define SET_BORDER "\033[=%dA"
#define SET_CURSOR "\033[=%d;%dC"
#define SET_PITCH  "\033[=%d;%dB"

#define SGR	   "\033[%dm"
#define SGR_RESET  "\033[0m"
#define SGR_BOLD   "\033[1m"
#define BG_BOLD    "\033[=0E"
#define ERASE_EOD  "\033[J"

#define INITIALIZE "\033[=0E"

