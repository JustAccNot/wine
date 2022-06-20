/*
 * Default window procedure
 *
 * Copyright 1993, 1996 Alexandre Julliard
 *	     1995 Alex Korobka
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <string.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winnls.h"
#include "imm.h"
#include "win.h"
#include "user_private.h"
#include "controls.h"
#include "wine/server.h"
#include "wine/exception.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win);

#define DRAG_FILE  0x454C4946

/***********************************************************************
 *           DEFWND_ControlColor
 *
 * Default colors for control painting.
 */
HBRUSH DEFWND_ControlColor( HDC hDC, UINT ctlType )
{
    if( ctlType == CTLCOLOR_SCROLLBAR)
    {
        HBRUSH hb = GetSysColorBrush(COLOR_SCROLLBAR);
        COLORREF bk = GetSysColor(COLOR_3DHILIGHT);
        SetTextColor( hDC, GetSysColor(COLOR_3DFACE));
        SetBkColor( hDC, bk);

        /* if COLOR_WINDOW happens to be the same as COLOR_3DHILIGHT
         * we better use 0x55aa bitmap brush to make scrollbar's background
         * look different from the window background.
         */
        if (bk == GetSysColor(COLOR_WINDOW))
            return SYSCOLOR_Get55AABrush();

        UnrealizeObject( hb );
        return hb;
    }

    SetTextColor( hDC, GetSysColor(COLOR_WINDOWTEXT));

    if ((ctlType == CTLCOLOR_EDIT) || (ctlType == CTLCOLOR_LISTBOX))
        SetBkColor( hDC, GetSysColor(COLOR_WINDOW) );
    else {
        SetBkColor( hDC, GetSysColor(COLOR_3DFACE) );
        return GetSysColorBrush(COLOR_3DFACE);
    }
    return GetSysColorBrush(COLOR_WINDOW);
}


/***********************************************************************
 *           DEFWND_Print
 *
 * This method handles the default behavior for the WM_PRINT message.
 */
static void DEFWND_Print( HWND hwnd, HDC hdc, ULONG uFlags)
{
  /*
   * Visibility flag.
   */
  if ( (uFlags & PRF_CHECKVISIBLE) &&
       !IsWindowVisible(hwnd) )
      return;

  /*
   * Unimplemented flags.
   */
  if ( (uFlags & PRF_CHILDREN) ||
       (uFlags & PRF_OWNED)    ||
       (uFlags & PRF_NONCLIENT) )
  {
    WARN("WM_PRINT message with unsupported flags\n");
  }

  /*
   * Background
   */
  if ( uFlags & PRF_ERASEBKGND)
    SendMessageW(hwnd, WM_ERASEBKGND, (WPARAM)hdc, 0);

  /*
   * Client area
   */
  if ( uFlags & PRF_CLIENT)
    SendMessageW(hwnd, WM_PRINTCLIENT, (WPARAM)hdc, uFlags);
}



/***********************************************************************
 *           DEFWND_DefWinProc
 *
 * Default window procedure for messages that are the same in Ansi and Unicode.
 */
