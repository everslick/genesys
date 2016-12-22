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

#include "filesystem.h"
#include "terminal.h"
#include "logger.h"
#include "system.h"
#include "module.h"
#include "config.h"
#include "clock.h"
#include "edit.h"
#include "gpio.h"
#include "led.h"
#include "rtc.h"
#include "ntp.h"
#include "net.h"
#include "log.h"

#include "cli.h"

#define MAX_RUNNING_TASKS 5

enum TaskState {
  TASK_STATE_IDLE,
  TASK_STATE_START,
  TASK_STATE_STOP,
  TASK_STATE_EXEC
};

enum TaskID {
  TASK_ID_NONE,
  TASK_ID_EDIT,
  TASK_ID_TOP,
  TASK_ID_C64
};

class Task {

public:

  Task(Terminal &term, TaskID id, int pid, const String &arg) :
       term(term), id(id), pid(pid), arg(arg) {

    state = TASK_STATE_START;
    time  = millis();
    data  = NULL;
  }

  TaskID id;
  TaskState state;
  Terminal &term;
  String arg;
  void *data;
  int time;
  int pid;

};

static Task *task[MAX_RUNNING_TASKS];
static bool initialized = false;

static void cli_task_delete(int pid);

static char *color_str(char buf[], uint8_t col) {
  sprintf_P(buf, PSTR("\033[0;3%im"), col);

  return (buf);
}

static void module_state(String &str, int idx, const String &module) {
  int len, color = COL_RED;
  char col[8];
  int state;

  str += module;
  len = module.length();

  for (int i=0; i<10-len; i++) str += F(" ");

  if (idx == -1) {
    module_call_state(module, state);
  } else {
    module_call_state(idx, state);
  }

  if (state == MODULE_STATE_ACTIVE) {
    color = COL_GREEN;
    str += F("  ");
  }

  str += color_str(col, color);
  str += module_state_str(state);
  str += color_str(col, COL_DEFAULT);
  str += F("\r\n");
}

static void eval_module_call(Terminal &term, const String &mod, const String &act,
                             int return_value, bool module_found) {
  char col[8];
  String str;
  bool ret;

  if (!module_found) {
    if (mod == F("help")) {
      str += color_str(col, COL_GREEN);
      str += F("available modules are:\r\n");

      for (int i=0; i<module_count(); i++) {
        str += F("\t");
        str += module_name(i);
        str += F("\r\n");
      }

      str += color_str(col, COL_DEFAULT);
    } else if ((act == F("state")) && ((mod == "") || (mod == F("all")))) {
      for (int i=0; i<module_count(); i++) {
        module_state(str, i, module_name(i));
      }
    } else if (mod == F("all")) {
      if (act == F("init")) {
        for (int i=module_count(); i>=0; i--) {
          module_call_init(i, ret);
        }
      }

      if (act == F("fini")) {
        for (int i=0; i<module_count(); i++) {
          module_call_fini(i, ret);
        }
      }
    } else {
      str += color_str(col, COL_RED);

      if (mod.length() != 0) {
        str += F("unknown module '");
        str += mod + F("'\r\n");
      }

      str += F("try '");
      str += act + F(" help' to get a list of modules\r\n");
      str += color_str(col, COL_DEFAULT);
    }

    term.Print(str);

    return;
  }

  if (act == F("state")) {
    module_state(str, -1, mod);

    term.Print(str);

    return;
  }

  if (!return_value) {
    str += F("MOD:  could not ");

    if (act == F("init")) str += F("initialize");
    if (act == F("fini")) str += F("finalize");

    str += F(" module '");
    str += mod + F("'\r\n");

    term.Print(str);

    return;
  }
}

static void cpu_turbo(Terminal &term, const String &arg) {
  char col[8];
  String str;

       if (arg == "") str = (system_turbo_get()) ? F("on\r\n") : F("off\r\n");
  else if (arg == F("1") || arg == F("on"))  system_turbo_set(true);
  else if (arg == F("0") || arg == F("off")) system_turbo_set(false);
  else {
    str += color_str(col, COL_RED);
    str += F("unknown argument: ");
    str += arg + F("\r\n");
    str += color_str(col, COL_DEFAULT);
  }

  term.Print(str);
}

