#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

static Display* display = nullptr;
static Window   window;
static GC       gc;
static int      screen;
static XFontStruct* font_small = nullptr;
static XFontStruct* font_big   = nullptr;
static XFontStruct* font_tiny  = nullptr;
static Atom     wm_delete;

static unsigned long C_BG, C_DISPLAY_BG, C_DISPLAY_FG;
static unsigned long C_BTN_NUM, C_BTN_OP, C_BTN_FN, C_BTN_EQ, C_BTN_CLR;
static unsigned long C_BORDER, C_TEXT_DARK, C_TEXT_LIGHT, C_ERROR, C_EXPR_FG;

static int  ev_x = 0, ev_y = 0, ev_char = 0;

static char display_buf[64] = "0";
static int  display_len = 1;
static int  display_has_dot = 0;

static char expr_prefix[128] = "";
static char expr_final[160] = "";

static double calc_current = 0.0;
static int    calc_pending_op = 0;
static int    calc_reset_next = 1;
static int    calc_error = 0;
static int    calc_just_equal = 0;
static int    calc_has_prefix = 0;

static unsigned long rgb(int r, int g, int b) {
    return (static_cast<unsigned long>(r) << 16) |
           (static_cast<unsigned long>(g) << 8)  |
           static_cast<unsigned long>(b);
}

static void calc_format(double v) {
    if (v == (double)(long long)v) {
        snprintf(display_buf, sizeof(display_buf), "%lld", (long long)v);
    } else {
        snprintf(display_buf, sizeof(display_buf), "%.10g", v);
    }
    display_len = (int)strlen(display_buf);
    display_has_dot = (strchr(display_buf, '.') != nullptr) ? 1 : 0;
}

static double calc_apply(double a, double b, int op) {
    if (op == 43) return a + b;
    if (op == 45) return a - b;
    if (op == 42) return a * b;
    if (op == 47) {
        if (b == 0.0) return std::nan("");
        return a / b;
    }
    return b;
}

static const char* op_to_str(int op) {
    if (op == 43) return " + ";
    if (op == 45) return " - ";
    if (op == 42) return " * ";
    if (op == 47) return " / ";
    return "";
}

static void prefix_clear() {
    expr_prefix[0] = '\0';
    calc_has_prefix = 0;
}

static void prefix_append(const char* s) {
    size_t l = strlen(expr_prefix);
    size_t n = strlen(s);
    if (l + n < sizeof(expr_prefix) - 1) {
        memcpy(expr_prefix + l, s, n);
        expr_prefix[l + n] = '\0';
        calc_has_prefix = 1;
    }
}

extern "C" {

int open_linux_window(const char* title_text) {
    display = XOpenDisplay(nullptr);
    if (!display) return 1;
    screen = DefaultScreen(display);
    window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                 100, 100, 360, 560, 1,
                                 BlackPixel(display, screen),
                                 WhitePixel(display, screen));
    XStoreName(display, window, title_text);
    XSelectInput(display, window,
        ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete, 1);
    XMapWindow(display, window);
    gc = XCreateGC(display, window, 0, nullptr);
    font_small = XLoadQueryFont(display, "9x15");
    if (!font_small) font_small = XLoadQueryFont(display, "fixed");
    font_big = XLoadQueryFont(display, "10x20");
    if (!font_big) font_big = font_small;
    font_tiny = XLoadQueryFont(display, "6x13");
    if (!font_tiny) font_tiny = font_small;

    C_BG         = rgb(30, 30, 36);
    C_DISPLAY_BG = rgb(20, 20, 24);
    C_DISPLAY_FG = rgb(255, 255, 255);
    C_BTN_NUM    = rgb(60, 60, 70);
    C_BTN_OP     = rgb(240, 150, 40);
    C_BTN_FN     = rgb(90, 90, 100);
    C_BTN_EQ     = rgb(60, 180, 90);
    C_BTN_CLR    = rgb(200, 60, 60);
    C_BORDER     = rgb(15, 15, 18);
    C_TEXT_DARK  = rgb(30, 30, 30);
    C_TEXT_LIGHT = rgb(240, 240, 240);
    C_ERROR      = rgb(240, 80, 80);
    C_EXPR_FG    = rgb(160, 160, 170);

    display_buf[0] = '0';
    display_buf[1] = '\0';
    display_len = 1;
    display_has_dot = 0;
    expr_final[0] = '\0';
    prefix_clear();
    calc_current = 0.0;
    calc_pending_op = 0;
    calc_reset_next = 1;
    calc_error = 0;
    calc_just_equal = 0;
    return 0;
}

