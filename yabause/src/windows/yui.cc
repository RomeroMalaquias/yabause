/*  Copyright 2004 Guillaume Duhamel
    Copyright 2004-2005 Theo Berkau
    Copyright 2005 Joost Peters

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <windows.h>
#include <commctrl.h>
#include "SDL.h"
#include "../superh.hh"
#include "../sh2d.hh"
#include "../vdp2.hh"
#include "../scsp.hh"
#include "../scu.hh"
#include "../yui.hh"
#include "resource.h"
#include "settings.hh"
#include "cd.hh"

int stop;
int yabwinw;
int yabwinh;

char SDL_windowhack[32];
HINSTANCE y_hInstance;
HWND YabWin;

unsigned long mtrnssaddress=0x06004000;
unsigned long mtrnseaddress=0x06100000;
char mtrnsfilename[MAX_PATH] = "\0";
char mtrnsreadwrite=1;
bool mtrnssetpc=true;

unsigned long memaddr=0;

SaturnMemory *yabausemem;
SuperH *debugsh;

//bool shwaspaused=true;

// vdp2 related
char vdp2bppstr[8][10]=
{
"4-bit",
"8-bit",
"11-bit",
"16-bit",
"24-bit",
"Invalid",
"Invalid",
"Invalid"
};

char vdp2charsizestr[2][10]=
{
"1Hx1V",
"2Hx2V"
};

char vdp2bmsizestr[4][10]=
{
"512x256",
"512x512",
"1024x256",
"1024x512"
};

CDInterface *cd = 0;

LRESULT CALLBACK WindowProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
LRESULT CALLBACK MemTransferDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam);
LRESULT CALLBACK SH2DebugDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam);
LRESULT CALLBACK VDP2DebugDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam);
LRESULT CALLBACK M68KDebugDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam);
LRESULT CALLBACK SCUDSPDebugDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam);

char * yui_bios(void) {
        return biosfilename;
}

CDInterface *yui_cd(void) {
        return cd;
}

char * yui_saveram(void) {
        return backupramfilename;
}

char * yui_mpegrom(void) {
        return mpegromfilename;
}

unsigned char yui_region(void) {
        return regionid;
}

void yui_hide_show(void) {
}

void yui_quit(void) {
	stop = 1;
}

void yui_errormsg(Exception error, SuperH sh2opcodes) {
   cerr << error << endl;
   cerr << sh2opcodes << endl;
}

void yui_init(int (*yab_main)(void*)) {
	SaturnMemory *mem;
        WNDCLASS                    MyWndClass;
        HWND                        hWnd;
        DWORD inifilenamesize=0;
        char *pinifilename;
        static char szAppName[128];
        static char szClassName[] = "Yabause";
//        RECT                        workarearect;
//        DWORD ret;
        char tempstr[MAX_PATH];

        sprintf(szAppName, "Yabause %s\0", VERSION);

        y_hInstance = GetModuleHandle(NULL);

        // Set up and register window class
        MyWndClass.style = CS_HREDRAW | CS_VREDRAW;
        MyWndClass.lpfnWndProc = (WNDPROC) WindowProc;
        MyWndClass.cbClsExtra = 0;
        MyWndClass.cbWndExtra = sizeof(DWORD);
        MyWndClass.hInstance = y_hInstance;
        MyWndClass.hIcon = LoadIcon(y_hInstance, MAKEINTRESOURCE(IDI_ICON));
        MyWndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        MyWndClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
        MyWndClass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
        MyWndClass.lpszClassName = szClassName;

        if (!RegisterClass(&MyWndClass))
          return;

        // get program pathname
        inifilenamesize = GetModuleFileName(y_hInstance, inifilename, MAX_PATH);

        // set pointer to start of extension
        pinifilename = inifilename + inifilenamesize - 4;

        // replace .exe with .ini
        sprintf(pinifilename, ".ini\0");

        if (GetPrivateProfileString("General", "BiosPath", "", biosfilename, MAX_PATH, inifilename) == 0 ||
            GetPrivateProfileString("General", "CDROMDrive", "", cdrompath, MAX_PATH, inifilename) == 0)
        {
           // Startup Settings Configuration here
           if (DialogBox(y_hInstance, "SettingsDlg", NULL, (DLGPROC)SettingsDlgProc) != TRUE)
           {
              // exit program with error
              MessageBox (NULL, "yabause.ini must be properly setup before program can be used.", "Error",  MB_OK | MB_ICONINFORMATION);
              return;
           }
        }

        GetPrivateProfileString("General", "BackupRamPath", "", backupramfilename, MAX_PATH, inifilename);
        GetPrivateProfileString("General", "MpegRomPath", "", mpegromfilename, MAX_PATH, inifilename);

        // Grab Bios Language Settings
//        GetPrivateProfileString("General", "BiosLanguage", "", tempstr, MAX_PATH, inifilename);

        // Grab Region Settings
        GetPrivateProfileString("General", "Region", "", tempstr, MAX_PATH, inifilename);

        if (strlen(tempstr) == 1)
        {
           switch (tempstr[0])
           {
              case 'J':
                 regionid = 1;
                 break;
              case 'T':
                 regionid = 2;
                 break;
              case 'U':
                 regionid = 4;
                 break;
              case 'B':
                 regionid = 5;
                 break;
              case 'K':
                 regionid = 6;
                 break;
              case 'A':
                 regionid = 0xA;
                 break;
              case 'E':
                 regionid = 0xC;
                 break;
              case 'L':
                 regionid = 0xD;
                 break;
              default: break;
           }
        }
        else if (stricmp(tempstr, "AUTO") == 0)
           regionid = 0;

        // Figure out how much of the screen is useable
//        if (SystemParametersInfo(SPI_GETWORKAREA, 0, &workarearect, 0) == FALSE)
//        {
           // Since we can't retrieve it, use a default values
           yabwinw = 320 + GetSystemMetrics(SM_CXSIZEFRAME) * 2;
           yabwinh = 224 + (GetSystemMetrics(SM_CYSIZEFRAME) * 2) + GetSystemMetrics(SM_CYMENU) + GetSystemMetrics(SM_CYCAPTION);

//        }
//        else
//        {
//           // Calculate sizes that fit in the work area
//           yabwinw = workarearect.right - workarearect.left - (GetSystemMetrics(SM_CXSIZEFRAME) * 2);
//           yabwinh = workarearect.bottom - workarearect.top - ((GetSystemMetrics(SM_CYSIZEFRAME) * 2) + GetSystemMetrics(SM_CYMENU) + GetSystemMetrics(SM_CYCAPTION));
//        }

        // Create a window
        hWnd = CreateWindow(szClassName,        // class
                            szAppName,          // caption
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                            WS_THICKFRAME | WS_MINIMIZEBOX |  // style
                            WS_CLIPCHILDREN,  
                            CW_USEDEFAULT,      // x pos
                            CW_USEDEFAULT,      // y pos
                            yabwinw,        // width
                            yabwinh,       // height
                            HWND_DESKTOP,       // parent window
                            NULL,               // menu 
                            y_hInstance,          // instance
                            NULL);              // parms

        if (!hWnd)
            return;

        SetWindowPos(hWnd, HWND_TOP, 0, 0, yabwinw, yabwinh, SWP_NOREPOSITION);

        // may change this
        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);

        sprintf(SDL_windowhack,"SDL_WINDOWID=%ld", hWnd);
	putenv(SDL_windowhack);

        YabWin = hWnd;

	stop = 0;
//        cd = new DummyCDDrive();
        cd = new SPTICDDrive(cdrompath);
//        cd = new ASPICDDrive(cdrompath);
        mem = new SaturnMemory();
        yabausemem = mem;
        while (!stop) { yab_main(mem); }
	delete(mem);

}

LRESULT CALLBACK WindowProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
   switch (uMsg)
   {
      case WM_COMMAND:
      {
         switch (LOWORD(wParam))
         {
//            case IDM_RUN:
//            {
//               break;
//            }
            case IDM_MEMTRANSFER:
            {
               DialogBox(y_hInstance, "MemTransferDlg", hWnd, (DLGPROC)MemTransferDlgProc);
               break;
            }
            case IDM_SETTINGS:
            {
               DialogBox(y_hInstance, "SettingsDlg", hWnd, (DLGPROC)SettingsDlgProc);
               break;
            }
            case IDM_MSH2DEBUG:
            {
               debugsh = yabausemem->getMasterSH();
               DialogBox(y_hInstance, "SH2DebugDlg", hWnd, (DLGPROC)SH2DebugDlgProc);
               break;
            }
            case IDM_SSH2DEBUG:
            {
               debugsh = yabausemem->getSlaveSH();
               DialogBox(y_hInstance, "SH2DebugDlg", hWnd, (DLGPROC)SH2DebugDlgProc);
               break;
            }
            case IDM_VDP2DEBUG:
            {
               DialogBox(y_hInstance, "VDP2DebugDlg", hWnd, (DLGPROC)VDP2DebugDlgProc);
               break;
            }
            case IDM_M68KDEBUG:
            {
               DialogBox(y_hInstance, "M68KDebugDlg", hWnd, (DLGPROC)M68KDebugDlgProc);
               break;
            }
            case IDM_SCUDSPDEBUG:
            {
               DialogBox(y_hInstance, "SCUDSPDebugDlg", hWnd, (DLGPROC)SCUDSPDebugDlgProc);
               break;
            }
            case IDM_EXIT:
               PostMessage(hWnd, WM_CLOSE, 0, 0);
               break;
         }
         return 0L;
      }
      case WM_ENTERMENULOOP:
      {
         return 0L;
      }
      case WM_EXITMENULOOP:
      {
         return 0L;
      }
      case WM_SIZE:
      {
         SetWindowPos(hWnd, HWND_TOP, 0, 0, yabwinw, yabwinh, SWP_NOREPOSITION);
         return 0L;
      }
      case WM_PAINT:
      {
         PAINTSTRUCT ps;

         BeginPaint(hWnd, &ps);
         EndPaint(hWnd, &ps);
         return 0L;
      }
      case WM_DESTROY:
      {
         PostQuitMessage(0);

         return 0L;
      }

    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MemTransferDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam)
{
   char tempstr[256];

   switch (uMsg)
   {
      case WM_INITDIALOG:
      {
         SetDlgItemText(hDlg, IDC_EDITTEXT1, mtrnsfilename);

         sprintf(tempstr, "%08x\0", mtrnssaddress);
         SetDlgItemText(hDlg, IDC_EDITTEXT2, tempstr);

         sprintf(tempstr, "%08x\0", mtrnseaddress);
         SetDlgItemText(hDlg, IDC_EDITTEXT3, tempstr);

         if (mtrnsreadwrite == 0)
         {
            SendMessage(GetDlgItem(hDlg, IDC_DOWNLOADMEM), BM_SETCHECK, BST_CHECKED, 0);
            SendMessage(GetDlgItem(hDlg, IDC_UPLOADMEM), BM_SETCHECK, BST_UNCHECKED, 0);
            EnableWindow(HWND(GetDlgItem(hDlg, IDC_EDITTEXT3)), TRUE);
            EnableWindow(HWND(GetDlgItem(hDlg, IDC_CHECKBOX1)), FALSE);
         }
         else
         {
            SendMessage(GetDlgItem(hDlg, IDC_DOWNLOADMEM), BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessage(GetDlgItem(hDlg, IDC_UPLOADMEM), BM_SETCHECK, BST_CHECKED, 0);
            if (mtrnssetpc)
               SendMessage(GetDlgItem(hDlg, IDC_CHECKBOX1), BM_SETCHECK, BST_CHECKED, 0);
            EnableWindow(HWND(GetDlgItem(hDlg, IDC_EDITTEXT3)), FALSE);
            EnableWindow(HWND(GetDlgItem(hDlg, IDC_CHECKBOX1)), TRUE);
         }

         return TRUE;
      }
      case WM_COMMAND:
      {
         switch (LOWORD(wParam))
         {
            case IDC_BROWSE:
            {
               OPENFILENAME ofn;

               if (SendMessage(GetDlgItem(hDlg, IDC_DOWNLOADMEM), BM_GETCHECK, 0, 0) == BST_CHECKED)
               {
                  // setup ofn structure
                  ZeroMemory(&ofn, sizeof(ofn));
                  ofn.lStructSize = sizeof(ofn);
                  ofn.hwndOwner = hDlg;
                  ofn.lpstrFilter = "All Files\0*.*\0Binary Files\0*.BIN\0";
                  ofn.nFilterIndex = 1;
                  ofn.lpstrFile = mtrnsfilename;
                  ofn.nMaxFile = sizeof(mtrnsfilename);
                  ofn.Flags = OFN_OVERWRITEPROMPT;
 
                  if (GetSaveFileName(&ofn))
                  {
                     SetDlgItemText(hDlg, IDC_EDITTEXT1, mtrnsfilename);
                  }
               }
               else
               {
                  // setup ofn structure
                  ZeroMemory(&ofn, sizeof(OPENFILENAME));
                  ofn.lStructSize = sizeof(OPENFILENAME);
                  ofn.hwndOwner = hDlg;
                  ofn.lpstrFilter = "All Files\0*.*\0Binary Files\0*.BIN\0";
                  ofn.nFilterIndex = 1;
                  ofn.lpstrFile = mtrnsfilename;
                  ofn.nMaxFile = sizeof(mtrnsfilename);
                  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                  if (GetOpenFileName(&ofn))
                  {
                     SetDlgItemText(hDlg, IDC_EDITTEXT1, mtrnsfilename);
                  }
               }

               return TRUE;
            }
            case IDOK:
            {
               GetDlgItemText(hDlg, IDC_EDITTEXT1, mtrnsfilename, MAX_PATH);

               GetDlgItemText(hDlg, IDC_EDITTEXT2, tempstr, 9);
               sscanf(tempstr, "%08x", &mtrnssaddress);

               GetDlgItemText(hDlg, IDC_EDITTEXT3, tempstr, 9);
               sscanf(tempstr, "%08x", &mtrnseaddress);

               if ((mtrnseaddress - mtrnssaddress) < 0)
               {
                  MessageBox (hDlg, "Invalid Start/End Address Combination", "Error",  MB_OK | MB_ICONINFORMATION);
                  EndDialog(hDlg, TRUE);
                  return FALSE;
               }

               if (SendMessage(GetDlgItem(hDlg, IDC_CHECKBOX1), BM_GETCHECK, 0, 0) == BST_CHECKED)
               {
                  SuperH *proc=yabausemem->getMasterSH();
                  sh2regs_struct sh2regs;
                  proc->GetRegisters(&sh2regs);
                  sh2regs.PC = mtrnssaddress;
                  proc->SetRegisters(&sh2regs);

                  mtrnssetpc = true;
               }
               else
                  mtrnssetpc = false;

               if (SendMessage(GetDlgItem(hDlg, IDC_DOWNLOADMEM), BM_GETCHECK, 0, 0) == BST_CHECKED)
               {
                  // Let's do a ram dump
                  yabausemem->save(mtrnsfilename, mtrnssaddress, mtrnseaddress - mtrnssaddress);
                  mtrnsreadwrite = 0;
               }
               else
               {
                  // upload to ram and possibly execute
                  mtrnsreadwrite = 1;

                  // Is this a program?
                  if (SendMessage(GetDlgItem(hDlg, IDC_CHECKBOX1), BM_GETCHECK, 0, 0) == BST_CHECKED)
                     yabausemem->loadExec(mtrnsfilename, mtrnssaddress);
                  else
                     yabausemem->load(mtrnsfilename, mtrnssaddress);
               }

               EndDialog(hDlg, TRUE);

               return TRUE;
            }
            case IDCANCEL:
            {
               EndDialog(hDlg, FALSE);

               return TRUE;
            }
            case IDC_UPLOADMEM:
            {
               if (HIWORD(wParam) == BN_CLICKED)
               {
                  EnableWindow(HWND(GetDlgItem(hDlg, IDC_EDITTEXT3)), FALSE);
                  EnableWindow(HWND(GetDlgItem(hDlg, IDC_CHECKBOX1)), TRUE);
               }

               break;
            }
            case IDC_DOWNLOADMEM:
            {
               if (HIWORD(wParam) == BN_CLICKED)
               {
                  EnableWindow(HWND(GetDlgItem(hDlg, IDC_EDITTEXT3)), TRUE);
                  EnableWindow(HWND(GetDlgItem(hDlg, IDC_CHECKBOX1)), FALSE);
               }
               break;
            }
            default: break;
         }
         break;
      }

      default: break;
   }

   return FALSE;
}

void SH2UpdateRegList(HWND hDlg, sh2regs_struct *regs)
{
   char tempstr[128];
   int i;

   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_RESETCONTENT, 0, 0);

   for (i = 0; i < 16; i++)
   {                                       
      sprintf(tempstr, "R%02d =  %08x\0", i, regs->R[i]);
      strupr(tempstr);
      SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);
   }

   // SR
   sprintf(tempstr, "SR =   %08x\0", regs->SR.all);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   // GBR
   sprintf(tempstr, "GBR =  %08x\0", regs->GBR);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   // VBR
   sprintf(tempstr, "VBR =  %08x\0", regs->VBR);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   // MACH
   sprintf(tempstr, "MACH = %08x\0", regs->MACH);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   // MACL
   sprintf(tempstr, "MACL = %08x\0", regs->MACL);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   // PR
   sprintf(tempstr, "PR =   %08x\0", regs->PR);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   // PC
   sprintf(tempstr, "PC =   %08x\0", regs->PC);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);
}

void SH2UpdateCodeList(HWND hDlg, unsigned long addr)
{
   int i;
   char buf[60];
   unsigned long offset;
//   SuperH *proc=yabausemem->getSH();

   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX2), LB_RESETCONTENT, 0, 0);

   offset = addr - (12 * 2);

   for (i=0; i < 24; i++) // amount of lines
   {
//      SH2Disasm(offset, proc->getMemory()->getWord(offset), 0, buf);
      SH2Disasm(offset, yabausemem->getWord(offset), 0, buf);

      SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX2)), LB_ADDSTRING, 0,
                  (long)buf);
      offset += 2;
   }

   SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX2)), LB_SETCURSEL,12,0);
}

LRESULT CALLBACK MemDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam)
{
   switch (uMsg)
   {
      case WM_INITDIALOG:
      {
         char buf[8];

         sprintf(buf, "%08X",memaddr);
         SetDlgItemText(hDlg, IDC_EDITTEXT1, buf);
         return TRUE;
      }
      case WM_COMMAND:
      {
         switch (wParam)
         {
            case IDOK:
            {
               char buf[9];

               EndDialog(hDlg, TRUE);
               GetDlgItemText(hDlg, IDC_EDITTEXT1, buf, 11);

               sscanf(buf, "%08x", &memaddr);

               return TRUE;
            }
            case IDCANCEL:
            {
               EndDialog(hDlg, FALSE);
               return TRUE;
            }
            default: break;
         }
         break;
      }
   }

   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

void BreakpointHandler (bool slave, unsigned long addr)
{
   MessageBox (NULL, "Breakpoint Reached", "Notice",  MB_OK | MB_ICONINFORMATION);

   if (!slave)
   {
      debugsh = yabausemem->getMasterSH();
      DialogBox(y_hInstance, "SH2DebugDlg", YabWin, (DLGPROC)SH2DebugDlgProc);
   }
   else
   {
      debugsh = yabausemem->getSlaveSH();
      DialogBox(y_hInstance, "SH2DebugDlg", YabWin, (DLGPROC)SH2DebugDlgProc);
   }
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK SH2DebugDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam)
{
   switch (uMsg)
   {
      case WM_INITDIALOG:
      {
         sh2regs_struct sh2regs;
         codebreakpoint_struct *cbp;
         char tempstr[10];

         cbp = debugsh->GetBreakpointList();

         for (int i = 0; i < MAX_BREAKPOINTS; i++)
         {
            if (cbp[i].addr != 0xFFFFFFFF)
            {
               sprintf(tempstr, "%08X", cbp[i].addr);
               SendMessage(GetDlgItem(hDlg, IDC_LISTBOX3), LB_ADDSTRING, 0, (LPARAM)tempstr);
            }
         }

//         if (proc->paused())
//         {
            debugsh->GetRegisters(&sh2regs);
            SH2UpdateRegList(hDlg, &sh2regs);
            SH2UpdateCodeList(hDlg, sh2regs.PC);
//         }

         debugsh->SetBreakpointCallBack(&BreakpointHandler);

         return TRUE;
      }
      case WM_COMMAND:
      {
         switch (LOWORD(wParam))
         {
            case IDOK:
            {
               EndDialog(hDlg, TRUE);

               return TRUE;
            }
            case IDC_STEP:
            {
               sh2regs_struct sh2regs;
               debugsh->step();
               debugsh->GetRegisters(&sh2regs);
               SH2UpdateRegList(hDlg, &sh2regs);
               SH2UpdateCodeList(hDlg, sh2regs.PC);
               break;
            }
            case IDC_STEPOVER:
            {
               break;
            }
            case IDC_MEMTRANSFER:
            {
               DialogBox(y_hInstance, "MemTransferDlg", hDlg, (DLGPROC)MemTransferDlgProc);
               break;
            }
            case IDC_ADDBP1:
            {
               char bptext[10];
               unsigned long addr=0;
               memset(bptext, 0, 10);
               GetDlgItemText(hDlg, IDC_EDITTEXT1, bptext, 10);

               if (bptext[0] != 0)
               {
                  sscanf(bptext, "%X", &addr);
                  sprintf(bptext, "%08X", addr);

                  if (debugsh->AddCodeBreakpoint(addr) == 0)
                     SendMessage(GetDlgItem(hDlg, IDC_LISTBOX3), LB_ADDSTRING, 0, (LPARAM)bptext);
               }

               break;
            }
            case IDC_DELBP1:
            {
               LRESULT ret;
               char bptext[10];
               unsigned long addr=0;

               if ((ret = SendMessage(GetDlgItem(hDlg, IDC_LISTBOX3), LB_GETCURSEL, 0, 0)) != LB_ERR)
               {
                  SendMessage(GetDlgItem(hDlg, IDC_LISTBOX3), LB_GETTEXT, 0, (LPARAM)bptext);
                  sscanf(bptext, "%X", &addr);
                  debugsh->DelCodeBreakpoint(addr);
                  SendMessage(GetDlgItem(hDlg, IDC_LISTBOX3), LB_DELETESTRING, ret, 0);
               }

               break;
            }
            case IDC_LISTBOX1:
            {
               switch (HIWORD(wParam))
               {
                  case LBN_DBLCLK:
                  {
                     // dialogue for changing register values
                     int cursel;

                     sh2regs_struct sh2regs;
                     debugsh->GetRegisters(&sh2regs);

                     cursel = SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX1)), LB_GETCURSEL,0,0);

                     if (cursel < 16)
                     {
                        memaddr = sh2regs.R[cursel];
                     }
                     else if (cursel == 16)
                     {
                        memaddr = sh2regs.SR.all;
                     }
                     else if (cursel == 17)
                     {
                        memaddr = sh2regs.GBR;
                     }
                     else if (cursel == 18)
                     {
                        memaddr = sh2regs.VBR;
                     }
                     else if (cursel == 19)
                     {
                        memaddr = sh2regs.MACH;
                     }
                     else if (cursel == 20)
                     {
                        memaddr = sh2regs.MACL;
                     }
                     else if (cursel == 21)
                     {
                        memaddr = sh2regs.PR;
                     }
                     else if (cursel == 22)
                     {
                        memaddr = sh2regs.PC;
                     }

                     if (DialogBox(GetModuleHandle(0), "MemDlg", hDlg, (DLGPROC)MemDlgProc) != FALSE)
                     {
                        if (cursel < 16)
                        {
                           sh2regs.R[cursel] = memaddr;
                        }
                        else if (cursel == 16)
                        {
                           sh2regs.SR.all = memaddr;
                        }
                        else if (cursel == 17)
                        {
                           sh2regs.GBR = memaddr;
                        }
                        else if (cursel == 18)
                        {
                           sh2regs.VBR = memaddr;
                        }
                        else if (cursel == 19)
                        {
                           sh2regs.MACH = memaddr;
                        }
                        else if (cursel == 20)
                        {
                           sh2regs.MACL = memaddr;
                        }
                        else if (cursel == 21)
                        {
                           sh2regs.PR = memaddr;
                        }
                        else if (cursel == 22)
                        {
                           sh2regs.PC = memaddr;
                           SH2UpdateCodeList(hDlg, sh2regs.PC);
                        }
                     }

                     debugsh->SetRegisters(&sh2regs);
                     SH2UpdateRegList(hDlg, &sh2regs);

                     break;
                  }
                  default: break;
               }

               break;
            }

            default: break;
         }
         break;
      }
      default: break;
   }

   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

void DisplayScreenCCRInfo(HWND hControl, unsigned char reg_data)
{
   char tempstr[256];

   // bpp
   SendMessage(hControl, LB_ADDSTRING, 0, (LPARAM)vdp2bppstr[(reg_data & 0x0070) >> 4]);

   // Bitmap or Tile mode?
   if (reg_data & 0x0002)
   {
      // Bitmap
      sprintf(tempstr, "Bitmap(%s)", vdp2bmsizestr[(reg_data & 0x000C) >> 2]);
      SendMessage(hControl, LB_ADDSTRING, 0, (LPARAM)tempstr);
   }
   else
   {
      // Tile
      sprintf(tempstr, "Tile(%s)", vdp2charsizestr[reg_data & 1]);
      SendMessage(hControl, LB_ADDSTRING, 0, (LPARAM)tempstr);
   }
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDP2DebugDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam)
{
   switch (uMsg)
   {
      case WM_INITDIALOG:
      {
         Vdp2 *proc=yabausemem->vdp2_3;
         unsigned long reg;
         char tempstr[1024];
         bool isscrenabled;

         // is NBG0/RBG1 enabled?
         ((NBG0 *)proc->getNBG0())->debugStats(tempstr, &isscrenabled);

         if (isscrenabled)
         {
            SendMessage(GetDlgItem(hDlg, IDC_NBG0ENABCB), BM_SETCHECK, BST_CHECKED, 0);
            SetDlgItemText(hDlg, IDC_NBG0LB, tempstr);
         }
         else
         {
            SendMessage(GetDlgItem(hDlg, IDC_NBG0ENABCB), BM_SETCHECK, BST_UNCHECKED, 0);
         }

         ((NBG1 *)proc->getNBG1())->debugStats(tempstr, &isscrenabled);

         // is NBG1 enabled?
         if (isscrenabled)
         {
            // enabled
            SendMessage(GetDlgItem(hDlg, IDC_NBG1ENABCB), BM_SETCHECK, BST_CHECKED, 0);
            SetDlgItemText(hDlg, IDC_NBG1LB, tempstr);
         }
         else
         {
            // disabled
            SendMessage(GetDlgItem(hDlg, IDC_NBG1ENABCB), BM_SETCHECK, BST_UNCHECKED, 0);
         }

         ((NBG2 *)proc->getNBG2())->debugStats(tempstr, &isscrenabled);

         // is NBG2 enabled?
         if (isscrenabled)
         {
            // enabled
            SendMessage(GetDlgItem(hDlg, IDC_NBG2ENABCB), BM_SETCHECK, BST_CHECKED, 0);
            SetDlgItemText(hDlg, IDC_NBG2LB, tempstr);
         }
         else
         {
            // disabled
            SendMessage(GetDlgItem(hDlg, IDC_NBG2ENABCB), BM_SETCHECK, BST_UNCHECKED, 0);
         }

         ((NBG3 *)proc->getNBG3())->debugStats(tempstr, &isscrenabled);

         // is NBG3 enabled?
         if (isscrenabled)
         {
            // enabled
            SendMessage(GetDlgItem(hDlg, IDC_NBG3ENABCB), BM_SETCHECK, BST_CHECKED, 0);
            SetDlgItemText(hDlg, IDC_NBG3LB, tempstr);
         }
         else
         {
            // disabled
            SendMessage(GetDlgItem(hDlg, IDC_NBG3ENABCB), BM_SETCHECK, BST_UNCHECKED, 0);
         }

         // is RBG0 enabled?
         if (proc->getWord(0x20) & 0x10)
         {
            // enabled
            SendMessage(GetDlgItem(hDlg, IDC_RBG0ENABCB), BM_SETCHECK, BST_CHECKED, 0);

            // Generate Info for RBG0
            DisplayScreenCCRInfo(GetDlgItem(hDlg, IDC_RBG0LB), proc->getWord(0x2A) >> 8);
         }
         else
         {
            // disabled
            SendMessage(GetDlgItem(hDlg, IDC_RBG0ENABCB), BM_SETCHECK, BST_UNCHECKED, 0);
         }

         return TRUE;
      }
      case WM_COMMAND:
      {
         switch (LOWORD(wParam))
         {
            case IDOK:
            {
               EndDialog(hDlg, TRUE);

               return TRUE;
            }

            default: break;
         }
         break;
      }
      default: break;
   }

   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

void M68KUpdateRegList(HWND hDlg, m68kregs_struct *regs)
{
   char tempstr[128];
   int i;

   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_RESETCONTENT, 0, 0);

   // Data registers
   for (i = 0; i < 8; i++)
   {
      sprintf(tempstr, "D%d =   %08x\0", i, regs->D[i]);
      strupr(tempstr);
      SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);
   }

   // Address registers
   for (i = 0; i < 8; i++)
   {
      sprintf(tempstr, "A%d =   %08x\0", i, regs->A[i]);
      strupr(tempstr);
      SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);
   }

   // SR
   sprintf(tempstr, "SR =   %08x\0", regs->SR);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   // PC
   sprintf(tempstr, "PC =   %08x\0", regs->PC);
   strupr(tempstr);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);
}

//////////////////////////////////////////////////////////////////////////////

void M68KUpdateCodeList(HWND hDlg, unsigned long addr)
{
/*
   unsigned long buf_size;
   unsigned long buf_addr;
   int i, i2;
   char buf[60];
   unsigned long offset;
   char op[64], inst[32], arg[24];
   unsigned char *buffer;
   unsigned long op_size;

   buffer = ((ScspRam *)((Scsp *)mem->soundr)->getSRam())->getBuffer();
        
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX2), LB_RESETCONTENT, 0, 0);

//   offset = addr - (12 * 2);
   offset = addr;

   for (i = 0; i < 24; i++)
   {
      op_size += Dis68000One(offset, &buffer[offset], op, inst, arg);
      sprintf(buf, "%06x: %s %s\0", offset, inst, arg);
      offset += op_size;

      SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX2)), LB_ADDSTRING, 0,
                  (long)buf);
   }

//   SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX2)), LB_SETCURSEL,12,0);
   SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX2)), LB_SETCURSEL,0,0);
*/
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK M68KDebugDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam)
{
   switch (uMsg)
   {
      case WM_INITDIALOG:
      {
         m68kregs_struct m68kregs;

/*
         SendMessage(GetDlgItem(hDlg, IDC_CHKREAD), BM_SETCHECK, BST_UNCHECKED, 0);
         SendMessage(GetDlgItem(hDlg, IDC_CHKWRITE), BM_SETCHECK, BST_UNCHECKED, 0);
         SendMessage(GetDlgItem(hDlg, IDC_CHKBYTE), BM_SETCHECK, BST_UNCHECKED, 0);
         SendMessage(GetDlgItem(hDlg, IDC_CHKWORD), BM_SETCHECK, BST_UNCHECKED, 0);
         SendMessage(GetDlgItem(hDlg, IDC_CHKDWORD), BM_SETCHECK, BST_UNCHECKED, 0);
*/

         EnableWindow(HWND(GetDlgItem(hDlg, IDC_STEP)), TRUE);
//         EnableWindow(HWND(GetDlgItem(hDlg, IDC_STEPOVER)), TRUE);

         ((Scsp*)yabausemem->soundr)->Get68kRegisters(&m68kregs);
         M68KUpdateRegList(hDlg, &m68kregs);
         M68KUpdateCodeList(hDlg, m68kregs.PC);

         return TRUE;
      }
      case WM_COMMAND:
      {
         switch (LOWORD(wParam))
         {
            case IDOK:
            {
/*
               if (scsprunthreadhandle != INVALID_HANDLE_VALUE)
               {
                  killScspRunThread=1;

                  // wait for thread to end(should really set it to timeout after a
                  // certain time so I can test for keypresses)
                  WaitForSingleObject(scsprunthreadhandle,INFINITE);
                  CloseHandle(scsprunthreadhandle);                                           
                  scsprunthreadhandle = INVALID_HANDLE_VALUE;
               }
*/

               EndDialog(hDlg, TRUE);

               return TRUE;
            }
            case IDC_STEP:
            {
               m68kregs_struct m68kregs;

               // execute instruction
               ((Scsp*)yabausemem->soundr)->step68k();

               ((Scsp*)yabausemem->soundr)->Get68kRegisters(&m68kregs);
               M68KUpdateRegList(hDlg, &m68kregs);
               M68KUpdateCodeList(hDlg, m68kregs.PC);

               break;
            }
            case IDC_ADDBP1:
            {
               // add a code breakpoint
               break;
            }
            case IDC_DELBP1:
            {
               // delete a code breakpoint
               break;
            }
/*
            case IDC_ADDBP2:
            {
               // add a memory breakpoint
               break;
            }
*/
            case IDC_LISTBOX1:
            {
               switch (HIWORD(wParam))
               {
                  case LBN_DBLCLK:
                  {
                     // dialogue for changing register values
                     int cursel;
                     m68kregs_struct m68kregs;

                     cursel = SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX1)), LB_GETCURSEL,0,0);

                     ((Scsp*)yabausemem->soundr)->Get68kRegisters(&m68kregs);

                     switch (cursel)
                     {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                        case 4:
                        case 5:
                        case 6:
                        case 7:
                           memaddr = m68kregs.D[cursel];                           
                           break;
                        case 8:
                        case 9:
                        case 10:
                        case 11:
                        case 12:
                        case 13:
                        case 14:
                        case 15:
                           memaddr = m68kregs.A[cursel - 8];
                           break;
                        case 16:
                           memaddr = m68kregs.SR;
                           break;
                        case 17:
                           memaddr = m68kregs.PC;
                           break;
                        default: break;
                     }

                     if (DialogBox(y_hInstance, "MemDlg", hDlg, (DLGPROC)MemDlgProc) == TRUE)
                     {
                        switch (cursel)
                        {
                           case 0:
                           case 1:
                           case 2:
                           case 3:
                           case 4:
                           case 5:
                           case 6:
                           case 7:
                              m68kregs.D[cursel] = memaddr;
                              break;
                           case 8:
                           case 9:
                           case 10:
                           case 11:
                           case 12:
                           case 13:
                           case 14:
                           case 15:
                              m68kregs.A[cursel - 8] = memaddr;
                              break;
                           case 16:
                              m68kregs.SR = memaddr;
                              break;
                           case 17:
                              m68kregs.PC = memaddr;
                              M68KUpdateCodeList(hDlg, m68kregs.PC);
                              break;
                           default: break;
                        }

                        ((Scsp*)yabausemem->soundr)->Set68kRegisters(&m68kregs);
                     }

                     M68KUpdateRegList(hDlg, &m68kregs);

                     break;
                  }
                  default: break;
               }

               break;
            }

            default: break;
         }
         break;
      }
      default: break;
   }

   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