static void config_key(Terminal &term, const String &arg) {
  int idx = arg.indexOf('=');
  String key, val;
  char col[8];
  String str;
  bool ok;

  if (arg == "") {
    term.Print(F("conf: missing argument\r\n"));
    return;
  }

  if (idx >= 0) {
    key = arg.substring(0, idx);
    val = arg.substring(idx + 1);

    key.trim();
    val.trim();

    if (val == "") {
      ok = config_clr(key);
    } else {
      ok = config_set(key, val);
    }
  } else {
    key = arg;
    ok = config_get(key, str);
  }

  if (!ok) {
    str += color_str(col, COL_RED);
    str += F("conf: invalid config key: ");
    str += key;
    str += color_str(col, COL_DEFAULT);
  }

  if (str != "") str += F("\r\n");

  term.Print(str);
}

void cat(Terminal &term, const String &arg) {
  if (!rootfs) {
    term.Print(F("cat: filesystem not mounted\r\n"));
    return;
  }

  if (arg == "") {
    term.Print(F("cat: missing argument\r\n"));
    return;
  }

  File f = rootfs->open(arg, "r");

  if (!f) {
    term.Print(F("cat: file not found\r\n"));
    return;
  }

  while (f.available()) {
    char buf[256];

    int n = f.readBytes(buf, sizeof (buf));
    term.tty.write(buf, n);
  }

  f.close();
}

static void mv(Terminal &term, const String &arg) {
  int idx = arg.indexOf(' ');
  String from, to;

  if (!rootfs) {
    term.Print(F("mv: filesystem not mounted\r\n"));
    return;
  }

  if (idx < 0) {
    term.Print(F("mv: missing argument\r\n"));
    return;
  }

  from = arg.substring(0, idx);
  to = arg.substring(idx + 1);

  from.trim();
  to.trim();

  if (!rootfs->rename(from, to)) {
    term.Print(F("mv: file not found\r\n"));
    return;
  }
}

static void rm(Terminal &term, const String &arg) {
  if (!rootfs) {
    term.Print(F("rm: filesystem not mounted\r\n"));
    return;
  }

  if (arg == "") {
    term.Print(F("rm: missing argument\r\n"));
    return;
  }

  if (!rootfs->remove(arg)) {
    term.Print(F("rm: file not found\r\n"));
    return;
  }

}

static void date(Terminal &term, const String &arg) {
  char col[8];
  String str;

  if (arg == "") {
    DateTime dt(clock_time());

    str = dt.str() + F("\r\n");
  } else {
    struct timespec tv;
    DateTime dt(arg);

    if (dt.Valid()) {
      tv.tv_sec  = dt;
      tv.tv_nsec = 0;

      clock_settime(CLOCK_REALTIME, &tv);
    } else {
      str += color_str(col, COL_RED);
      str += F("malformed date string: ");
      str += arg + F("\r\n");
      str += color_str(col, COL_DEFAULT);
    }
  }

  term.Print(str);
}

static void ps(Terminal &term, const String &arg) {
  term.Print(F("PID TTY          TIME CMD\r\n"));

  for (int i=0; i<MAX_RUNNING_TASKS; i++) {
    Task *t = task[i];

    if (t) {
      DateTime dt((millis() - t->time) / 1000);
      String cmd = F("-- ");

      if (t->id == TASK_ID_EDIT) cmd = F("edit ");
      if (t->id == TASK_ID_TOP)  cmd = F("top ");
      if (t->id == TASK_ID_C64)  cmd = F("c64 ");

      cmd += t->arg;

      char tty[10];
      int n = snprintf_P(tty, sizeof (tty), PSTR("tty/%i"), t->term.pty);

      term.Print(F("%3i %s"), t->pid, tty);
      term.Insert(' ', 9 - n);
      term.Print(F("%s %s\r\n"), dt.time_str().c_str(), cmd.c_str());
    }
  }
}

