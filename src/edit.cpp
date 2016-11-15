/*
    This file is part of Genesys.

    Genesys is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Genesys is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Genesys.  If not, see <http://www.gnu.org/licenses/>.

    Copyright (C) 2016 Clemens Kirchgatterer <clemens@1541.org>.
*/

/*
  line.c is probably the smallest still usful editor written in C. It is based
  on ue.c from Terry Loveall, but expanded and modified for this project. The
  original copyright notice can be found below.

  TODO

  * convert \r\n to \n on load and save
  * add help screen with key bindings
  * re-add undo
  * re-add search
  * maybe second status line on bottom
  * fix disapearing top line when starting scrolling
    probalby related to line feed (additional \r) in bottom line

  Key Bindings
  ------------
  CTRL_S ... save file to disk
  CTRL_F ... find string in text (not working)
  CTRL_K ... delete to end of line
  CTRL_B ... go to bottom of file
  CTRL_T ... go to top of file
  CTRL_Q ... exit
*/

/*
  ue.c from ae.c. Public Domain 1991 by Anthony Howe.  All rights released.
  version 1.25, Public Domain, (C) 2002 Terry Loveall, 
  THIS PROGRAM COMES WITH ABSOLUTELY NO WARRANTY OR BINARIES. COMPILE AND USE 
  AT YOUR OWN RISK.
*/

#include <Arduino.h>

#include "filesystem.h"
#include "terminal.h"

#define MAX_FILE_SIZE 1024*4
#define TAB_SIZE           4

class Edit {

public:

  enum {
    STATE_UNCHANGED,
    STATE_CHANGED,
    STATE_SAVED
  };

  Edit(Terminal &term) : term(term) {
    state = STATE_UNCHANGED;
    lines = 1;
    done  = 0;

    buf   = (char *)malloc(MAX_FILE_SIZE);
    ebuf  = buf + MAX_FILE_SIZE;
    etxt  = buf;
    curp  = buf;
    page  = buf;
    epage = buf;

    filename = NULL;
    memset(search, '\0', 32);

    term.ScreenSave();
  }

  ~Edit(void) {
    free(buf);
    free(filename);

    term.ScreenRestore();
  }

private:

  int outx, outy;
  int col,  row;
  int state;
  int lines;
  int done;

  char *buf;
  char *ebuf;
  char *etxt;
  char *curp;
  char *page;
  char *epage;
  char *filename;

  char search[32];
  Terminal &term;

  int width() {
    return (term.Width());
  }

  int height() {
    return (term.Height() - 1); // minus status line
  }

  void gotoxy(int x, int y) {
    term.CursorPosition(x, y + 1);
    outx = x, outy = y;
  }

  void emitch(int c) {
    if (c == '\t') {
      do {
        term.Insert(' ');
        outx++;
      } while (outx&(TAB_SIZE-1));
    } else if (c == '\n') {
      term.LineFeed();
    } else {
      term.Insert(c);
      outx++;
    }

    if (c == '\n') { outx = 0; outy++; };
  }

  void clrtoeol() {
    int n = width() - outx;
    char str[n + 1];

    memset(str, ' ', n);
    str[n] = '\0';
    term.Print(str);
    gotoxy(outx, outy);
  }

  char *prevline(char *p) {
    while ((buf < --p) && (*p != '\n'));

    return ((buf < p) ? ++p : buf);
  }

  char *nextline(char *p) {
    while ((p < etxt) && (*p++ != '\n'));

    return ((p < etxt) ? p : etxt);
  }

  char *adjust(char *p, int column) {
    int i = 0;

    while ((p < etxt) && (*p != '\n') && (i < column)) {
      i += (*p++ == '\t') ? TAB_SIZE - (i&(TAB_SIZE-1)) : 1;
    }

    return (p);
  }

  void left() {
    if (buf < curp) curp--;
  } 

  void right() {
    if (curp < etxt) curp++;
  }

  void up() {
    curp = adjust(prevline(prevline(curp) - 1), col);
  }

  void down() {
    curp = adjust(nextline(curp), col);
  }

  void wleft() {
    while (isspace(*(curp - 1)) && buf < curp) curp--;
    while (!isspace(*(curp - 1)) && buf < curp) curp--;
  }

  void wright() {
    while (!isspace(*curp) && curp < etxt) curp++;
    while (isspace(*curp) && curp < etxt) curp++;
  }

  void pgdown() {
    page = curp = prevline(epage - 1);

    while (0 < row--) down();

    epage = etxt;
  }

  void pgup() {
    int i = height();

    while (0 < --i) {
      page = prevline(page - 1); 
      up();
    }
  }

  void home() {
    curp = prevline(curp);
  }

  void end() {
    curp = nextline(curp);
    left();
  }

  void top() {
    curp = buf;
  }

  void bottom() {
    epage = curp = etxt;
  }

  void cmove(char *src, char *dest, int cnt) {
    if (src > dest){
      while (cnt--) *dest++ = *src++;
    }

    if (src < dest){
      src += cnt;
      dest += cnt;

      while (cnt--) *--dest = *--src;
    }

    etxt += dest - src;
    state = STATE_CHANGED;
  }