static LRESULT DEFWND_DefWinProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch(msg)
    {
    case WM_NCMOUSEMOVE:
        return NC_HandleNCMouseMove( hwnd, wParam, lParam );

    case WM_NCMOUSELEAVE:
        return NC_HandleNCMouseLeave( hwnd );

    case WM_RBUTTONUP:
        {
            POINT pt;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            ClientToScreen(hwnd, &pt);
            SendMessageW( hwnd, WM_CONTEXTMENU, (WPARAM)hwnd, MAKELPARAM(pt.x, pt.y) );
        }
        break;

    case WM_NCRBUTTONUP:
        /*
         * FIXME : we must NOT send WM_CONTEXTMENU on a WM_NCRBUTTONUP (checked
         * in Windows), but what _should_ we do? According to MSDN :
         * "If it is appropriate to do so, the system sends the WM_SYSCOMMAND
         * message to the window". When is it appropriate?
         */
        break;

    case WM_XBUTTONUP:
    case WM_NCXBUTTONUP:
        if (HIWORD(wParam) == XBUTTON1 || HIWORD(wParam) == XBUTTON2)
        {
            SendMessageW(hwnd, WM_APPCOMMAND, (WPARAM)hwnd,
                         MAKELPARAM(LOWORD(wParam), FAPPCOMMAND_MOUSE | HIWORD(wParam)));
        }
        break;

    case WM_PRINT:
        DEFWND_Print(hwnd, (HDC)wParam, lParam);
        return 0;

    case WM_SYSCOMMAND:
        return NC_HandleSysCommand( hwnd, wParam, lParam );

    case WM_SHOWWINDOW:
        {
            LONG style = GetWindowLongW( hwnd, GWL_STYLE );
            WND *pWnd;
            if (!lParam) return 0; /* sent from ShowWindow */
            if ((style & WS_VISIBLE) && wParam) return 0;
            if (!(style & WS_VISIBLE) && !wParam) return 0;
            if (!GetWindow( hwnd, GW_OWNER )) return 0;
            if (!(pWnd = WIN_GetPtr( hwnd ))) return 0;
            if (pWnd == WND_OTHER_PROCESS) return 0;
            if (wParam)
            {
                if (!(pWnd->flags & WIN_NEEDS_SHOW_OWNEDPOPUP))
                {
                    WIN_ReleasePtr( pWnd );
                    return 0;
                }
                pWnd->flags &= ~WIN_NEEDS_SHOW_OWNEDPOPUP;
            }
            else pWnd->flags |= WIN_NEEDS_SHOW_OWNEDPOPUP;
            WIN_ReleasePtr( pWnd );
            NtUserShowWindow( hwnd, wParam ? SW_SHOWNOACTIVATE : SW_HIDE );
            break;
        }

    case WM_VKEYTOITEM:
    case WM_CHARTOITEM:
        return -1;

    case WM_DROPOBJECT:
        return DRAG_FILE;

    case WM_QUERYDROPOBJECT:
        return (GetWindowLongA( hwnd, GWL_EXSTYLE ) & WS_EX_ACCEPTFILES) != 0;

    case WM_QUERYDRAGICON:
        {
            UINT len;

            HICON hIcon = (HICON)GetClassLongPtrW( hwnd, GCLP_HICON );
            HINSTANCE instance = (HINSTANCE)GetWindowLongPtrW( hwnd, GWLP_HINSTANCE );
            if (hIcon) return (LRESULT)hIcon;
            for(len=1; len<64; len++)
                if((hIcon = LoadIconW(instance, MAKEINTRESOURCEW(len))))
                    return (LRESULT)hIcon;
            return (LRESULT)LoadIconW(0, (LPWSTR)IDI_APPLICATION);
        }
        break;

    case WM_ISACTIVEICON:
        return (win_get_flags( hwnd ) & WIN_NCACTIVATED) != 0;

    case WM_NOTIFYFORMAT:
      if (IsWindowUnicode(hwnd)) return NFR_UNICODE;
      else return NFR_ANSI;

    case WM_QUERYOPEN:
    case WM_QUERYENDSESSION:
        return 1;

    case WM_HELP:
        SendMessageW( GetParent(hwnd), msg, wParam, lParam );
        break;

    case WM_STYLECHANGED:
        if (wParam == GWL_STYLE && (GetWindowLongW( hwnd, GWL_EXSTYLE ) & WS_EX_LAYERED))
        {
            STYLESTRUCT *style = (STYLESTRUCT *)lParam;
            if ((style->styleOld ^ style->styleNew) & (WS_CAPTION|WS_THICKFRAME|WS_VSCROLL|WS_HSCROLL))
                NtUserSetWindowPos( hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER |
                                    SWP_NOSIZE | SWP_NOMOVE | SWP_NOCLIENTSIZE | SWP_NOCLIENTMOVE );
        }
        break;

    case WM_INPUTLANGCHANGEREQUEST:
        NtUserActivateKeyboardLayout( (HKL)lParam, 0 );
        break;

    case WM_INPUTLANGCHANGE:
        {
            struct user_thread_info *info = get_user_thread_info();
            int count = 0;
            HWND *win_array = WIN_ListChildren( hwnd );
            info->kbd_layout = (HKL)lParam;

            if (!win_array)
                break;
            while (win_array[count])
                SendMessageW( win_array[count++], WM_INPUTLANGCHANGE, wParam, lParam);
            HeapFree(GetProcessHeap(),0,win_array);
            break;
        }

    default:
        return NtUserMessageCall( hwnd, msg, wParam, lParam, 0, NtUserDefWindowProc, FALSE );

    }

    return 0;
}

static LPARAM DEFWND_GetTextA( WND *wndPtr, LPSTR dest, WPARAM wParam )
{
    LPARAM result = 0;

    __TRY
    {
        if (wndPtr->text)
        {
            if (!WideCharToMultiByte( CP_ACP, 0, wndPtr->text, -1,
                                      dest, wParam, NULL, NULL )) dest[wParam-1] = 0;
            result = strlen( dest );
        }
        else dest[0] = '\0';
    }
    __EXCEPT_PAGE_FAULT
    {
        return 0;
    }
    __ENDTRY
    return result;
}

/***********************************************************************
 *              DefWindowProcA (USER32.@)
 *
 * See DefWindowProcW.
 */