static void kill(Terminal &term, const String &arg) {
  int pid = arg.toInt();

  if ((pid >= 0) && (pid < MAX_RUNNING_TASKS) && task[pid]) {
    task[pid]->state = TASK_STATE_STOP;
  } else {
    term.Print(F("kill: (%i) - No such process"), pid);
  }
}

static void localtime(Terminal &term, const String &arg) {
  DateTime dt(clock_time());

  dt.ConvertToLocalTime();

  term.Print(dt.str() + F("\r\n"));
}

static void adc_read(Terminal &term) {
  term.Print(String(analogRead(17)) + F("\r\n"));
}

static void systohc(Terminal &term, const String &arg) {
  struct timespec tv;

  clock_gettime(CLOCK_REALTIME, &tv);
  rtc_set(&tv);
}

static void uptime(Terminal &term, const String &arg) {
  char uptime[24];
  String str;

  str += system_uptime(uptime);
  str += F("\r\n");

  term.Print(str);
}

static void info(Terminal &term, const String &arg) {
  char col[8];
  String str;

  if (!str.reserve(200)) {
    log_print(F("CON:  failed to allocate memory"));
  }

  if (arg == F("help") || arg == "") {
    str = F("info [i] can be one of:\r\n\t");
    str += F("all, log, device, version, build, ");
    str += F("sys, flash, net, ap, wifi");
    term.Print(str);
  }

  if (arg == F("all") || arg == F("log")) {
    str = "\r\n";
    term.Color(TERM_RED);
    logger_dump_raw(str, -1);
    term.Print(str);
  }

  if (arg == F("all") || arg == F("device")) {
    str = "\r\n";
    term.Color(TERM_MAGENTA);
    system_device_info(str);
    term.Print(str);
  }

  if (arg == F("all") || arg == F("version")) {
    str = "\r\n";
    term.Color(TERM_BLUE);
    system_version_info(str);
    term.Print(str);
  }

  if (arg == F("all") || arg == F("build")) {
    str = "\r\n";
    term.Color(TERM_YELLOW);
    system_build_info(str);
    term.Print(str);
  }

  if (arg == F("all") || arg == F("sys")) {
    str = "\r\n";
    term.Color(TERM_GREEN);
    system_sys_info(str);
    term.Print(str);
  }

  if (arg == F("all") || arg == F("flash")) {
    str = "\r\n";
    term.Color(TERM_CYAN);
    system_flash_info(str);
    term.Print(str);
  }

  if (arg == F("all") || arg == F("net")) {
    str = "\r\n";
    term.Color(TERM_MAGENTA);
    system_net_info(str);
    term.Print(str);
  }

  if (arg == F("all") || arg == F("ap")) {
    str = "\r\n";
    term.Color(TERM_RED);
    system_ap_info(str);
    term.Print(str);
  }

  if (arg == F("all") || arg == F("wifi")) {
    str = "\r\n";
    term.Color(TERM_YELLOW);
    system_wifi_info(str);
    term.Print(str);
  }

  str = "\r\n";
  term.Color(TERM_DEFAULT);
  term.Print(str);
}

