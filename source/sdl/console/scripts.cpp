#include "raine.h"
#include "games.h"
#include "files.h"
#include "scripts.h"
#include "console.h"
#include "dialogs/messagebox.h"
#include "profile.h"
#include "gui/tconsole.h"

#define MAX_PARAM 190

extern int active_if_script_section();

typedef struct {
    int argc;
    char **argv;
    void (*cmd)(int, char **);
} tparsed;

typedef struct {
  char *title;
  // status : 0 = off, 1 = on (in dialog)
  int status, hidden;
  int changing; // keeps last value selected
  char **on, **off, **run, **change;
  char *on_lua, *off_lua, *run_lua, *change_lua;
  int nb_param;
  int value_list[MAX_PARAM];
  char *value_list_label[MAX_PARAM];
  char *comment;
  tparsed *parsed;
  int lua;
} tscript;

tscript *script;
int nb_scripts;

static char *get_script_name(int writeable) {
  char base[10];
   if (is_neocd())
       strcpy(base,"neocd");
   else
       strcpy(base,"raine");
  char buf[FILENAME_MAX];
  snprintf(buf,FILENAME_MAX,"scripts%s%s%s%s.txt",SLASH,base,SLASH,current_game->main_name);

  char *s = get_shared(buf);
  if (!exists(s) && is_neocd()) {
      snprintf(buf,FILENAME_MAX,"scripts%s%s%s%s.txt",SLASH,"raine",SLASH,current_game->main_name);
      s = get_shared(buf);
  }

#if 0
  // don't try to use the parent for now, with all the new cheats from mame we have cheats for everything
  // and when it's not the case it's better to avoid to assume the parent has the same cheats (pacman25 !)
  if (!exists(s)) {
      snprintf(buf,FILENAME_MAX,"scripts%s%s%s%s.txt",SLASH,base,SLASH,parent_name());
      s = get_shared(buf);
  }
#endif
  if (!writeable)
    return s;
  // if it must be writable, force the use of the personnal folder, and
  // create the dirs by the way
  static char shared[1024];
  snprintf(shared,1024, "%sscripts", dir_cfg.exe_path);
  mkdir_rwx(shared);
  snprintf(shared+strlen(shared),1024-strlen(shared),"%s%s",SLASH,base);
  mkdir_rwx(shared);
  snprintf(shared+strlen(shared),1024-strlen(shared),"%s%s.txt",SLASH,current_game->main_name);
  return shared;
}

char *get_script_comment(int n) {
    if (script[n].comment)
	return script[n].comment;
    return "";
}

int atoh(char *s) {
    int val = 0;
    int plus;
    while (*s) {
	if (*s >= '0' && *s <= '9')
	    plus = (*s-'0');
	else if (*s >= 'A' && *s <= 'F')
	    plus = (*s-'A'+10);
	else if (*s >= 'a' && *s <= 'f')
	    plus = (*s-'a'+10);
	else
	    break;
	val *= 16;
	val += plus;
	s++;
    }
    return val;
}

void done_scripts() {
  if (nb_scripts) {
    for (int n=0; n<nb_scripts; n++) {
      free(script[n].title);
      if (script[n].parsed) {
	  int nb = 0;
	  for ( nb=0; script[n].run[nb]; nb++);
	  for (int a=0; a<nb; a++) {
	      for (int x=0; x<script[n].parsed[a].argc; x++)
		  free(script[n].parsed[a].argv[x]);
	      free(script[n].parsed[a].argv);
	  }
	  free(script[n].parsed);
      }
      if (script[n].run) {
	  for (char **l = script[n].run; l && *l; l++)
	      free(*l);
	  free(script[n].run);
      }
      if (script[n].on) {
	  for (char **l = script[n].on; l && *l; l++)
	      free(*l);
	  free(script[n].on);
      }
      if (script[n].off) {
	  for (char **l = script[n].off; l && *l; l++)
	      free(*l);
	  free(script[n].off);
      }
      if (script[n].change) {
	  for (char **l = script[n].change; l && *l; l++)
	      free(*l);
	  free(script[n].change);
      }
      if (script[n].run_lua) free(script[n].run_lua);
      if (script[n].on_lua) free(script[n].on_lua);
      if (script[n].off_lua) free(script[n].off_lua);
      if (script[n].change_lua) free(script[n].change_lua);
      if (script[n].nb_param && script[n].value_list_label[0]) {
	  for (int x=0; x<script[n].nb_param; x++)
	      free(script[n].value_list_label[x]);
      }
      if (script[n].comment) free(script[n].comment);
    }
    free(script);
    script = NULL;
    nb_scripts = 0;
  }
}

