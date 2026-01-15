#include <windows.h>


HWND ghost_window=NULL;


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

    SetLayeredWindowAttributes(hwnd_to_ghost, 0, 128, LWA_ALPHA); //применяем уровень прозрачности
    // crKey - цвет, который должен стать прозрачным (нам не нужно, поэтому 0)
    // bAlpha  - степень прозрачности (0 - невидимое, 255 - обычное) 128 = 50%
    // LWA_ALPHA - флаг, использующий параметр bAlpha

    ghost_window=hwnd_to_ghost;
    
}


int WINAPI WinMain(
    HINSTANCE hInstance, //HINSTANCE - это номер запущенного приложения в памяти
    HINSTANCE hPrev, // hPrevInstance не имеет смысла. Он использовался в 16-разрядной версии Windows, но теперь всегда равен нулю
    LPSTR lpCmdLine, //lpCmdLine - строка char* - аргументы при запуске
    int nCmdShow // nCmdShow — это флаг, указывающий, является ли основное окно приложения свернуто, развернуто или отображается в обычном режиме.
    )
{
   //сообщение WM_HOTKEY будет при нажатии
   //создаем хоткей
    if (!RegisterHotKey(NULL, 1, MOD_CONTROL, VK_SPACE))//когда будет данная комбинация то только она придет в этот поток и в эту программу
    {
        //MessageBox(NULL, "cant regist Hotkey!", "error", MB_ICONERROR);
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

    UnregisterHotKey(NULL, 1);
    return 0;
}