LRESULT WINAPI DefWindowProcA( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    LRESULT result = 0;
    HWND full_handle;

    if (!(full_handle = WIN_IsCurrentProcess( hwnd )))
    {
        if (!IsWindow( hwnd )) return 0;
        ERR( "called for other process window %p\n", hwnd );
        return 0;
    }
    hwnd = full_handle;

    SPY_EnterMessage( SPY_DEFWNDPROC, hwnd, msg, wParam, lParam );

    switch(msg)
    {
    case WM_NCCREATE:
        if (lParam)
        {
            CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;

            result = NtUserMessageCall( hwnd, msg, wParam, lParam, 0, NtUserDefWindowProc, TRUE );

            if(cs->style & (WS_HSCROLL | WS_VSCROLL))
            {
                SCROLLINFO si = {sizeof si, SIF_ALL, 0, 100, 0, 0, 0};
                SetScrollInfo( hwnd, SB_HORZ, &si, FALSE );
                SetScrollInfo( hwnd, SB_VERT, &si, FALSE );
            }
        }
        break;

    case WM_GETTEXTLENGTH:
        {
            WND *wndPtr = WIN_GetPtr( hwnd );
            if (wndPtr && wndPtr->text)
                result = WideCharToMultiByte( CP_ACP, 0, wndPtr->text, lstrlenW(wndPtr->text),
                                              NULL, 0, NULL, NULL );
            WIN_ReleasePtr( wndPtr );
        }
        break;

    case WM_GETTEXT:
        if (wParam)
        {
            LPSTR dest = (LPSTR)lParam;
            WND *wndPtr = WIN_GetPtr( hwnd );

            if (!wndPtr) break;
            result = DEFWND_GetTextA( wndPtr, dest, wParam );

            WIN_ReleasePtr( wndPtr );
        }
        break;

    case WM_SETTEXT:
    case WM_SYSCHAR:
        result = NtUserMessageCall( hwnd, msg, wParam, lParam, 0, NtUserDefWindowProc, TRUE );
        break;

    case WM_IME_CHAR:
        if (HIBYTE(wParam)) PostMessageA( hwnd, WM_CHAR, HIBYTE(wParam), lParam );
        PostMessageA( hwnd, WM_CHAR, LOBYTE(wParam), lParam );
        break;

    case WM_IME_KEYDOWN:
        result = PostMessageA( hwnd, WM_KEYDOWN, wParam, lParam );
        break;

    case WM_IME_KEYUP:
        result = PostMessageA( hwnd, WM_KEYUP, wParam, lParam );
        break;

    case WM_IME_COMPOSITION:
        if (lParam & GCS_RESULTSTR)
        {
            LONG size, i;
            unsigned char lead = 0;
            char *buf = NULL;
            HIMC himc = ImmGetContext( hwnd );

            if (himc)
            {
                if ((size = ImmGetCompositionStringA( himc, GCS_RESULTSTR, NULL, 0 )))
                {
                    if (!(buf = HeapAlloc( GetProcessHeap(), 0, size ))) size = 0;
                    else size = ImmGetCompositionStringA( himc, GCS_RESULTSTR, buf, size );
                }
                ImmReleaseContext( hwnd, himc );

                for (i = 0; i < size; i++)
                {
                    unsigned char c = buf[i];
                    if (!lead)
                    {
                        if (IsDBCSLeadByte( c ))
                            lead = c;
                        else
                            SendMessageA( hwnd, WM_IME_CHAR, c, 1 );
                    }
                    else
                    {
                        SendMessageA( hwnd, WM_IME_CHAR, MAKEWORD(c, lead), 1 );
                        lead = 0;
                    }
                }
                HeapFree( GetProcessHeap(), 0, buf );
            }
        }
        /* fall through */
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_SELECT:
    case WM_IME_NOTIFY:
    case WM_IME_CONTROL:
        {
            HWND hwndIME = ImmGetDefaultIMEWnd( hwnd );
            if (hwndIME)
                result = SendMessageA( hwndIME, msg, wParam, lParam );
        }
        break;
    case WM_IME_SETCONTEXT:
        {
            HWND hwndIME = ImmGetDefaultIMEWnd( hwnd );
            if (hwndIME) result = ImmIsUIMessageA( hwndIME, msg, wParam, lParam );
        }
        break;

    default:
        result = DEFWND_DefWinProc( hwnd, msg, wParam, lParam );
        break;
    }

    SPY_ExitMessage( SPY_RESULT_DEFWND, hwnd, msg, result, wParam, lParam );
    return result;
}