void init_scripts() {
  done_scripts();
  // rb should allow fseek in windoze...
  FILE *f = fopen(get_script_name(0),"rb");
  int nb_alloc = 0;
  const int size_cr = 1; // I thought about 2 for windows, but apparently not ?
  if (f) {
      char buff[10240];
      *buff = 0;
    while (!feof(f)) {
	if (strncmp(buff,"script",6) && strncmp(buff,"hidden",6) && strncmp(buff,"luascript",9))  // if containing a script line then it's just a loop, don't erase it !
	    myfgets(buff,10240,f);
	while (*buff && buff[strlen(buff)-1] == '\\')
	    myfgets(buff+strlen(buff)-1,10240-strlen(buff)+1,f);
	char *com = strstr(buff,"\"com");
	if (com) { // comments can be on multiple lines !!!
	    while (!strchr(com+1,'"')) {
		strcat(buff,"\n");
		myfgets(buff+strlen(buff),10240-strlen(buff),f);
	    }
	}
	if (!strncmp(buff,"script \"",8) ||
		!strncmp(buff,"hidden \"",8) ||
		!strncmp(buff,"luascript \"",11)) {
	    if (nb_scripts == nb_alloc) {
		nb_alloc += 10;
		script = (tscript *)realloc(script,sizeof(tscript)*nb_alloc);
	    }
	    script[nb_scripts].hidden = !strncmp(buff,"hidden",6);
	    script[nb_scripts].lua = !strncmp(buff,"luascript",9);
	    script[nb_scripts].on =
		script[nb_scripts].off =
		script[nb_scripts].run =
		script[nb_scripts].change = NULL;
	    script[nb_scripts].on_lua =
		script[nb_scripts].off_lua =
		script[nb_scripts].run_lua =
		script[nb_scripts].change_lua = NULL;
	    script[nb_scripts].changing = 0;
	    int argc;
	    char *argv[200];
	    char **margv = argv;
	    split_command(buff,argv,&argc,200,1); // there are about 130 levels to choose from for pbobble2 !
	    if (argc < 2) {
		printf("init_scripts: script without title: %s\n",buff);
		fclose(f);
		return;
	    }
	    script[nb_scripts].title = strdup(argv[1]);
	    script[nb_scripts].parsed = NULL;
	    script[nb_scripts].comment = NULL;
	    script[nb_scripts].status = 0;
	    script[nb_scripts].nb_param = 0;
	    if (argc > 2 && !strncmp(argv[2],"comm:",5)) {
		script[nb_scripts].comment = strdup(argv[2]+5);
		margv = &argv[1];
		argc--;
	    }
	    if (argc > 2) { // param needed for script
		if (!strncmp(margv[2],"inter=",6)) {
		    char *s = margv[2]+6;
		    for (int n=0; n<=1; n++) {
			char *e = strchr(s,',');
			if (e) {
			    *e = 0;
			    script[nb_scripts].value_list[n] = atoi(s);
			    *e = ','; s=e+1;
			} else {
			    printf("syntax error in inter=\n");
			    fclose(f);
			    return;
			}
		    }
		    script[nb_scripts].value_list[2] = atoi(s);
		    script[nb_scripts].nb_param = 3;
		    script[nb_scripts].value_list_label[0] = NULL;
		} else {
		    // in this case all the remaining arguments are in the format value/description
		    script[nb_scripts].nb_param = argc - 2;
		    if (argc - 2 > MAX_PARAM) {
			fatal_error("too many arguments for %s (%d)",buff,argc-2);
		    }
		    for (int n=0; n<argc-2; n++) {
			char *s = strchr(margv[n+2],'/');
			if (!s) {
			    printf("script: argument wrong format, expected value/description\n");
			    fclose(f);
			    return;
			}
			*s = 0;
			int val;
			if (!strncasecmp(margv[n+2],"0x",2)) {
			    val = atoh(margv[n+2]+2);
			} else {
			    val = atoi(margv[n+2]);
			}
			script[nb_scripts].value_list[n] = val;
			script[nb_scripts].value_list_label[n] = strdup(s+1);
		    }
		}
	    }

	    int pos = ftell(f);
	    int nb_on = 0, nb_off = 0, nb_run = 0, nb_change = 0;
	    int size_on = 0, size_off = 0, size_run = 0, size_change = 0;
	    bool on = false, off = false, change = false, run = false;
	    while (!feof(f)) {
		myfgets(buff,10240,f);
		while (*buff && buff[strlen(buff)-1] == '\\')
		    myfgets(buff+strlen(buff)-1,10240-strlen(buff)+1,f);
		int n=0;
		// skip spaces, tabs, and comments
		while (buff[n] == ' ' || buff[n] == 9)
		    n++;
		if (buff[n] == 0 || !strncmp(&buff[n],"luascript",9) || !strncmp(&buff[n],"script",6) || !strncmp(&buff[n],"hidden",6)) // optional empty line to end the script
		    break;
		if (!strcmp(&buff[n],"on:")) {
		    on = true;
		    off = run = change = false;
		    continue;
		} else if (!strcmp(&buff[n],"off:")) {
		    off = true;
		    on = run = change = false;
		    continue;
		} else if (!strcmp(&buff[n],"change:")) {
		    change = true;
		    on = off = run = false;
		    continue;
		} else if (!strcmp(&buff[n],"run:")) {
		    run = true;
		    on = off = change = false;
		    continue;
		}

		if (on) {
		    nb_on++;
		    size_on += strlen(buff)+size_cr;
		} else if (off) {
		    nb_off++;
		    size_off += strlen(buff)+size_cr;
		} else if (run) {
		    nb_run++;
		    size_run += strlen(buff)+size_cr;
		}
		else if (change) {
		    nb_change++;
		    size_change += strlen(buff) + size_cr;
		}
	    }
	    fseek(f,pos,SEEK_SET);
	    int l = 0;
	    if (script[nb_scripts].lua) {
		if (nb_on) {
		    script[nb_scripts].on_lua = (char*)malloc(size_on+1);
		}
		if (nb_off) {
		    script[nb_scripts].off_lua = (char*)malloc(size_off+1);
		}
		if (nb_run) {
		    script[nb_scripts].run_lua = (char*)malloc(size_run+1);
		}
		if (nb_change) {
		    script[nb_scripts].change_lua = (char*)malloc(size_change+1);
		}
	    } else {
		if (nb_on) {
		    script[nb_scripts].on = (char**)malloc(sizeof(char*)*(nb_on+1));
		    script[nb_scripts].on[nb_on] = NULL;
		}
		if (nb_off) {
		    script[nb_scripts].off = (char**)malloc(sizeof(char*)*(nb_off+1));
		    script[nb_scripts].off[nb_off] = NULL;
		}
		if (nb_run) {
		    script[nb_scripts].run = (char**)malloc(sizeof(char*)*(nb_run+1));
		    script[nb_scripts].run[nb_run] = NULL;
		}
		if (nb_change) {
		    script[nb_scripts].change = (char**)malloc(sizeof(char*)*(nb_change+1));
		    script[nb_scripts].change[nb_change] = NULL;
		}
	    }

	    char **lines = NULL;
	    char *file = NULL;
	    while (!feof(f)) {
		myfgets(buff,10240,f);
		while (*buff && buff[strlen(buff)-1] == '\\')
		    myfgets(buff+strlen(buff)-1,10240-strlen(buff)+1,f);
		int n=0;
		// skip spaces, tabs, and comments
		while (buff[n] == ' ' || buff[n] == 9)
		    n++;
		if (buff[n] == 0 || !strncmp(&buff[n],"luascript",9) || !strncmp(&buff[n],"script",6) || !strncmp(&buff[n],"hidden",6)) // optional empty line to end the script
		    break;
		if (script[nb_scripts].lua) {
		    if (!strcmp(&buff[n],"on:")) {
			file = script[nb_scripts].on_lua;
			l = 0;
			continue;
		    } else if (!strcmp(&buff[n],"off:")) {
			file = script[nb_scripts].off_lua;
			l = 0;
			continue;
		    } else if (!strcmp(&buff[n],"change:")) {
			file = script[nb_scripts].change_lua;
			l = 0;
			continue;
		    } else if (!strcmp(&buff[n],"run:")) {
			file = script[nb_scripts].run_lua;
			l = 0;
			continue;
		    }
		    if (!file) {
			raine_mbox("alert","cheats file in the wrong format, please update ! (luascript)","ok");
			fclose(f);
			return;
		    }
		    sprintf(&file[l],"%s\n",buff);
		    l += strlen(buff)+size_cr;
		} else {
		    if (!strcmp(&buff[n],"on:")) {
			lines = script[nb_scripts].on;
			l = 0;
			continue;
		    } else if (!strcmp(&buff[n],"off:")) {
			lines = script[nb_scripts].off;
			l = 0;
			continue;
		    } else if (!strcmp(&buff[n],"change:")) {
			lines = script[nb_scripts].change;
			l = 0;
			continue;
		    } else if (!strcmp(&buff[n],"run:")) {
			lines = script[nb_scripts].run;
			l = 0;
			continue;
		    }
		    if (!lines) {
			raine_mbox("alert","cheats file in the wrong format, please update !","ok");
			fclose(f);
			return;
		    }
		    lines[l++] = strdup(&buff[n]);
		}
	    }
	    nb_scripts++;
	} else { // script line
	    run_console_command(buff); // pre-init, usually for variables
	}
    } // feof
    fclose(f);
  } // if (f)
  set_nb_scripts(nb_scripts);
}