void close_linux_window() {
    if (font_small) XFreeFont(display, font_small);
    if (font_big && font_big != font_small) XFreeFont(display, font_big);
    if (font_tiny && font_tiny != font_small && font_tiny != font_big)
        XFreeFont(display, font_tiny);
    if (gc) XFreeGC(display, gc);
    if (window) XDestroyWindow(display, window);
    if (display) XCloseDisplay(display);
    display = nullptr;
}

void clear_window() {
    if (!display) return;
    XClearWindow(display, window);
    XFlush(display);
}

unsigned long get_color_bg()         { return C_BG; }
unsigned long get_color_display_bg() { return C_DISPLAY_BG; }
unsigned long get_color_display_fg() { return C_DISPLAY_FG; }
unsigned long get_color_btn_num()    { return C_BTN_NUM; }
unsigned long get_color_btn_op()     { return C_BTN_OP; }
unsigned long get_color_btn_fn()     { return C_BTN_FN; }
unsigned long get_color_btn_eq()     { return C_BTN_EQ; }
unsigned long get_color_btn_clr()    { return C_BTN_CLR; }
unsigned long get_color_border()     { return C_BORDER; }
unsigned long get_color_text_dark()  { return C_TEXT_DARK; }
unsigned long get_color_text_light() { return C_TEXT_LIGHT; }
unsigned long get_color_error()      { return C_ERROR; }
unsigned long get_color_expr_fg()    { return C_EXPR_FG; }

void set_drawing_color(unsigned long c) {
    if (display) XSetForeground(display, gc, c);
}

void fill_rectangle(int x, int y, int w, int h) {
    if (display) XFillRectangle(display, window, gc, x, y, w, h);
}

void draw_rectangle(int x, int y, int w, int h) {
    if (display) XDrawRectangle(display, window, gc, x, y, w, h);
}

void draw_line(int x1, int y1, int x2, int y2) {
    if (display) XDrawLine(display, window, gc, x1, y1, x2, y2);
}

void draw_text(int x, int y, const char* text, unsigned long color, int size) {
    if (!display) return;
    XFontStruct* f;
    if (size == 2) f = font_big;
    else if (size == 1) f = font_small;
    else f = font_tiny;
    if (!f) return;
    XSetFont(display, gc, f->fid);
    XSetForeground(display, gc, color);
    XDrawString(display, window, gc, x, y, text, (int)strlen(text));
}

int measure_text(const char* text, int size) {
    XFontStruct* f;
    if (size == 2) f = font_big;
    else if (size == 1) f = font_small;
    else f = font_tiny;
    if (!f) return 0;
    return XTextWidth(f, text, (int)strlen(text));
}

void draw_text_center(int cx, int y, const char* text, unsigned long color, int size) {
    int w = measure_text(text, size);
    draw_text(cx - w / 2, y, text, color, size);
}

void draw_text_right(int rx, int y, const char* text, unsigned long color, int size) {
    int w = measure_text(text, size);
    draw_text(rx - w, y, text, color, size);
}

void flush_display() {
    if (display) XFlush(display);
}

int get_event_x()    { return ev_x; }
int get_event_y()    { return ev_y; }
int get_event_char() { return ev_char; }

int poll_window_event() {
    if (!display) return -1;
    while (XPending(display) > 0) {
        XEvent ev;
        XNextEvent(display, &ev);

        if (ev.type == Expose) return 4;
        if (ev.type == DestroyNotify) return 3;
        if (ev.type == ClientMessage) {
            if ((Atom)ev.xclient.data.l[0] == wm_delete) return 1;
        }
        if (ev.type == ButtonPress) {
            ev_x = ev.xbutton.x;
            ev_y = ev.xbutton.y;
            return 2;
        }
        if (ev.type == KeyPress) {
            char buf[8];
            KeySym ks;
            int len = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, nullptr);
            if (ks == XK_Escape) return 1;
            if (ks == XK_Return) { ev_char = '='; return 5; }
            if (ks == XK_BackSpace) { ev_char = '<'; return 5; }
            ev_char = (len > 0) ? (int)(unsigned char)buf[0] : 0;
            return 5;
        }
    }
    return 0;
}

void calc_reset() {
    strcpy(display_buf, "0");
    display_len = 1;
    display_has_dot = 0;
    prefix_clear();
    expr_final[0] = '\0';
    calc_current = 0.0;
    calc_pending_op = 0;
    calc_reset_next = 1;
    calc_error = 0;
    calc_just_equal = 0;
}