static void top(Terminal &term) {
  char uptime[24];

  term.ScreenClear();

  // time and uptime
  DateTime dt(clock_time());
  String line = F("top  ");

  dt.ConvertToLocalTime();
  line += dt.str();
  line += F(" up ");
  line += system_uptime(uptime);
  line += F("\r\n");
  term.Print(line);

  // modules
  int state, active = 0;

  line = F("Mod: ");
  for (int i=0; i<module_count(); i++) {
    module_call_state(i, state);
    if (state == MODULE_STATE_ACTIVE) active++;
  }
  line += module_count();
  line += F(" total, ");
  line += active;
  line += F(" active, ");
  line += module_count() - active;
  line += F(" inactive\r\n");
  term.Print(line);

  // CPU
  line = F("CPU: ");
  line += system_cpu_load();
  line += F("% ");
  line += system_main_loops();
  line += F(" loops/s @ ");
  line += ESP.getCpuFreqMHz();
  line += F("MHz\r\n");
  term.Print(line);

  // memory
  line = F("Mem: ");
  line += system_mem_usage();
  line += F("% ");
  line += system_mem_free();
  line += F(" bytes free heap, ");
  line += system_free_stack();
  line += F(" bytes free stack\r\n");
  term.Print(line);

  // net
  line = F("Net: ");
  line += system_net_traffic();
  line += F("% ");
  line += system_net_xfer();
  line += F(" bytes/s, SSID=");
  line += net_ssid();
  line += F(", RSSI=");
  line += net_rssi();
  line += F("%\r\n\r\n");
  term.Print(line);

  // fourth line
  line = "";
  if (fs_state() == MODULE_STATE_ACTIVE) {
    fs_df(line);
    term.Print(line);
  }

  // seperator
  term.Print(
    F("==============================================================\r\n")
  );

  // log dump
  line = "";
  logger_dump_raw(line, 5);
  term.Print(line);
}

static int exec_top(Task *task) {
  struct Data { uint32_t ms; };
  Terminal &term = task->term;

  if (task->state == TASK_STATE_START) {
    Data *d = new Data();

    d->ms = millis();
    top(term);

    task->state = TASK_STATE_EXEC;
    task->data = d;
  } else if (task->state == TASK_STATE_STOP) {
    delete ((Data *)task->data);
    cli_task_delete(task->pid);

    return (-1);
  } else {
    Data *d = (Data *)task->data;

    if ((millis() - d->ms) > 2500) {
      d->ms = millis();

      top(term);
    }

    while (term.tty.available()) term.tty.read();
  }

  return (task->pid);
}

static int exec_c64(Task *task) {
  struct Data { uint32_t count; uint32_t ms; };
  Terminal &term = task->term;

  if (task->state == TASK_STATE_START) {
    Data *d = new Data();

    d->ms = millis();
    d->count = 3;

    term.Color(TERM_WHITE, TERM_BLUE, TERM_BRIGHT);
    term.ScreenClear();
    term.Center(F("**** COMMODORE 64 BASIC V2 ****"));
    term.LineFeed(2);
    term.Center(F("64K RAM SYSTEM  38911 BASIC BYTES FREE"));
    term.LineFeed(2);

    task->state = TASK_STATE_EXEC;
    task->data = d;
  } else if (task->state == TASK_STATE_STOP) {
    term.Print(F("READY.\r\n"));
    term.Color(TERM_DEFAULT, TERM_DEFAULT, TERM_RESET);

    delete ((Data *)task->data);
    cli_task_delete(task->pid);

    return (-1);
  } else {
    Data *d = (Data *)task->data;

    if ((millis() - d->ms) > 1000) {
      d->ms = millis();

      if (d->count--) {
        term.Print(F("HELLO, WORLD!\r\n"));
      } else {
        task->state = TASK_STATE_STOP;
      }
    }

    while (term.tty.available()) term.tty.read();
  }

  return (task->pid);
}

static int exec_edit(Task *task) {
  if (task->state == TASK_STATE_START) {
    task->data = edit_start(task->term, task->arg);
    task->state = TASK_STATE_EXEC;
  } else if (task->state == TASK_STATE_STOP) {
    edit_stop(task->data);
    cli_task_delete(task->pid);

    return (-1);
  } else {
    if (edit_exec(task->data) == -1) {
      task->state = TASK_STATE_STOP;
    }
  }

  return (task->pid);
}

static int cli_task_new(Terminal &term, TaskID id, const String &arg) {
  int pid = -1;

  // search for free pid
  for (int i=0; i<MAX_RUNNING_TASKS; i++) {
    if (!task[i]) {
      pid = i;
      break;
    }
  }

  // no free pid found
  if (pid == -1) {
    term.Print(F("shell: Resource temporarily unavailable\r\n"));

    return (-1);
  }

  task[pid] = new Task(term, id, pid, arg);;

  return (pid);
}