static int running,nb_script,sline,parsing;
static char *section;

int get_running_script_info(int *nb, int *line, char **sect) {
    if (running) {
	*nb = nb_script;
	*line = sline;
	*sect = section;
	return 1;
    }
    return 0;
}

static void run_off_section(int n) {
    if (script[n].off) {
	running++;
	section = "off";
	for (sline=0; script[n].off[sline]; sline++)
	    run_console_command(script[n].off[sline]);
	running--;
    } else if (script[n].off_lua) {
	char *argv[2];
	section = "off";
	argv[0] = "luascript";
	argv[1] = script[n].off_lua;
	do_lua(2,argv);
    }
}

static void run_script(int n) {
    // It's annoying, in case of error in an on: clause then the error can be caught only here so we must duplicate the try...catch system !
    try {
	nb_script = n;
	if (!cons)
	    run_console_command("");
	if (script[n].change) {
	    running++;
	    section = "change";
	    for (sline=0; script[n].change[sline]; sline++)
		run_console_command(script[n].change[sline]);
	    running--;
	    return;
	} else if (script[n].change_lua) {
	    running++;
	    section = "change";
	    char *argv[2];
	    argv[0] = "luascript";
	    argv[1] = script[n].change_lua;
	    do_lua(2,argv);
	    running--;
	}

	if (!script[n].status) {
	    if (!script[n].off & !script[n].run) {
		// no off, nor run, nor changing -> it's a set script, try on in this case !
		if (script[n].on) {
		    running++;
		    section = "on";
		    for (sline=0; script[n].on[sline]; sline++)
			run_console_command(script[n].on[sline]);
		    running--;
		} else if (script[n].on_lua) {
		    running++;
		    section = "on";
		    char *argv[2];
		    argv[0] = "luascript";
		    argv[1] = script[n].on_lua;
		    do_lua(2,argv);
		    running--;
		}
		return;
	    }
	    run_off_section(n);
	    return;
	}
	if (script[n].on) {
	    running++;
	    section = "on";
	    for (sline=0; script[n].on[sline]; sline++)
		run_console_command(script[n].on[sline]);
	    running--;
	} else if (script[n].on_lua) {
	    running++;
	    section = "on";
	    char *argv[2];
	    argv[0] = "luascript";
	    argv[1] = script[n].on_lua;
	    do_lua(2,argv);
	    running--;
	}
    }
    catch(ConsExcept &e) {
	if (cons && cons->is_visible()) cons->print(e.what());
	else {
	    char msg[240];
	    int nb,line;
	    char *section;
	    if (get_running_script_info(&nb,&line,&section)) {
		// Line numbers are unprecise because lines with only a comment are jumped
		if (script[nb].lua)
		    sprintf(msg,"script: %s\nsection: %s\nLUA\n\n",
			    get_script_title(nb),
			    section);
		else
		    sprintf(msg,"script: %s\nsection: %s\nline: %d\n\n",
			    get_script_title(nb),
			    section,
			    line+1);
		strncat(msg, e.what(),240-strlen(msg));
		stop_script(nb);
		strncat(msg,"\n(script stopped)",240-strlen(msg));
		raine_mbox("script error",msg,"ok");
	    } else
		raine_mbox("script error",e.what(),"ok");
	    reset_ingame_timer();
	}
	script[n].status = 0;
    }
    catch(const char *msg) {
	if (cons && cons->is_visible()) cons->print(msg);
	else {
	    raine_mbox("script error",(char *)msg,"ok");
	}
	script[n].status = 0;
    }
}

