#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h> // Обязательно для ShapeInput
#include <X11/keysym.h>
#include <X11/Xutil.h> // Для XTextProperty
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Display* dpy = NULL;
Window root_win;
Window ghost_window = None;
unsigned int current_alpha = 128;

Atom OPACITY_ATOM;
Atom STATE_ATOM;
Atom STATE_ABOVE_ATOM;
Atom WM_STATE_ATOM;

int XErrorHandlerImpl(Display *display, XErrorEvent *event) {
    return 0; 
}

// Функция получения имени окна (для отладки)
void print_window_name(Window w) {
    XTextProperty name;
    if (XGetWMName(dpy, w, &name)) {
        printf("Target Window Name: %s\n", (char*)name.value);
        XFree(name.value);
    } else {
        printf("Target Window Name: (Unknown)\n");
    }
}

// Рекурсивный поиск "Клиентского" окна (настоящего приложения) внутри рамки WM
Window find_client_window(Window w) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    // Проверяем, есть ли у окна свойство WM_STATE. Если есть - это приложение.
    if (XGetWindowProperty(dpy, w, WM_STATE_ATOM, 0, 0, False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop) XFree(prop);
        if (actual_type != None) {
            return w; // Нашли!
        }
    }

    // Если нет, ищем среди детей
    Window root, parent;
    Window *children;
    unsigned int nchildren;
    if (XQueryTree(dpy, w, &root, &parent, &children, &nchildren) != 0) {
        Window result = None;
        // Перебираем детей (обычно их немного у рамки, часто всего один)
        for (unsigned int i = 0; i < nchildren; i++) {
            result = find_client_window(children[i]);
            if (result != None) break;
        }
        if (children) XFree(children);
        return result;
    }

    return None;
}

void set_opacity(Window win, unsigned int alpha_255) {
    unsigned long opacity = (unsigned long)((double)alpha_255 / 255.0 * 0xFFFFFFFF);
    XChangeProperty(dpy, win, OPACITY_ATOM, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&opacity, 1);
    XFlush(dpy);
}

void set_topmost(Window win, bool enable) {
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = STATE_ATOM;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = enable ? 1 : 0; // 1 = add, 0 = remove
    xev.xclient.data.l[1] = STATE_ABOVE_ATOM;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 1; // source indication (1 = normal app)
    xev.xclient.data.l[4] = 0;

    XSendEvent(dpy, root_win, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(dpy);
}

void set_click_through(Window win, bool enable) {
    if (enable) {
        XRectangle rect;
        XserverRegion region = XFixesCreateRegion(dpy, &rect, 0);
        XFixesSetWindowShapeRegion(dpy, win, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(dpy, region);
    } else {
        XFixesSetWindowShapeRegion(dpy, win, ShapeInput, 0, 0, None);
    }
    XFlush(dpy);
}

void toggle_ghost_mode() {
    if (ghost_window != None) {
        // OFF
        set_topmost(ghost_window, false);
        set_click_through(ghost_window, false);
        XDeleteProperty(dpy, ghost_window, OPACITY_ATOM);
        XFlush(dpy);

        ghost_window = None;
        printf("Ghost mode OFF\n");
        return;
    }

    // ON
    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    
    // Получаем окно верхнего уровня (рамку), над которым мышь
    XQueryPointer(dpy, root_win, &root_return, &child_return, &root_x, &root_y, &win_x, &win_y, &mask);

    if (child_return == None || child_return == root_win) {
        printf("Error: No window found under mouse.\n");
        return;
    }

    // Ищем настоящее окно приложения внутри рамки
    Window client_win = find_client_window(child_return);
    
    // Если не нашли внутри, пробуем применить к самой рамке (child_return)
    Window target = (client_win != None) ? client_win : child_return;

    if (target == None) return;

    print_window_name(target); // Пишем в консоль, что нашли

    set_topmost(target, true);
    set_opacity(target, current_alpha);
    set_click_through(target, true);

    ghost_window = target;
    printf("Ghost mode ON. Window ID: 0x%lX\n", ghost_window);
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;

    root_win = DefaultRootWindow(dpy);
    XSetErrorHandler(XErrorHandlerImpl);

    OPACITY_ATOM = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    STATE_ATOM = XInternAtom(dpy, "_NET_WM_STATE", False);
    STATE_ABOVE_ATOM = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    WM_STATE_ATOM = XInternAtom(dpy, "WM_STATE", False);

    // Grab Hotkey Ctrl+Shift+A
    KeyCode key_a = XKeysymToKeycode(dpy, XK_A);
    XGrabKey(dpy, key_a, ControlMask | ShiftMask, root_win, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, key_a, ControlMask | ShiftMask | Mod2Mask, root_win, True, GrabModeAsync, GrabModeAsync);

    // Grab Mouse Alt+Scroll
    XGrabButton(dpy, Button4, Mod1Mask, root_win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button5, Mod1Mask, root_win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button4, Mod1Mask | Mod2Mask, root_win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button5, Mod1Mask | Mod2Mask, root_win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

    printf("Ready. Use Ctrl+Shift+A.\n");

    XEvent ev;
    while (true) {
        XNextEvent(dpy, &ev);

        if (ev.type == KeyPress && ev.xkey.keycode == key_a) {
            toggle_ghost_mode();
        }

        if (ev.type == ButtonPress && ghost_window != None) {
            bool changed = false;
            if (ev.xbutton.button == Button4) { // Up
                if (current_alpha < 240) current_alpha += 15;
                else current_alpha = 255;
                changed = true;
            } else if (ev.xbutton.button == Button5) { // Down
                if (current_alpha > 15) current_alpha -= 15;
                else current_alpha = 0;
                changed = true;
            }

            if (changed) {
                set_opacity(ghost_window, current_alpha);
                printf("Alpha: %d\n", current_alpha);
            }
        }
    }
    return 0;
}