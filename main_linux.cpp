#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// Глобальные переменные
Display* dpy = NULL;
Window root_win;
Window ghost_window = None;
unsigned int current_alpha = 128; // 0-255 для удобства, внутри конвертируем

// Атомы (константы X11 для свойств окон)
Atom OPACITY_ATOM;
Atom STATE_ATOM;
Atom STATE_ABOVE_ATOM;

// Обработчик ошибок, чтобы программа не падала, если окно исчезнет во время работы
int XErrorHandlerImpl(Display *display, XErrorEvent *event) {
    return 0; 
}

// Функция для поиска главного окна под курсором (аналог GetAncestor(..., GA_ROOT))
Window get_toplevel_window(Window start_win) {
    Window parent;
    Window root_return;
    Window *children;
    unsigned int nchildren;
    Window current = start_win;

    while (true) {
        if (current == 0 || current == root_win) return current;

        // Получаем родителя
        if (XQueryTree(dpy, current, &root_return, &parent, &children, &nchildren) == 0) {
            return None;
        }
        if (children) XFree(children);

        // Если родитель - это корневое окно (рабочий стол), значит current - это главное окно приложения
        if (parent == root_win) {
            return current;
        }
        current = parent;
    }
}

// Установка прозрачности (аналог LWA_ALPHA)
void set_opacity(Window win, unsigned int alpha_255) {
    // В X11 непрозрачность это 32-битное число (0 - min, 0xFFFFFFFF - max)
    unsigned long opacity = (unsigned long)alpha_255 * 0x01010101;
    if (alpha_255 == 255) opacity = 0xFFFFFFFF; 

    // Изменяем свойство _NET_WM_WINDOW_OPACITY
    XChangeProperty(dpy, win, OPACITY_ATOM, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&opacity, 1);
}

// Установка "поверх всех окон" (аналог HWND_TOPMOST)
void set_topmost(Window win, bool enable) {
    XEvent xev;
    xev.type = ClientMessage;
    xev.xclient.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = STATE_ATOM;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = enable ? 1 : 0; // 1 = добавить, 0 = убрать
    xev.xclient.data.l[1] = STATE_ABOVE_ATOM;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent(dpy, root_win, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(dpy);
}

// Включение/выключение режима клика насквозь (аналог WS_EX_TRANSPARENT)
void set_click_through(Window win, bool enable) {
    if (enable) {
        // Устанавливаем "Input Shape" в пустой регион (None). Мышь проваливается.
        XRectangle rect;
        XserverRegion region = XFixesCreateRegion(dpy, &rect, 0);
        XFixesSetWindowShapeRegion(dpy, win, ShapeInput, 0, 0, region);
        XFixesDestroyRegion(dpy, region);
    } else {
        // Возвращаем дефолтную область ввода (сбрасываем маску)
        XFixesSetWindowShapeRegion(dpy, win, ShapeInput, 0, 0, None);
    }
}

void toggle_ghost_mode() {
    if (ghost_window != None) {
        // --- Выключение призрачного режима ---
        
        // 1. Убираем TopMost
        set_topmost(ghost_window, false);
        
        // 2. Возвращаем кликабельность
        set_click_through(ghost_window, false);

        // 3. Возвращаем полную непрозрачность (удаляем свойство opacity)
        XDeleteProperty(dpy, ghost_window, OPACITY_ATOM);

        ghost_window = None;
        printf("Ghost mode OFF\n");
        return;
    }

    // --- Включение призрачного режима ---

    // 1. Узнаем, где мышь
    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    XQueryPointer(dpy, root_win, &root_return, &child_return, &root_x, &root_y, &win_x, &win_y, &mask);

    if (child_return == None) return; // Мышь над пустым столом

    // 2. Ищем главное окно
    Window target_win = get_toplevel_window(child_return);
    if (target_win == None || target_win == root_win) return;

    // 3. Применяем настройки
    // Always on top
    set_topmost(target_win, true);
    
    // Прозрачность
    set_opacity(target_win, current_alpha);
    
    // Клик насквозь
    set_click_through(target_win, true);

    ghost_window = target_win;
    printf("Ghost mode ON. Window ID: 0x%lX\n", ghost_window);
}

int main() {
    // Подключаемся к X серверу
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    // Инициализация переменных
    root_win = DefaultRootWindow(dpy);
    XSetErrorHandler(XErrorHandlerImpl);

    // Получаем атомы для управления окнами (стандарт EWMH)
    OPACITY_ATOM = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
    STATE_ATOM = XInternAtom(dpy, "_NET_WM_STATE", False);
    STATE_ABOVE_ATOM = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);

    // --- Регистрация хоткеев (Global Hooks) ---
    
    // 1. Хоткей Ctrl+Shift+A
    // Получаем код клавиши 'A'
    KeyCode key_a = XKeysymToKeycode(dpy, XK_A);
    // XGrabKey перехватывает нажатие глобально
    XGrabKey(dpy, key_a, ControlMask | ShiftMask, root_win, True, GrabModeAsync, GrabModeAsync);
    // Часто NumLock/CapsLock мешают, поэтому грабим комбинации с ними тоже (для надежности)
    XGrabKey(dpy, key_a, ControlMask | ShiftMask | Mod2Mask, root_win, True, GrabModeAsync, GrabModeAsync); // +NumLock

    // 2. Мышь (Alt + Колесико)
    // Button4 = Scroll Up, Button5 = Scroll Down
    // Mod1Mask = Alt
    XGrabButton(dpy, Button4, Mod1Mask, root_win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button5, Mod1Mask, root_win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    // То же самое для Alt+NumLock
    XGrabButton(dpy, Button4, Mod1Mask | Mod2Mask, root_win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button5, Mod1Mask | Mod2Mask, root_win, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);


    printf("Running... Press Ctrl+Shift+A to toggle ghost mode.\n");
    printf("Hold Alt + Scroll to change opacity.\n");

    XEvent ev;
    while (true) {
        XNextEvent(dpy, &ev);

        // Обработка клавиш (Ctrl+Shift+A)
        if (ev.type == KeyPress) {
            if (ev.xkey.keycode == key_a) {
                toggle_ghost_mode();
            }
        }

        // Обработка мыши (Колесико с зажатым Alt)
        if (ev.type == ButtonPress && ghost_window != None) {
            bool changed = false;
            if (ev.xbutton.button == Button4) { // Scroll Up
                if (current_alpha < 240) current_alpha += 15;
                else current_alpha = 255;
                changed = true;
            } else if (ev.xbutton.button == Button5) { // Scroll Down
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

    XCloseDisplay(dpy);
    return 0;
}