static int activate_cheat(int n) {
    int nb_hidden = 0;
    // Hidden scripts break the indexes so we fix it here
    for (int x=0; x<=n; x++)
	if (script[x].hidden)
	    nb_hidden++;
    n += nb_hidden;
    if (script[n].nb_param) {
	if (script[n].nb_param == 3 && !script[n].value_list_label[0]) { // interval
	    menu_item_t *menu;
	    menu = (menu_item_t*)calloc(2,sizeof(menu_item_t));
	    menu[0].label = script[n].title;
	    int param = script[n].value_list[0];
	    if (script[n].changing >= script[n].value_list[0] && script[n].changing <= script[n].value_list[1])
		param = script[n].changing;
	    menu[0].value_int = &param;
	    menu[0].values_list_size = 3;
	    menu[0].values_list[0] = script[n].value_list[0];
	    menu[0].values_list[1] = script[n].value_list[1];
	    menu[0].values_list[2] = script[n].value_list[2];
	    TDialog *dlg = new TDialog("script parameter",menu);
	    dlg->execute();
	    delete dlg;
	    free(menu);
	    set_script_param(n,param);
	    script[n].changing = param;
	} else {
	    char btn[10240];
	    *btn = 0;
	    for (int x=0; x<script[n].nb_param; x++) {
		snprintf(&btn[strlen(btn)],10240-strlen(btn),"%s|",script[n].value_list_label[x]);
	    }
	    btn[strlen(btn)-1] = 0; // remove last |
	    int ret = raine_mbox("script parameter",script[n].title,btn);
	    if (ret)
		set_script_param(n,script[n].value_list[ret-1]);
	    else
		script[n].status = 0;
	}
    }
    run_script(n);
    return 0;
}