void SCUDSPUpdateRegList(HWND hDlg, scudspregs_struct *regs)
{
   char tempstr[128];
   int i;

   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_RESETCONTENT, 0, 0);

   sprintf(tempstr, "PR =          %d\0", regs->dspProgControlPort.part.PR);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "EP =          %d\0", regs->dspProgControlPort.part.EP);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "T0 =          %d\0", regs->dspProgControlPort.part.T0);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "S =           %d\0", regs->dspProgControlPort.part.S);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "Z =           %d\0", regs->dspProgControlPort.part.Z);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "C =           %d\0", regs->dspProgControlPort.part.C);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "V =           %d\0", regs->dspProgControlPort.part.V);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "E =           %d\0", regs->dspProgControlPort.part.E);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "ES =          %d\0", regs->dspProgControlPort.part.ES);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "EX =          %d\0", regs->dspProgControlPort.part.EX);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "LE =          %d\0", regs->dspProgControlPort.part.LE);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

   sprintf(tempstr, "P =          %02X\0", regs->dspProgControlPort.part.P);
   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX1), LB_ADDSTRING, 0, (LPARAM)tempstr);

/*       unsigned long unused1:5;
       unsigned long PR:1; // Pause cancel flag
       unsigned long EP:1; // Temporary stop execution flag
       unsigned long unused2:1;
       unsigned long T0:1; // D0 bus use DMA execute flag
       unsigned long S:1;  // Sine flag
       unsigned long Z:1;  // Zero flag
       unsigned long C:1;  // Carry flag
       unsigned long V:1;  // Overflow flag
       unsigned long E:1;  // Program end interrupt flag
       unsigned long ES:1; // Program step execute control bit
       unsigned long EX:1; // Program execute control bit
       unsigned long LE:1; // Program counter load enable bit
       unsigned long unused3:7;
       unsigned long P:8;  // Program Ram Address
    } part;
    unsigned long all;
  } dspProgControlPort;
*/
}