static void cli_task_delete(int pid) {
  delete (task[pid]);
  task[pid] = NULL;
}

bool cli_init(void) {
  if (initialized) return (false);

  for (int i=0; i<MAX_RUNNING_TASKS; i++) {
    task[i] = NULL;
  }

  initialized = true;

  return (true);
}

bool cli_fini(void) {
  if (!initialized) return (false);

  for (int i=0; i<MAX_RUNNING_TASKS; i++) {
    delete (task[i]);
    task[i] = NULL;
  }

  initialized = false;

  return (true);
}

void cli_kill_task(int pid) {
  if ((pid >= 0) && (pid < MAX_RUNNING_TASKS)) {
    task[pid]->state = TASK_STATE_STOP;
  }
}

int cli_poll_task(int pid) {
  Task *t = NULL;

  if ((pid >= 0) && (pid < MAX_RUNNING_TASKS)) {
    t = task[pid];
  }

  if (t) {
    if (t->id == TASK_ID_EDIT) return (exec_edit(t));
    if (t->id == TASK_ID_TOP)  return (exec_top(t));
    if (t->id == TASK_ID_C64)  return (exec_c64(t));
  }
}

static void add_completion(lined_t *l, const String &str) {
  lined_completion_add(l, str.c_str());
}

static char *add_hint(const String &str) {
  static String hint;

  hint = str;

  return ((char *)hint.c_str());
}

char *cli_hint_cb(const char *buf, int *color, int *bold) {
  String cmd = buf;
  *color = 34;
  *bold = 1;

  if (cmd == F("cat"))    return (add_hint(F(" <file>")));
  if (cmd == F("conf"))   return (add_hint(F(" <key>[=<value>]")));
  if (cmd == F("date"))   return (add_hint(F(" [YYYY/MM/DD HH:MM:SS]")));
  if (cmd == F("fini"))   return (add_hint(F(" <module>")));
  if (cmd == F("flash"))  return (add_hint(F(" <led>")));
  if (cmd == F("high"))   return (add_hint(F(" <gpio>")));
  if (cmd == F("info"))   return (add_hint(F(" <info>|all")));
  if (cmd == F("init"))   return (add_hint(F(" <module>")));
  if (cmd == F("kill"))   return (add_hint(F(" <pid>")));
  if (cmd == F("low"))    return (add_hint(F(" <gpio>")));
  if (cmd == F("mv"))     return (add_hint(F(" <file> <name>")));
  if (cmd == F("off"))    return (add_hint(F(" <led>")));
  if (cmd == F("on"))     return (add_hint(F(" <led>")));
  if (cmd == F("ping"))   return (add_hint(F(" <host>")));
  if (cmd == F("pulse"))  return (add_hint(F(" <led>")));
  if (cmd == F("rm"))     return (add_hint(F(" <file>")));
  if (cmd == F("state"))  return (add_hint(F(" [<module>|all]")));
  if (cmd == F("toggle")) return (add_hint(F(" <gpio>")));
  if (cmd == F("turbo"))  return (add_hint(F(" [0|1]")));

  return (NULL);
}