void add_scripts(menu_item_t *menu) {
    static char off[20],on[20];
    sprintf(off,"\E[31m%s",_("Off"));
    sprintf(on,"\E[32m%s",_("On"));
  for (int n=0; n<nb_scripts; n++) {
      if (script[n].hidden) continue;
      menu->label = script[n].title;
      if ((!script[n].lua && (script[n].on || script[n].off || script[n].run || script[n].change)) ||
	      (script[n].lua && (script[n].on_lua || script[n].off_lua || script[n].run_lua || script[n].change_lua))) {
	  menu->value_int = &script[n].status;
	  menu->menu_func = &activate_cheat;
	  if (script[n].run || script[n].off || script[n].run_lua || script[n].off_lua) {
	      menu->values_list_size = 2;
	      menu->values_list[0] = 0;
	      menu->values_list[1] = 1;
	      menu->values_list_label[0] = off;
	      menu->values_list_label[1] = on;
	  } else {
	      menu->values_list_size = 1;
	      menu->values_list[0] = 0; // default value = 0, otherwise they run all the time !
	      menu->values_list_label[0] = _("Set");
	  }
      }
      menu++;
  }
}

extern double frame;

void do_stop(int argc, char **argv) {
    // Without argument, it's handled directly by update_scripts
    if (argc == 2) {
	char *s = argv[1];
	char old = 0;
	if (*s == '"' || *s == 39) {
	    s++;
	    old = s[strlen(s)-1];
	    s[strlen(s)-1] = 0;
	}
	for (int n=0; n<nb_scripts; n++) {
	    if (!strcmp(script[n].title,s)) {
		script[n].status = 0;
		break;
	    }
	}
	if (old) s[strlen(s)-1] = old;
    }
}