//////////////////////////////////////////////////////////////////////////////

void SCUDSPUpdateCodeList(HWND hDlg, unsigned long addr)
{
   int i;
   char buf[60];
   unsigned long offset;

   SendMessage(GetDlgItem(hDlg, IDC_LISTBOX2), LB_RESETCONTENT, 0, 0);

   offset = addr;

   for (i = 0; i < 24; i++)
   {
      ((Scu*)yabausemem->scu)->DSPDisasm(offset, buf);
      offset++;

      SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX2)), LB_ADDSTRING, 0,
                  (long)buf);
   }

   SendMessage(HWND(GetDlgItem(hDlg, IDC_LISTBOX2)), LB_SETCURSEL,0,0);
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK SCUDSPDebugDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam)
{
   switch (uMsg)
   {
      case WM_INITDIALOG:
      {
         scudspregs_struct dspregs;

         EnableWindow(HWND(GetDlgItem(hDlg, IDC_STEP)), TRUE);

         ((Scu*)yabausemem->scu)->GetDSPRegisters(&dspregs);
         SCUDSPUpdateRegList(hDlg, &dspregs);
         SCUDSPUpdateCodeList(hDlg, dspregs.dspProgControlPort.part.P);

         return TRUE;
      }
      case WM_COMMAND:
      {
         switch (LOWORD(wParam))
         {
            case IDOK:
            {
               EndDialog(hDlg, TRUE);

               return TRUE;
            }
            case IDC_STEP:
            {
               scudspregs_struct dspregs;

               // execute instruction here

               ((Scu*)yabausemem->scu)->GetDSPRegisters(&dspregs);
               SCUDSPUpdateRegList(hDlg, &dspregs);
               SCUDSPUpdateCodeList(hDlg, dspregs.dspProgControlPort.part.P);

               break;
            }
            case IDC_ADDBP1:
            {
               // add a code breakpoint
               break;
            }
            case IDC_DELBP1:
            {
               // delete a code breakpoint
               break;
            }

            default: break;
         }
         break;
      }
      default: break;
   }

   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