void cli_completion_cb(lined_t *l, const char *buf) {
  if (buf[0] == 'a') {
    add_completion(l, F("adc"));
  } else if (buf[0] == 'c') {
    add_completion(l, F("cat"));
    add_completion(l, F("clear"));
    add_completion(l, F("conf"));
  } else if (buf[0] == 'd') {
    add_completion(l, F("date"));
    add_completion(l, F("df"));
  } else if (buf[0] == 'f') {
    add_completion(l, F("fini"));
    add_completion(l, F("flash"));
    add_completion(l, F("format"));
  } else if (buf[0] == 'h') {
    add_completion(l, F("help"));
    add_completion(l, F("high"));
  } else if (buf[0] == 'i') {
    add_completion(l, F("init"));
  } else if (buf[0] == 'k') {
    add_completion(l, F("kill"));
  } else if (buf[0] == 'l') {
    add_completion(l, F("localtime"));
    add_completion(l, F("low"));
    add_completion(l, F("ls"));
  } else if (buf[0] == 'm') {
    add_completion(l, F("mv"));
  } else if (buf[0] == 'n') {
    add_completion(l, F("ntp"));
  } else if (buf[0] == 'o') {
    add_completion(l, F("off"));
    add_completion(l, F("on"));
  } else if (buf[0] == 'p') {
    add_completion(l, F("ping"));
    add_completion(l, F("ps"));
    add_completion(l, F("pulse"));
  } else if (buf[0] == 'r') {
    add_completion(l, F("reboot"));
    add_completion(l, F("reset"));
    add_completion(l, F("rm"));
    add_completion(l, F("rtc"));
  } else if (buf[0] == 's') {
    add_completion(l, F("save"));
    add_completion(l, F("scan"));
    add_completion(l, F("state"));
    add_completion(l, F("systohc"));
  } else if (buf[0] == 't') {
    add_completion(l, F("toggle"));
    add_completion(l, F("top"));
    add_completion(l, F("turbo"));
  } else if (buf[0] == 'u') {
    add_completion(l, F("uptime"));
  }
}