void update_scripts() {
    if (!nb_scripts) return;
    if (!cons)
	run_console_command("");
    int n;
    try {
	get_regs(); // Required in case a script execute a cpu command !
	for (n=0; n<nb_scripts; n++) {
	    if (script[n].status) {
		init_script_param(n);
		nb_script = n;
		if (script[n].run || script[n].run_lua) {
		    running++;
		    section = "run";
		    parsing = 0;
		    if (!script[n].parsed && !script[n].lua) {
			int nb = 0;
			for ( nb=0; script[n].run[nb]; nb++);

			script[n].parsed = (tparsed*)calloc(nb,sizeof(tparsed));
			parsing = 1;
		    }

		    if (script[n].lua) {
			char *argv[2];
			argv[0] = "luascript";
			argv[1] = script[n].run_lua;
			do_lua(2,argv);
			running--;
			continue;
		    }

		    for ( sline=0; script[n].run[sline]; sline++) {
			if (*script[n].run[sline] == '#') continue;
			if (parsing) {
			    run_console_command(script[n].run[sline]);
			    int argc; char **argv; void (*cmd)(int, char **);
			    cons->get_parsed_info(&argc,&argv,&cmd);
			    if (argc) {
				script[n].parsed[sline].argc = argc;
				script[n].parsed[sline].argv = (char**)calloc(argc,sizeof(char*));
				script[n].parsed[sline].cmd = cmd;
			    }
			    for (int x=0; x<argc; x++)
				script[n].parsed[sline].argv[x] = strdup(argv[x]);
			    cons->finish_parsed_info();
			} else {
			    if (cons->interactive)
				(*cons->interactive)(script[n].run[sline]);
			    else if (script[n].parsed[sline].cmd)
				(*script[n].parsed[sline].cmd)(script[n].parsed[sline].argc,script[n].parsed[sline].argv);
			    else {
				// Simple line of the type variable=value or variable=function(arg)
				run_console_command(script[n].run[sline]);
			    }
			}
			if (active_if_script_section() && script[n].parsed[sline].argc == 1 && !strcmp(script[n].parsed[sline].argv[0],"stop")) {
			    // stop current script
			    script[n].status = 0;
			    break;
			}
		    }
		    running--;
		}
	    }
	}
    }
    catch(ConsExcept &e) {
	if (cons && cons->is_visible()) cons->print(e.what());
	else {
	    char msg[240];
	    int nb,line;
	    char *section;
	    if (get_running_script_info(&nb,&line,&section)) {
		// Line numbers are unprecise because lines with only a comment are jumped
		if (script[nb].lua)
		    sprintf(msg,"script: %s\nsection: %s\nLUA\n\n",
			    get_script_title(nb),
			    section);
		else
		    sprintf(msg,"script: %s\nsection: %s\nline: %d\n\n",
			    get_script_title(nb),
			    section,
			    line+1);
		strncat(msg, e.what(),240-strlen(msg));
		stop_script(nb);
		strncat(msg,"\n(script stopped)",240-strlen(msg));
		raine_mbox("script error",msg,"ok");
	    } else
		raine_mbox("script error",e.what(),"ok");
	    reset_ingame_timer();
	}
    }
    catch(const char *msg) {
	if (cons && cons->is_visible()) cons->print(msg);
	else {
	    raine_mbox("script error",(char *)msg,"ok");
	}
    }
}

