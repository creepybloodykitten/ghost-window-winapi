#include <windows.h>


HWND ghost_window=NULL;
int current_alpha=128;
HHOOK hMouseHook = NULL; //хендл для мыши(чтобы детектить колесико колесико)

void toggle_ghost_mode()
{
    if(ghost_window!=NULL) // уже есть ghost_window значит переключение происходит в обычный режим из призрачного
    {
        LONG_PTR oldStyle = GetWindowLongPtr(ghost_window, GWL_EXSTYLE); //64битное значение стиля
        LONG_PTR newStyle = oldStyle & ~(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST);

        SetWindowLongPtr(ghost_window, GWL_EXSTYLE, newStyle); //применение стиля
        SetWindowPos(ghost_window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED); // измение всех данных окна
        SetLayeredWindowAttributes(ghost_window, 0, 255, LWA_ALPHA); // измение альфа канала окна
        RedrawWindow(ghost_window, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);//перерисовывание окна

        ghost_window=NULL;
        return;
    }

    //если нет призрачного окна
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    HWND hwnd_under_mouse = WindowFromPoint(cursor_pos);
    HWND hwnd_to_ghost = GetAncestor(hwnd_under_mouse, GA_ROOT); //точно берем самое главное родительское окно а не дочерний элемент(окно)

    if(hwnd_to_ghost==NULL || hwnd_to_ghost == GetDesktopWindow()) return; //если нет окна которое можно сделать прозрачным или это рабочий стол

    // функция возвращает 32 бита, где каждый бит - это какая-то настройка
    LONG_PTR oldStyle = GetWindowLongPtr(hwnd_to_ghost, GWL_EXSTYLE); //читаем информацию о стиле окна которое должно стать призрачным
    LONG_PTR newStyle = oldStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST;
    // WS_EX_LAYERED - разрешает окну быть полупрозрачным
    // WS_EX_TRANSPARENT - делает окно "прозрачным" для мыши (клики проходят сквозь)
    // WS_EX_TOPMOST - заставляет окно висеть поверх всех остальных

    SetWindowLongPtr(hwnd_to_ghost, GWL_EXSTYLE, newStyle); // обновляем окно применяя стиль

    SetWindowPos(hwnd_to_ghost, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    SetLayeredWindowAttributes(hwnd_to_ghost, 0, (BYTE)current_alpha, LWA_ALPHA); //применяем уровень прозрачности
    // crKey - цвет, который должен стать прозрачным (нам не нужно, поэтому 0)
    // bAlpha  - степень прозрачности (0 - невидимое, 255 - обычное) 128 = 50%
    // LWA_ALPHA - флаг, использующий параметр bAlpha

    ghost_window=hwnd_to_ghost;
    
}

// для обработки мыши соблюдаем контракт LRESULT (CALLBACK *HOOKPROC)(int code, WPARAM wParam, LPARAM lParam);
//и hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0); в main
LRESULT CALLBACK LowLevelMouseProc(int code,WPARAM wParam,LPARAM lParam)
{
    // nCode == HC_ACTION означает, что есть полезное сообщение
    // wParam == WM_MOUSEWHEEL означает, что крутят колесико
    if (code == HC_ACTION && wParam == WM_MOUSEWHEEL) 
    {
        if (ghost_window != NULL)//если существует окно призрак
        {
            if (GetAsyncKeyState(VK_RMENU) & 0x8000)//если зажат правый alt
            {
                // получаем информацию о колесике из структуры
                MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
                // mouseData хранит направление прокрутки в старшем слове
                short delta = HIWORD(pMouseStruct->mouseData);

                if (delta > 0) current_alpha += 15; // крутим колесико вверх - непрозрачнее
                else current_alpha -= 15; // крутим вниз - прозрачнее

                if (current_alpha > 255) current_alpha = 255;
                if (current_alpha < 10) current_alpha = 0;

                SetLayeredWindowAttributes(ghost_window, 0, (BYTE)current_alpha, LWA_ALPHA);

                return 1; //1 - не передаем событие дальше 
            }
            
        }
    }
    // eсли условия не совпали, передаем управление дальше системе
    return CallNextHookEx(hMouseHook, code, wParam, lParam);
}

int WINAPI WinMain(
    HINSTANCE hInstance, //HINSTANCE - это номер запущенного приложения в памяти
    HINSTANCE hPrev, // hPrevInstance не имеет смысла. Он использовался в 16-разрядной версии Windows, но теперь всегда равен нулю
    LPSTR lpCmdLine, //lpCmdLine - строка char* - аргументы при запуске
    int nCmdShow // nCmdShow — это флаг, указывающий, является ли основное окно приложения свернуто, развернуто или отображается в обычном режиме.
    )
{
   //сообщение WM_HOTKEY будет при нажатии
   //создаем хоткей для перехода в призрачный режим
    if (!RegisterHotKey(NULL, 1, MOD_CONTROL | MOD_ALT, 'Z'))//когда будет данная комбинация то только она придет в этот поток и в эту программу
    {
        return 1;
    }

    //хук на мышь
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);
    if (!hMouseHook)
    {
        return 1;
    }


    MSG msg; //cтруктура сообщения ,cюда windows будет класть информацию о событиях

    while (GetMessage(&msg, NULL, 0, 0)) 
    {
        //понять что делают и почему в while
        TranslateMessage(&msg); 
        DispatchMessage(&msg);
        if (msg.message == WM_HOTKEY)//пришло ли сообщение о том что зажата клавиша
        {
            // Если ID хоткея имеется
            if (msg.wParam == 1) 
            {
                toggle_ghost_mode();
            }
        }
    }

    UnhookWindowsHookEx(hMouseHook);
    UnregisterHotKey(NULL, 1);
    return 0;
}