  void del() {
    if (curp < etxt) {
      if (*curp == '\n') lines--;
      cmove(curp + 1, curp, etxt - curp);
    }
  }

  void bkspc() {
    if (buf < curp){
      left();
      del();
    }
  }

  void delrol() {
    int l = lines;

    do { del(); } while ((curp < etxt) && (l == lines));
  }

  void find() {
    char c;
    int i;

    i = strlen(search);

    term.CursorPosition(1, 1);
    term.LineClear();

    term.Print(F("Find: %s"), search);

    do {
      c = term.GetKey();

      if (c == '\b'){
        if (!i) continue;
        i--; emitch(c); emitch(' ');
      } else {
        if (i == width()) continue;
        search[i++] = c;
      }

      if (c != 0x1b) emitch(c);
    } while ((c != 0x1b) && (c != '\n') && (c != '\r'));

    search[--i] = 0;

    if (c != 0x1b){
      do {
        right();
      } while ((curp < etxt) && strncmp(curp, search, i));
    }
  }

  void quit() {
    done = 1;
  }

  void status() {
    String str = F("unchanged");
    int w = 0;

    if (state == STATE_CHANGED) {
      str = F("changed");
    } else if (state == STATE_SAVED) {
      str = F("saved");
    }
 
    gotoxy(0, 0);

    term.Color(TERM_REVERSE, TERM_BLUE, TERM_WHITE);

    w += term.Print(F("File: %s (%s)"), filename, str.c_str());
    w += term.Insert(' ', width() - w - 50);
    w += term.Print(F("Size: %i bytes, %i lines"), etxt-buf, lines);
    w += term.Insert(' ', 15);
    w += term.Print(F("Pos: %i,%i"), col + 1, row + 1);
    w += term.Insert(' ', width() - w);

    term.Color(TERM_RESET, TERM_DEFAULT, TERM_DEFAULT);
  }

  void update() {
    int i=0, j=0;

    if (curp < page) page = prevline(curp);

    if (epage <= curp) {
      page = curp; 
      i = height();
      while (1 < i--) page = prevline(page-1);
    }

    epage = page;
    gotoxy(0, 1);

    while (1) {
      if (curp == epage) {
        row = i;
        col = j;
      }

      if (i >= height() || lines <= i || etxt <= epage) break;

      if (*epage == '\n' || width() <= j) {
        ++i;
        j = 0;
        clrtoeol();
      }

      if (*epage != '\r') {
        emitch(*epage);
        j += *epage == '\t' ? TAB_SIZE-(j&(TAB_SIZE-1)) : *epage == '\n' ? 0 : 1;
      }

      ++epage;
    }

    i = outy;
    while (i++ <= height()) {
      clrtoeol();
      gotoxy(1, i);
    }

    status();

    gotoxy(col+1, row+1);
  }

  void save() {
    if (!rootfs) return;

    File f = rootfs->open(filename, "w");

    if (f) {
      f.write((uint8_t *)buf, (int)(etxt - buf));
      f.close();

      state = STATE_SAVED;
    }
  }

public:

  void load(const String &path) {
    free(filename);
    filename = strdup(path.c_str());

    term.ScreenClear();

    if (!rootfs) return;

    File f = rootfs->open(path, "r");

    if (f) {
      etxt += f.read((uint8_t *)buf, MAX_FILE_SIZE);

      if (etxt < buf) {
        etxt = buf;
      } else {
        char *p = etxt;

        while (p > buf) {
          if (*--p == '\n') lines++;
        }
      }

      f.close();
    }

    update();
  }

  int exec() {
    uint8_t c = term.GetKey();

    if (c == TERM_KEY_NONE) return (0);

         if (c == TERM_KEY_LEFT)      left();
    else if (c == TERM_KEY_RIGHT)     right();
    else if (c == TERM_KEY_UP)        up();
    else if (c == TERM_KEY_DOWN)      down();
    else if (c == TERM_KEY_DELETE)    del();
    else if (c == TERM_KEY_BACKSPACE) bkspc();
    else if (c == TERM_KEY_PAGEUP)    pgup();
    else if (c == TERM_KEY_PAGEDOWN)  pgdown();
    else if (c == TERM_KEY_HOME)      home();
    else if (c == TERM_KEY_END)       end();
    else if (c == TERM_KEY_CTRL_B)    bottom();
    else if (c == TERM_KEY_CTRL_F)    find();
    else if (c == TERM_KEY_CTRL_K)    delrol();
    else if (c == TERM_KEY_CTRL_S)    save();
    else if (c == TERM_KEY_CTRL_T)    top();
    else if (c == TERM_KEY_CTRL_Q)    quit();
    else {
      if (etxt < ebuf) {
        cmove(curp, curp + 1, etxt - curp);
        *curp = (c == '\r') ? '\n' : c;
        if (*curp++ == '\n') lines++;
      }
    }

    update();

    if (done) {
      return (-1);
    }

    return (0);
  }

}; // class Edit

void *edit_start(Terminal &term, const String &arg) {
  Edit *edit = new Edit(term);

  edit->load(arg);

  return (edit);
}

void edit_stop(void *ptr) {
  delete ((Edit *)ptr);
}

int edit_exec(void *ptr) {
  Edit *edit = (Edit *)ptr;
 
  return (edit->exec());
}