static FILE *fscript;

static void get_script_commands(char *field) {
  if (strcmp(field,".")) {
    fprintf(fscript,"  %s\n",field);
    cons->print("  %s",field);
    *field = 0;
  } else {
    fprintf(fscript,"\n");
    fclose(fscript);
    cons->print("your script has been recorded");
    cons->end_interactive();
  }
}

static void get_script_mode(char *field) {
  if (!*field) {
    cons->print("executed only once");
  } else {
    cons->print("looping script");
    fprintf(fscript," always");
  }
  if (*field) *field = 0;
  fprintf(fscript,"\n");
  cons->print("now type your commands, a . to finish...");
  cons->set_interactive(&get_script_commands);
}

static void get_script_name(char *field) {
  fprintf(fscript,"script \"%s\"",field);
  cons->print("  %s",field);
  *field = 0;
  cons->print("Type return for a script which is executed only once, or type any text + return for a script which loops until manually stopped");
  cons->set_interactive(&get_script_mode);
}

void do_start_script(int argc, char **argv) {
    if (argc != 2)
	throw "Syntax : start_script name\n";
    for (int n=0; n<nb_scripts; n++) {
	if (!strcmp(script[n].title,argv[1])) {
	    if (script[n].run)
		script[n].status = 1;
	    else
		run_script(n);
	    return;
	}
    }

    throw ConsExcept("Didn't find script %s",argv[1]);
}

void do_script(int argc, char **argv) {
  char *s = get_script_name(1);
  fscript = fopen(s,"a");
  if (!fscript) {
    cons->print("can't create file %s",s);
    return;
  }
  if (argc == 1) {
    cons->print("Please type the script description (spaces allowed)");
    cons->set_interactive(&get_script_name);
  } else {
    fprintf(fscript,"script \"%s\"",argv[1]);
    if (argc == 3) {
      get_script_mode(argv[2]);
    } else {
      get_script_mode("");
    }
  }
}

void stop_scripts() {
    // Stop scripts which run every frame, useful after an error in a script
    for (int n=0; n<nb_scripts; n++) {
	if (script[n].status)
	    run_off_section(n);
	script[n].status = 0;
    }
    running = 0;
}

char* get_script_title(int n) {
    if (n < nb_scripts && n>=0)
	return script[n].title;
    return "";
}

void stop_script(int n) {
    if (n < nb_scripts && n>=0)
	script[n].status = 0;
}

int is_script_parsing() { return parsing; }

void get_script_parsed(int n, int line, int *myargc, char ***myargv,void (**mycmd)(int, char **) ) {
    *myargc = script[n].parsed[line].argc;
    *myargv = script[n].parsed[line].argv;
    *mycmd = script[n].parsed[line].cmd;
}

void script_set_parsed(int n, int line, int argc, char **argv, void (*cmd)(int, char **) ) {
    if (argc) {
	script[n].parsed[line].argc = argc;
	script[n].parsed[line].argv = (char**)calloc(argc,sizeof(char*));
	script[n].parsed[line].cmd = cmd;
    }
    for (int x=0; x<argc; x++)
	script[n].parsed[line].argv[x] = strdup(argv[x]);
}