void calc_digit(int d) {
    if (calc_error) calc_reset();
    if (calc_just_equal) {
        prefix_clear();
        expr_final[0] = '\0';
        calc_just_equal = 0;
        calc_reset_next = 1;
    }
    if (calc_reset_next) {
        display_buf[0] = (char)('0' + d);
        display_buf[1] = '\0';
        display_len = 1;
        display_has_dot = 0;
        calc_reset_next = 0;
        return;
    }
    if (display_buf[0] == '0' && display_len == 1 && !display_has_dot) {
        display_buf[0] = (char)('0' + d);
        display_buf[1] = '\0';
        return;
    }
    if (display_len < (int)sizeof(display_buf) - 1) {
        display_buf[display_len++] = (char)('0' + d);
        display_buf[display_len] = '\0';
    }
}

void calc_dot() {
    if (calc_error) calc_reset();
    if (calc_just_equal) {
        prefix_clear();
        expr_final[0] = '\0';
        calc_just_equal = 0;
        calc_reset_next = 1;
    }
    if (calc_reset_next) {
        strcpy(display_buf, "0.");
        display_len = 2;
        display_has_dot = 1;
        calc_reset_next = 0;
        return;
    }
    if (!display_has_dot && display_len < (int)sizeof(display_buf) - 1) {
        display_buf[display_len++] = '.';
        display_buf[display_len] = '\0';
        display_has_dot = 1;
    }
}

void calc_op(int op) {
    if (calc_error) return;

    if (calc_just_equal) {
        calc_just_equal = 0;
        prefix_clear();
        char keep[64];
        strcpy(keep, display_buf);
        prefix_append(keep);
        calc_current = atof(keep);
        prefix_append(op_to_str(op));
        calc_pending_op = op;
        calc_reset_next = 1;
        expr_final[0] = '\0';
        return;
    }

    double val = atof(display_buf);

    if (calc_pending_op && !calc_reset_next) {
        prefix_append(display_buf);
        calc_current = calc_apply(calc_current, val, calc_pending_op);
        if (std::isnan(calc_current)) {
            calc_error = 1;
            strcpy(display_buf, "Error");
            display_len = 5;
            display_has_dot = 0;
            prefix_clear();
            return;
        }
        calc_format(calc_current);
    } else {
        if (calc_has_prefix && calc_pending_op) {
            prefix_clear();
        }
        prefix_append(display_buf);
        calc_current = val;
    }

    prefix_append(op_to_str(op));
    calc_pending_op = op;
    calc_reset_next = 1;
}

void calc_equal() {
    if (calc_error) return;

    if (calc_pending_op) {
        char lhs[160];
        snprintf(lhs, sizeof(lhs), "%s%s", expr_prefix, display_buf);
        double val = atof(display_buf);
        calc_current = calc_apply(calc_current, val, calc_pending_op);
        if (std::isnan(calc_current)) {
            calc_error = 1;
            strcpy(display_buf, "Error");
            display_len = 5;
            display_has_dot = 0;
            snprintf(expr_final, sizeof(expr_final), "%s =", lhs);
            prefix_clear();
            calc_pending_op = 0;
            calc_reset_next = 1;
            calc_just_equal = 1;
            return;
        }
        snprintf(expr_final, sizeof(expr_final), "%s =", lhs);
        prefix_clear();
        calc_format(calc_current);
        calc_pending_op = 0;
        calc_reset_next = 1;
        calc_just_equal = 1;
    }
}

void calc_backspace() {
    if (calc_error) { calc_reset(); return; }
    if (calc_just_equal) {
        prefix_clear();
        expr_final[0] = '\0';
        calc_just_equal = 0;
    }
    if (calc_reset_next) return;
    if (display_len <= 1) {
        strcpy(display_buf, "0");
        display_len = 1;
        display_has_dot = 0;
        calc_reset_next = 1;
        return;
    }
    display_len--;
    display_buf[display_len] = '\0';
    display_has_dot = (strchr(display_buf, '.') != nullptr) ? 1 : 0;
}

void calc_percent() {
    if (calc_error) return;
    double v = atof(display_buf) / 100.0;
    calc_format(v);
    calc_reset_next = 1;
}

void calc_sign() {
    if (calc_error) return;
    if (display_buf[0] == '-') {
        memmove(display_buf, display_buf + 1, display_len);
        display_len--;
    } else if (display_buf[0] != '0' || display_has_dot) {
        if (display_len < (int)sizeof(display_buf) - 1) {
            memmove(display_buf + 1, display_buf, display_len + 1);
            display_buf[0] = '-';
            display_len++;
        }
    }
}

const char* calc_get_display() {
    return display_buf;
}

const char* calc_get_expr() {
    if (calc_just_equal && expr_final[0] != '\0') {
        return expr_final;
    }
    if (!calc_has_prefix) {
        return "";
    }
    static char out[192];
    snprintf(out, sizeof(out), "%s%s", expr_prefix, display_buf);
    return out;
}

int calc_has_error() {
    return calc_error;
}

}