int cli_run_command(Terminal &term, const String &line) {
#ifdef ALPHA
  enum { PARSE_CMD, PARSE_ARG, PARSE_END } parse = PARSE_CMD;
  String str, cmd, arg;
  bool exists, ret;
  char col[8];
  int state;

  // handle leading and trailing spaces
  str = line;
  str.trim();

  // split line into command and argument
  for (char c : str) {
    if (parse == PARSE_CMD) {
      if (c == ' ') parse = PARSE_ARG; else cmd += c;
    } else arg += c;
  }

  str = String();

         if (cmd == F("top")) {
    return (cli_task_new(term, TASK_ID_TOP, arg));
  } else if (cmd == F("edit")) {
    return (cli_task_new(term, TASK_ID_EDIT, arg));
  } else if (cmd == F("c64")) {
    return (cli_task_new(term, TASK_ID_C64, arg));
  } else if (cmd == F("ps")) {
    ps(term, arg);
  } else if (cmd == F("kill")) {
    kill(term, arg);
  } else if (cmd == F("info")) {
    info(term, arg);
  } else if (cmd == F("init")) {
    exists = module_call_init(arg, ret);
    eval_module_call(term, arg, F("init"), ret, exists);
  } else if (cmd == F("fini")) {
    exists = module_call_fini(arg, ret);
    eval_module_call(term, arg, F("fini"), ret, exists);
  } else if (cmd == F("state")) {
    exists = module_call_state(arg, state);
    eval_module_call(term, arg, F("state"), state, exists);
  } else if (cmd == F("turbo")) {
    cpu_turbo(term, arg);
  } else if (cmd == F("conf")) {
    config_key(term, arg);
  } else if (cmd == F("save")) {
    config_write();
  } else if (cmd == F("format")) {
    fs_format();
  } else if (cmd == F("ls")) {
    fs_ls(str);
  } else if (cmd == F("cat")) {
    cat(term, arg);
  } else if (cmd == F("rm")) {
    rm(term, arg);
  } else if (cmd == F("df")) {
    fs_df(str);
  } else if (cmd == F("mv")) {
    mv(term, arg);
  } else if (cmd == F("ntp")) {
    ntp_settime();
  } else if (cmd == F("rtc")) {
    rtc_settime();
  } else if (cmd == F("date")) {
    date(term, arg);
  } else if (cmd == F("systohc")) {
    systohc(term, arg);
  } else if (cmd == F("uptime")) {
    uptime(term, arg);
  } else if (cmd == F("localtime")) {
    localtime(term, arg);
  } else if (cmd == F("adc")) {
    adc_read(term);
  } else if (cmd == F("toggle")) {
    gpio_toggle(arg.toInt());
  } else if (cmd == F("high")) {
    gpio_high(arg.toInt());
  } else if (cmd == F("low")) {
    gpio_low(arg.toInt());
  } else if (cmd == F("flash")) {
    led_flash(arg.toInt(), 200);
  } else if (cmd == F("pulse")) {
    led_pulse(arg.toInt(), 300);
  } else if (cmd == F("off")) {
    led_off(arg.toInt());
  } else if (cmd == F("on")) {
    led_on(arg.toInt());
  } else if (cmd == F("ping")) {
    net_ping(arg.c_str());
  } else if (cmd == F("scan")) {
    net_scan_wifi();
    system_wifi_info(str);
  } else if (cmd == F("clear")) {
    term.ScreenClear();
  } else if (cmd == F("reboot")) {
    system_reboot();
  } else if (cmd == F("reset")) {
    config_reset();
  } else if (cmd == F("help")) {
    term.Color(TERM_GREEN);
    term.Print(F("available commands are:\r\n"));
    term.Print(F("\tinit <m>     ... initialize module <m>\r\n"));
    term.Print(F("\tfini <m>     ... finalize module <m>\r\n"));
    term.Print(F("\tstate [m]    ... query state of module [m]\r\n"));
    term.Print(F("\tturbo [0|1]  ... switch cpu turbo mode on or off\r\n"));
    term.Print(F("\tconf <k|k=v> ... get or set config key <k>\r\n"));
    term.Print(F("\tsave         ... save config to EEPROM\r\n"));
    term.Print(F("\tformat       ... create / filesystem\r\n"));
    term.Print(F("\tls           ... list filesystem content\r\n"));
    term.Print(F("\tcat <f>      ... print content of file <f>\r\n"));
    term.Print(F("\trm <f>       ... remove file <f> from filesystem\r\n"));
    term.Print(F("\tmv <f> <t>   ... rename file <f> to file <t>\r\n"));
    term.Print(F("\tdf           ... report file system disk space usage\r\n"));
    term.Print(F("\tntp          ... set system time from ntp server\r\n"));
    term.Print(F("\trtc          ... set system time from RTC\r\n"));
    term.Print(F("\tdate [d]     ... get/set time [YYYY/MM/DD HH:MM:SS]\r\n"));
    term.Print(F("\tsystohc      ... set RTC from system time\r\n"));
    term.Print(F("\tuptime       ... get system uptime\r\n"));
    term.Print(F("\tlocaltime    ... get local time\r\n"));
    term.Print(F("\tadc          ... read ADC value\r\n"));
    term.Print(F("\ttoggle <p>   ... toggle GPIO pin <p>\r\n"));
    term.Print(F("\thigh <p>     ... set GPIO pin <p> high\r\n"));
    term.Print(F("\tlow <p>      ... set GPIO pin <p> low\r\n"));
    term.Print(F("\tflash <l>    ... flash led <l> once\r\n"));
    term.Print(F("\tpulse <l>    ... let led <l> blink\r\n"));
    term.Print(F("\ton <l>       ... switch led <l> on\r\n"));
    term.Print(F("\toff <l>      ... switch led <l> off\r\n"));
    term.Print(F("\tping <h>     ... send 3 ICMP ping requests to host <h>\r\n"));
    term.Print(F("\tscan         ... scan WiFi for available accesspoints\r\n"));
    term.Print(F("\tps           ... list all currently running tasks\r\n"));
    term.Print(F("\tkill <p>     ... terminate the task with PID <p>\r\n"));
    term.Print(F("\ttop          ... show runtime system usage statistics\r\n"));
    term.Print(F("\tc64          ... 'hello, world!' demo\r\n"));
    term.Print(F("\tclear        ... clear screen\r\n"));
    term.Print(F("\treboot       ... reboot device\r\n"));
    term.Print(F("\treset        ... perform factory reset\r\n"));
    term.Print(F("\thelp         ... print this info\r\n"));
    term.Color(TERM_DEFAULT);
  } else {
    if (cmd != "") {
      term.Color(TERM_RED);
      term.Print(F("unknown command '"));
      term.Print(cmd + F("'\r\ntry 'help' to get a list of commands\r\n"));
      term.Color(TERM_DEFAULT);
    }
  }

  term.Print(str);
#endif

  return (-1);
}