static LPARAM DEFWND_GetTextW( WND *wndPtr, LPWSTR dest, WPARAM wParam )
{
    LPARAM result = 0;

    __TRY
    {
        if (wndPtr->text)
        {
            lstrcpynW( dest, wndPtr->text, wParam );
            result = lstrlenW( dest );
        }
        else dest[0] = '\0';
    }
    __EXCEPT_PAGE_FAULT
    {
        return 0;
    }
    __ENDTRY

    return result;
}

/***********************************************************************
 *              DefWindowProcW (USER32.@) Calls default window message handler
 *
 * Calls default window procedure for messages not processed
 *  by application.
 *
 *  RETURNS
 *     Return value is dependent upon the message.
*/
LRESULT WINAPI DefWindowProcW(
    HWND hwnd,      /* [in] window procedure receiving message */
    UINT msg,       /* [in] message identifier */
    WPARAM wParam,  /* [in] first message parameter */
    LPARAM lParam )   /* [in] second message parameter */
{
    LRESULT result = 0;
    HWND full_handle;

    if (!(full_handle = WIN_IsCurrentProcess( hwnd )))
    {
        if (!IsWindow( hwnd )) return 0;
        ERR( "called for other process window %p\n", hwnd );
        return 0;
    }
    hwnd = full_handle;
    SPY_EnterMessage( SPY_DEFWNDPROC, hwnd, msg, wParam, lParam );

    switch(msg)
    {
    case WM_NCCREATE:
        if (lParam)
        {
            CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;

            result = NtUserMessageCall( hwnd, msg, wParam, lParam, 0, NtUserDefWindowProc, FALSE );

            if(cs->style & (WS_HSCROLL | WS_VSCROLL))
            {
                SCROLLINFO si = {sizeof si, SIF_ALL, 0, 100, 0, 0, 0};
                SetScrollInfo( hwnd, SB_HORZ, &si, FALSE );
                SetScrollInfo( hwnd, SB_VERT, &si, FALSE );
            }
        }
        break;

    case WM_GETTEXTLENGTH:
        {
            WND *wndPtr = WIN_GetPtr( hwnd );
            if (wndPtr && wndPtr->text) result = (LRESULT)lstrlenW(wndPtr->text);
            WIN_ReleasePtr( wndPtr );
        }
        break;

    case WM_GETTEXT:
        if (wParam)
        {
            LPWSTR dest = (LPWSTR)lParam;
            WND *wndPtr = WIN_GetPtr( hwnd );

            if (!wndPtr) break;
            result = DEFWND_GetTextW( wndPtr, dest, wParam );
            WIN_ReleasePtr( wndPtr );
        }
        break;

    case WM_IME_CHAR:
        PostMessageW( hwnd, WM_CHAR, wParam, lParam );
        break;

    case WM_IME_KEYDOWN:
        result = PostMessageW( hwnd, WM_KEYDOWN, wParam, lParam );
        break;

    case WM_IME_KEYUP:
        result = PostMessageW( hwnd, WM_KEYUP, wParam, lParam );
        break;

    case WM_IME_SETCONTEXT:
        {
            HWND hwndIME = ImmGetDefaultIMEWnd( hwnd );
            if (hwndIME) result = ImmIsUIMessageW( hwndIME, msg, wParam, lParam );
        }
        break;

    case WM_IME_COMPOSITION:
        if (lParam & GCS_RESULTSTR)
        {
            LONG size, i;
            WCHAR *buf = NULL;
            HIMC himc = ImmGetContext( hwnd );

            if (himc)
            {
                if ((size = ImmGetCompositionStringW( himc, GCS_RESULTSTR, NULL, 0 )))
                {
                    if (!(buf = HeapAlloc( GetProcessHeap(), 0, size * sizeof(WCHAR) ))) size = 0;
                    else size = ImmGetCompositionStringW( himc, GCS_RESULTSTR, buf, size * sizeof(WCHAR) );
                }
                ImmReleaseContext( hwnd, himc );

                for (i = 0; i < size / sizeof(WCHAR); i++)
                    SendMessageW( hwnd, WM_IME_CHAR, buf[i], 1 );
                HeapFree( GetProcessHeap(), 0, buf );
            }
        }
        /* fall through */
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_SELECT:
    case WM_IME_NOTIFY:
    case WM_IME_CONTROL:
        {
            HWND hwndIME = ImmGetDefaultIMEWnd( hwnd );
            if (hwndIME)
                result = SendMessageW( hwndIME, msg, wParam, lParam );
        }
        break;

    default:
        result = DEFWND_DefWinProc( hwnd, msg, wParam, lParam );
        break;
    }
    SPY_ExitMessage( SPY_RESULT_DEFWND, hwnd, msg, result, wParam, lParam );
    return result;
}
