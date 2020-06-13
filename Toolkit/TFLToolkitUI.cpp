// generated by Fast Light User Interface Designer (fluid) version 1.0400

#include "TFLToolkitUI.h"
#include "app/TFLApp.h"
#include <FL/fl_draw.H>

Fl_Double_Window *wToolkitWindow=(Fl_Double_Window *)0;

Fl_Menu_Bar *wToolkitMenubar=(Fl_Menu_Bar *)0;

static void cb_Quit(Fl_Menu_*, void*) {
  gApp->UserActionQuit();
}

static void cb_Run(Fl_Menu_*, void*) {
  gApp->UserActionToolkitRun();
}

Fl_Menu_Item menu_wToolkitMenubar[] = {
 {"File", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"New...", FL_COMMAND|0x1006e,  0, 0, 1, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Open...", FL_COMMAND|0x1006f,  0, 0, 129, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Save", FL_COMMAND|0x10073,  0, 0, 1, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Save As...", 0,  0, 0, 129, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Close", FL_COMMAND|0x10077,  0, 0, 129, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"", 0,  0, 0, 17, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"", 0,  0, 0, 17, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"", 0,  0, 0, 17, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"", 0,  0, 0, 17, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"", 0,  0, 0, 145, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Page Setup...", 0,  0, 0, 1, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Print...", FL_COMMAND|0x10070,  0, 0, 129, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Quit", FL_COMMAND|0x71,  (Fl_Callback*)cb_Quit, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {"Edit", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Cut", FL_COMMAND|0x10078,  0, 0, 1, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Copy", FL_COMMAND|0x10063,  0, 0, 1, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Paste", FL_COMMAND|0x10076,  0, 0, 1, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {"Application", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Build", FL_COMMAND|0x62,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Install", FL_COMMAND|0x32,  0, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Run", FL_COMMAND|0x72,  (Fl_Callback*)cb_Run, 0, 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {"Help", 0,  0, 0, 64, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Toolkit Manual...", 0,  0, 0, 1, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"About Toolkit...", 0,  0, 0, 1, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0},
 {0,0,0,0,0,0,0,0,0}
};

Fl_Group *wToolkitToolbar=(Fl_Group *)0;

Fl_Button *wToolkitRun=(Fl_Button *)0;

static void cb_wToolkitRun(Fl_Button*, void*) {
  gApp->UserActionToolkitRun();
}

Fl_Text_Editor *wToolkitEditor=(Fl_Text_Editor *)0;

Fl_Text_Display *wToolkitTerminal=(Fl_Text_Display *)0;

Fl_Double_Window* CreateToolkitWindow(int x, int y) {
  { wToolkitWindow = new Fl_Double_Window(720, 600);
    { Fl_Menu_Bar* o = wToolkitMenubar = new Fl_Menu_Bar(0, 0, 720, 24);
      wToolkitMenubar->color(FL_LIGHT2);
      wToolkitMenubar->menu(menu_wToolkitMenubar);
      o->box(FL_FREE_BOXTYPE);
    } // Fl_Menu_Bar* wToolkitMenubar
    { Fl_Group* o = wToolkitToolbar = new Fl_Group(0, 24, 720, 54);
      wToolkitToolbar->box(FL_THIN_UP_BOX);
      wToolkitToolbar->color(FL_LIGHT2);
      { Fl_Button* o = wToolkitRun = new Fl_Button(10, 25, 36, 36, "@>");
        wToolkitRun->color(FL_LIGHT2);
        wToolkitRun->labelsize(24);
        wToolkitRun->labelcolor((Fl_Color)48);
        wToolkitRun->callback((Fl_Callback*)cb_wToolkitRun);
        wToolkitRun->align(Fl_Align(512|FL_ALIGN_INSIDE));
        o->box(FL_FREE_BOXTYPE);
        o->down_box(FL_FREE_BOXTYPE);
        o->clear_visible_focus();
      } // Fl_Button* wToolkitRun
      o->box(FL_FREE_BOXTYPE);
      wToolkitToolbar->end();
    } // Fl_Group* wToolkitToolbar
    { Fl_Box* o = new Fl_Box(0, 78, 320, 480);
      o->hide();
    } // Fl_Box* o
    { wToolkitEditor = new Fl_Text_Editor(0, 78, 720, 422);
      wToolkitEditor->box(FL_THIN_DOWN_FRAME);
      wToolkitEditor->selection_color((Fl_Color)246);
      wToolkitEditor->textfont(4);
      Fl_Group::current()->resizable(wToolkitEditor);
    } // Fl_Text_Editor* wToolkitEditor
    { wToolkitTerminal = new Fl_Text_Display(0, 500, 720, 100);
      wToolkitTerminal->box(FL_THIN_DOWN_FRAME);
      wToolkitTerminal->selection_color((Fl_Color)246);
      wToolkitTerminal->textfont(4);
    } // Fl_Text_Display* wToolkitTerminal
    wToolkitWindow->position(x, y);
    wToolkitWindow->end();
  } // Fl_Double_Window* wToolkitWindow
  return wToolkitWindow;
}
