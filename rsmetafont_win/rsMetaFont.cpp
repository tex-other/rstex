/*

Copyright (C) 2018 by Richard Sandberg.

This is the Windows specific version of rsMetaFont.

*/

#include <Windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <io.h>
#include <iostream>
#include <ctime>
#include <climits>
#include <cstring>
#include "resource.h"
#include "rsMetaFont.h"


int myabs(int x)
{
	// overflow check
	if (x == INT_MIN && -(INT_MIN + 1) == INT_MAX) {
		printf("Overflow myabs.\n");
		exit(1);
	}
	///////////////////////

	return x >= 0 ? x : -x;
}

bool myodd(int c)
{
	return ((c % 2) != 0);
}

///////////////////////////////////////////////////////////////////////////
// System specific addition for paths on windows

void copypath(char *s1, char *s2, int n)
{
	while ((*s1++ = *s2++) != 0) {
		if (--n == 0) {
			fprintf(stderr, "! Environment search path is too big\n");
			*--s1 = 0;
			return;
		}
	}
}

void set_paths()
{
	char *envpath;
	if ((envpath = getenv("MFINPUTS")) != NULL)
		copypath(input_path, envpath, MAX_INPUT_CHARS);
	if ((envpath = getenv("MFBASES")) != NULL)
		copypath(base_path, envpath, MAX_INPUT_CHARS);
	if ((envpath = getenv("MFPOOL")) != NULL)
		copypath(pool_path, envpath, MAX_INPUT_CHARS);
}

void pack_real_name_of_file(char **cpp)
{
	char *p;
	char *real_name;

	real_name = &real_name_of_file[1];
	if ((p = *cpp) != NULL) {
		while ((*p != ';') && (*p != 0)) {
			*real_name++ = *p++;
			if (real_name == &real_name_of_file[file_name_size])
				break;
		}
		if (*p == 0) *cpp = NULL;
		else *cpp = p + 1;
		*real_name++ = '/';
	}
	p = name_of_file.get_c_str();

	while (*p != 0) {
		if (real_name >= &real_name_of_file[file_name_size]) {
			fprintf(stderr, "! Full file name is too long\n");
			break;
		}
		*real_name++ = *p++;
	}
	*real_name = 0;

}

bool test_access(int filepath)
{
	bool ok;
	char *cur_path_place;

	switch (filepath) {
	case no_file_path: cur_path_place = NULL; break;

	case input_file_path:
	case read_file_path:
		cur_path_place = input_path;
		break;
	//case font_file_path: cur_path_place = font_path; break;
	case base_file_path: cur_path_place = base_path; break;
	case pool_file_path: cur_path_place = pool_path; break;
	default:
		fprintf(stderr, "! This should not happen, test_access\n");
		exit(1);
		break;
	}
	if (name_of_file[1] == '\\' || name_of_file[1] == '/' || (isalpha(name_of_file[1]) && name_of_file[2] == ':'))
		cur_path_place = NULL;
	do {
		pack_real_name_of_file(&cur_path_place);
		ok = (_access(real_name_of_file.get_c_str(), 0) == 0);

	} while (!ok && cur_path_place != NULL);

	return ok;
}

///////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// Windows specific display routines
//

// use new common controls
#pragma comment(linker, "/manifestdependency:\"type='win32' \
    name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
    processorArchitecture='*' \
    publicKeyToken='6595b64144ccf1df' language='*'\"")


HINSTANCE ghInst;
HMENU ghMenu;
HWND ghwnd;

// we use global device contexts for painting, perhaps not a great idea, should probably use
// the one returned by BeginPaint when painting in WM_PAINT at least
HDC client_dc;
HDC bitmap_dc;
HBITMAP hBitmap;

// prevent calling WndProc from two threads at the same time
HANDLE hMutex;

// signal to main thread that the window thread has set things up for painting
HANDLE threadStarted;

// grid variables
#define DEFAULT_GRID_COLOR RGB(220,220,220)
#define DEFAULT_FRAME_COLOR RGB(50,50,50)
COLORREF grid_color = DEFAULT_GRID_COLOR;
COLORREF frame_color = DEFAULT_FRAME_COLOR;

bool draw_grid_flag;
bool draw_frame_flag;
int grid_start_x=0;
int grid_start_y=0;
int grid_end_x=200;
int grid_end_y=200;
int grid_spacing=20;


#define ID_STATUS_BAR 1106 // we create status bar manually, could perhaps be a resourse instead

// default color values
#define WHITECOLOR RGB(255,255,255)
#define BLACKCOLOR RGB(0,0,0)

// color values which can be changed
COLORREF black_color=BLACKCOLOR;
COLORREF white_color=WHITECOLOR;

// Settings dialog stores some of the user set values here
static COLORREF dlgBlackColor;
static COLORREF dlgWhiteColor;
static COLORREF dlgGridColor;
static COLORREF dlgFrameColor;

HWND CreateStatusBar(HWND hwndParent, int idStatus, HINSTANCE hinst)
{
	HWND hwndStatus;
	RECT rcClient;
	HLOCAL hloc;
	PINT paParts;
	int cParts=2;
	InitCommonControls();

	hwndStatus = CreateWindowEx(
		0,
		STATUSCLASSNAME,
		NULL,
		SBARS_SIZEGRIP |
		WS_CHILD | WS_VISIBLE,
		0,0,0,0,
		hwndParent,
		(HMENU) (INT_PTR)idStatus,
		hinst,
		NULL);

		GetClientRect(hwndParent, &rcClient);
		hloc = LocalAlloc(LHND, sizeof(int) * cParts);
		if (!hloc) return NULL;
		paParts = (PINT)LocalLock(hloc);
		if (!paParts) {
			LocalFree(hloc);
			return NULL;
		}

		paParts[0] = 120;
		paParts[1] = -1;

		SendMessage(hwndStatus, SB_SETPARTS, (WPARAM)cParts, (LPARAM)
				paParts);

		LocalUnlock(hloc);
		LocalFree(hloc);
		return hwndStatus;
}




void draw_grid(HDC hdc, int x_offset, int y_offset,int win_left, int win_top, int win_right, int win_bot)
{

	HPEN hPenOld = (HPEN)SelectObject(hdc, GetStockObject(DC_PEN));
	SetDCPenColor(hdc, grid_color);

	// clip anything outside the metafont window
	HRGN hrgn = CreateRectRgn(win_left, win_top, win_right, win_bot);
	SelectClipRgn(hdc, hrgn);


	int screen_start_x = grid_start_x+x_offset;
	int screen_end_x = grid_end_x+x_offset;
	int screen_start_y = y_offset - grid_start_y;
	int screen_end_y = y_offset - grid_end_y;


	int start_x = min(screen_start_x, screen_end_x);
	int end_x = max(screen_start_x, screen_end_x);
	int start_y = min(screen_start_y, screen_end_y);
	int end_y = max(screen_start_y, screen_end_y);
	
	int cur_y = start_y;
	while (cur_y <= end_y) {
		MoveToEx(hdc, start_x, cur_y, NULL);
		LineTo(hdc, end_x+1, cur_y);
		cur_y += grid_spacing;
	}

	int cur_x = start_x;
	while (cur_x <= end_x) {
		MoveToEx(hdc, cur_x, start_y, NULL);
		LineTo(hdc, cur_x, end_y+1);
		cur_x += grid_spacing;
	}
	SelectClipRgn(hdc, NULL);
	SelectObject(hdc, hPenOld);
	DeleteObject(hrgn);
}



// This function adapted from Ataul Mukit at codeproject.com, "Replace one color with another in the bitmap of a given device context"
void ReplaceColor(HDC hDC, RECT *rcReplaceArea, COLORREF clrColorReplace, COLORREF clrColorFill)
{
	POINT pt;
	pt.x = rcReplaceArea->left;
	pt.y = rcReplaceArea->top;
	int rcWidth = rcReplaceArea->right-rcReplaceArea->left;
	int rcHeight = rcReplaceArea->bottom-rcReplaceArea->top;
	HDC memDCMonoChrome;
	memDCMonoChrome = CreateCompatibleDC(hDC);
	HBITMAP bmpMonoChrome;
	bmpMonoChrome = CreateCompatibleBitmap(memDCMonoChrome, rcWidth, rcHeight);

	HBITMAP pOldMonoBitmap = (HBITMAP)SelectObject(memDCMonoChrome, bmpMonoChrome);
	
	COLORREF nOldBkColor = SetBkColor(hDC, clrColorReplace);
	// BLT to mono dc so that mask color will have 1 set and the other colors 0
	BitBlt(memDCMonoChrome, 0, 0, rcWidth, rcHeight, hDC, pt.x, pt.y, SRCCOPY);
	
	HDC memDC;
	memDC = CreateCompatibleDC(hDC);

	HBITMAP bmp;
	bmp = CreateCompatibleBitmap(hDC, rcWidth, rcHeight);
	
	HBITMAP pOldBitmap = (HBITMAP)SelectObject(memDC, bmp);

	COLORREF nOldMemDCBkColor = SetBkColor(memDC, clrColorFill);
	COLORREF nOldMemDCTextColor = SetTextColor(memDC, RGB(255, 255, 255));
	// BLT to memory DC so that the monochrome white is set to fill color and the monochrome black is set to white
	BitBlt(memDC, 0, 0, rcWidth, rcHeight, memDCMonoChrome, 0, 0, SRCCOPY);

	// AND pDC with memory dc so that the replace color part is blackened out and all other colors remains same
	BitBlt(hDC, pt.x, pt.y, rcWidth, rcHeight, memDC, 0, 0, SRCAND);

	SetTextColor(memDC, RGB(0, 0, 0));	
	// BLT to memory DC so that the monochrome white is set to fill color and the monochrome black is set to black
	BitBlt(memDC, 0, 0, rcWidth, rcHeight, memDCMonoChrome, 0, 0, SRCCOPY);

	// OR pDC with memory dc so that all colors remains as they where except the blackened out (replace color) 
	// part receives the fill color 
	BitBlt(hDC, pt.x, pt.y, rcWidth, rcHeight, memDC, 0, 0, SRCPAINT);

	// Set the original values back
	SetTextColor(memDC, nOldMemDCTextColor);
	SetBkColor(memDC, nOldMemDCBkColor);
	
	SetBkColor(hDC, nOldBkColor);

	// Set the original bitmaps back
	SelectObject(memDCMonoChrome, pOldMonoBitmap);
	DeleteDC(memDCMonoChrome);

	SelectObject(memDC, pOldBitmap);
	DeleteDC(memDC);
	DeleteObject(bmpMonoChrome);
	DeleteObject(bmp);
}



INT_PTR CALLBACK MyDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{

	switch(msg) {
		case WM_INITDIALOG:
			// initialize dialog controls
			CheckDlgButton(hwndDlg, IDC_CHECK_SHOW_GRID, draw_grid_flag ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_CHECK_SHOW_WINDOW_FRAME, draw_frame_flag ? BST_CHECKED : BST_UNCHECKED);
			
			SetDlgItemInt(hwndDlg, IDC_EDIT_GRID_STARTX, grid_start_x, TRUE);
			SetDlgItemInt(hwndDlg, IDC_EDIT_GRID_STARTY, grid_start_y, TRUE);
			SetDlgItemInt(hwndDlg, IDC_EDIT_GRID_ENDX, grid_end_x, TRUE);
			SetDlgItemInt(hwndDlg, IDC_EDIT_GRID_ENDY, grid_end_y, TRUE);
			SetDlgItemInt(hwndDlg, IDC_EDIT_GRID_SPACING, grid_spacing, FALSE);

			dlgBlackColor = black_color;
			dlgWhiteColor = white_color;
			dlgGridColor = grid_color;
			dlgFrameColor = frame_color;
			return TRUE;

		case WM_NOTIFY:
			// custom drawing of buttons to set color
			switch (((LPNMHDR)lParam) -> code)
			{
				case NM_CUSTOMDRAW:
					{
						UINT_PTR ctl_id = ((LPNMHDR)lParam)->idFrom;
						if (ctl_id == IDC_BTN_BLACK_COLOR ||
							ctl_id == IDC_BTN_WHITE_COLOR ||
							ctl_id == IDC_BTN_GRID_COLOR || 
							ctl_id == IDC_BTN_FRAME_COLOR)
						{
							LPNMCUSTOMDRAW lpnmCD = (LPNMCUSTOMDRAW)lParam;

							switch (lpnmCD -> dwDrawStage)
							{
							case CDDS_PREPAINT:
								{
									COLORREF curColor = (ctl_id == IDC_BTN_BLACK_COLOR ? dlgBlackColor : (ctl_id == IDC_BTN_WHITE_COLOR ? dlgWhiteColor : dlgGridColor));
									switch (ctl_id) {
										case IDC_BTN_BLACK_COLOR:
											curColor = dlgBlackColor;
											break;
										case IDC_BTN_WHITE_COLOR:
											curColor = dlgWhiteColor;
											break;
										case IDC_BTN_GRID_COLOR:
											curColor = dlgGridColor;
											break;
										case IDC_BTN_FRAME_COLOR:
											curColor = dlgFrameColor;
											break;
									}
									HBRUSH hbr = CreateSolidBrush(curColor);
									RECT newRect = lpnmCD->rc;
									newRect.left += 3; newRect.top+=3; newRect.right -= 3; newRect.bottom -= 3;
									FillRect(lpnmCD -> hdc, &newRect, hbr);
									DeleteObject(hbr);
									SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
									return TRUE;
								}
							}
						}
					}
					break;
			}
			break;
		
		case WM_COMMAND:
			switch(LOWORD(wParam)) {

				case IDC_BTN_BLACK_COLOR:
				case IDC_BTN_WHITE_COLOR:
				case IDC_BTN_GRID_COLOR:
				case IDC_BTN_FRAME_COLOR:
					{
						int ctl_id = LOWORD(wParam);
						COLORREF curColor = 0;
						switch (ctl_id) {
							case IDC_BTN_BLACK_COLOR:
								curColor = dlgBlackColor;
								break;
							case IDC_BTN_WHITE_COLOR:
								curColor = dlgWhiteColor;
								break;
							case IDC_BTN_GRID_COLOR:
								curColor = dlgGridColor;
								break;
							case IDC_BTN_FRAME_COLOR:
								curColor = dlgFrameColor;
								break;
						}

						static COLORREF clr_refa[16];
						CHOOSECOLOR cc = {0};
						cc.lStructSize = sizeof cc;
						cc.hwndOwner = hwndDlg;
						cc.lpCustColors = clr_refa;
						cc.rgbResult = curColor;
						cc.Flags = CC_RGBINIT;
						if (ChooseColor(&cc)) {
							if (cc.rgbResult != curColor) {
								switch (ctl_id) {
									case IDC_BTN_BLACK_COLOR:
										dlgBlackColor = cc.rgbResult;
										break;
									case IDC_BTN_WHITE_COLOR:
										dlgWhiteColor = cc.rgbResult;
										break;
									case IDC_BTN_GRID_COLOR:
										dlgGridColor = cc.rgbResult;
										break;
									case IDC_BTN_FRAME_COLOR:
										dlgFrameColor = cc.rgbResult;
										break;
								}
							}
						}
					}
					break;

				case IDOK:
					{
						BOOL translated;
						int temp = GetDlgItemInt(hwndDlg, IDC_EDIT_GRID_STARTX, &translated, TRUE);
						if (translated)
							grid_start_x = temp;
						temp = GetDlgItemInt(hwndDlg, IDC_EDIT_GRID_STARTY, &translated, TRUE);
						if (translated)
							grid_start_y = temp;
						temp = GetDlgItemInt(hwndDlg, IDC_EDIT_GRID_ENDX, &translated, TRUE);
						if (translated)
							grid_end_x = temp;
						temp = GetDlgItemInt(hwndDlg, IDC_EDIT_GRID_ENDY, &translated, TRUE);
						if (translated)
							grid_end_y = temp;
						temp = GetDlgItemInt(hwndDlg, IDC_EDIT_GRID_SPACING, &translated, TRUE);
						if (translated && grid_spacing < 10000 && grid_spacing > 0)
							grid_spacing = temp;

						draw_grid_flag = IsDlgButtonChecked(hwndDlg, IDC_CHECK_SHOW_GRID) == BST_CHECKED;
						draw_frame_flag = IsDlgButtonChecked(hwndDlg, IDC_CHECK_SHOW_WINDOW_FRAME) == BST_CHECKED;
						CheckMenuItem(ghMenu, ID_SETTINGS_SHOWGRID, draw_grid_flag ? MF_CHECKED : MF_UNCHECKED);
						CheckMenuItem(ghMenu, ID_SETTINGS_SHOWWINDOWFRAME, draw_frame_flag ? MF_CHECKED : MF_UNCHECKED);

					}
				// fall through

				case IDCANCEL:
					EndDialog(hwndDlg, wParam);
					return TRUE;
			}
	}
	return FALSE;
}

LRESULT CALLBACK WinProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	LRESULT ret = 0;
	static HWND hwndStatus;
	WaitForSingleObject(hMutex, INFINITE);
	switch(msg) {

		case WM_CREATE:
			ghMenu = GetMenu(hwnd);
			hwndStatus = CreateStatusBar(hwnd, ID_STATUS_BAR, ghInst);
			ret = 0;
			break;

		case WM_SIZE:
			SendMessage(hwndStatus, msg, 0, 0);
			ret = 0;
			break;

		case WM_MOUSEMOVE:
			{
				int xPos = GET_X_LPARAM(lp);
				int yPos = GET_Y_LPARAM(lp);

				// which window are we in
				int inside_window = -1;
				for (int k=0; k<=15; k++) // metafont allows 16 separate screen areas inside one gui window
					if (window_open[k]) {
						if (xPos >= left_col[k] && xPos < right_col[k] &&
						    yPos >= top_row[k] && yPos < bot_row[k]) {
							inside_window = k;
							break;
							
						}
					}
				char buffer[256];
				if (inside_window != -1) {
					sprintf(buffer, "MF (%d,%d)", xPos - m_window[inside_window], n_window[inside_window] - yPos);
					SendMessage(hwndStatus, SB_SETTEXT, 0 | (SBT_NOBORDERS << 8), (LPARAM)buffer);	
				}

				sprintf(buffer, "Screen (%d,%d)", xPos, yPos);
				SendMessage(hwndStatus, SB_SETTEXT, 1 | (SBT_NOBORDERS << 8), (LPARAM)buffer);

			}
			ret = 0;
			break;

		case WM_COMMAND:
			switch(LOWORD(wp)) {
				case ID_SETTINGS_SETTINGS:
					{
						if (DialogBox(ghInst, MAKEINTRESOURCE(IDD_SETTINGS),
								hwnd, MyDialogProc) == IDOK) {

								RECT rc = {0};
								rc.right = screen_width;
								rc.bottom = screen_depth;

								// crude method
								if (dlgBlackColor != black_color) {
									ReplaceColor(bitmap_dc, &rc, black_color, dlgBlackColor);
									black_color = dlgBlackColor;
								}
								if (dlgWhiteColor != white_color) {
									ReplaceColor(bitmap_dc, &rc, white_color, dlgWhiteColor);
									white_color = dlgWhiteColor;
								}

								grid_color = dlgGridColor;
								frame_color = dlgFrameColor;
								update_screen();
						}
					}
					break;

				case ID_SETTINGS_SHOWWINDOWFRAME:
					{
						draw_frame_flag = !draw_frame_flag;
						CheckMenuItem(ghMenu, ID_SETTINGS_SHOWWINDOWFRAME, draw_frame_flag ? MF_CHECKED : MF_UNCHECKED);
						update_screen();
					}
					break;

				case ID_SETTINGS_SHOWGRID:
					{
						draw_grid_flag = !draw_grid_flag;
						CheckMenuItem(ghMenu, ID_SETTINGS_SHOWGRID, draw_grid_flag ? MF_CHECKED : MF_UNCHECKED);
						update_screen();
					}
					break;

				case ID_SETTINGS_PRINT:
					{
						DOCINFO di = {sizeof (DOCINFO), "rsMetafont screen"};
						HDC hdcPrinter;
						PRINTDLG pd = {0};
						pd.lStructSize = sizeof(pd);
						pd.hwndOwner = hwnd;
						pd.Flags = PD_RETURNDC;
						PrintDlg(&pd);
						hdcPrinter = pd.hDC;
						//int cxPage = GetDeviceCaps(hdcPrinter, HORZRES);
						//int cyPage = GetDeviceCaps(hdcPrinter, VERTRES);

						StartDoc(hdcPrinter, &di);
						StartPage(hdcPrinter);
						BitBlt(hdcPrinter, 0,0, screen_width, screen_depth, bitmap_dc, 0, 0, SRCCOPY);
						EndPage(hdcPrinter);
						EndDoc(hdcPrinter);
						DeleteDC(hdcPrinter);
					}
					break;
			}
			ret = 0;
			break;


		case WM_DESTROY:
			PostQuitMessage(0);
			ret = 0;
			break;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
				BitBlt(client_dc, 0,0, screen_width, screen_depth,
						bitmap_dc, 0, 0, SRCCOPY);



				// draw axis
				for (int k=0; k<=15; k++) // metafont allows 16 separate screen areas inside one gui window
					if (window_open[k]) {
						int x_offset = m_window[k];
						int y_offset = n_window[k];
						
						if (draw_grid_flag)
							draw_grid(client_dc, x_offset, y_offset, left_col[k], top_row[k], right_col[k], bot_row[k]);

						// draw outline of this metafont window
						if (draw_frame_flag) {
							SelectObject(client_dc, GetStockObject(DC_PEN));
							SetDCPenColor(client_dc, frame_color);
							HBRUSH oldbrush = (HBRUSH)SelectObject(client_dc, GetStockObject(NULL_BRUSH));
							Rectangle(client_dc, left_col[k], top_row[k], right_col[k], bot_row[k]);
							SelectObject(client_dc, oldbrush);
						}

						
					}
				EndPaint(hwnd, &ps);
			}
			ret = 0;
			break;

		default:			
			ret =  DefWindowProc(hwnd, msg, wp, lp);
			break;
	}
	ReleaseMutex(hMutex);
	return ret;
}


bool SetupWindow()
{
	char const*const classname="myclass";
	ghInst = GetModuleHandle(0);
	WNDCLASSEX wndclass = {sizeof wndclass, CS_HREDRAW|CS_VREDRAW|CS_OWNDC, WinProc,
			0, 0, GetModuleHandle(0), LoadIcon(0, IDI_APPLICATION),
			LoadCursor(0, IDC_ARROW), HBRUSH(COLOR_WINDOW+1),
			MAKEINTRESOURCE(IDR_MENU1), classname, LoadIcon(0, IDI_APPLICATION)};

	if (RegisterClassEx(&wndclass)) {
		HWND hwnd = CreateWindowEx(0, classname, "rsMetafont",
				WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, ghInst, 0);
		if (hwnd) {
			ghwnd = hwnd;

			hMutex = CreateMutex(NULL, FALSE, "MyMutex");
			client_dc = GetDC(hwnd);
			bitmap_dc = CreateCompatibleDC(client_dc);
			hBitmap = CreateCompatibleBitmap(client_dc, screen_width, screen_depth);
			SelectObject(bitmap_dc, hBitmap);

			SelectObject(bitmap_dc, GetStockObject(WHITE_BRUSH));
			PatBlt(bitmap_dc, 0, 0, screen_width, screen_depth, PATCOPY);

			ShowWindow(hwnd, SW_SHOWDEFAULT);
			MSG msg;
			SetEvent(threadStarted); // signal that it's ok to start drawing from the other thread now
			while(GetMessage(&msg, 0, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else return false;
	}
	else return false;

	return true;

}

DWORD WINAPI ThreadProc(LPVOID /*threadparam*/)
{
	bool ret = SetupWindow();
	if (!ret) {
		fprintf(stderr, "Failed to open a window.\n");
	}

	return 0;
}


bool init_win32_window()
{
	HANDLE hThread = CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);
	if (!hThread)
		fprintf(stderr, "Failed to create a thread.\n");
	return (bool)hThread;
}

//////////////////////////////////////////////////////////////////////////////












//////////////////////////////////////////////////////////////////////////////
// Windows specific routine to open editor
//
void call_edit(packed_ASCII_code *filename, int fnlength, int linenumber)
{
	char *temp;
	char *command;
	char c;
	int sdone;
	int ddone;
	int i;
	
	char dvalue[] = "start \"\" \"C:\\Program Files\\Notepad++\\notepad++\" -n%d %s";
	char *mfeditvalue = dvalue;
	sdone = ddone = 0;
	
	if(NULL != (temp = getenv("MFEDIT")))
		mfeditvalue = temp;
	
	if (NULL == (command = (char*)malloc(strlen(mfeditvalue) + fnlength + 25))) {
		fprintf(stderr, "! Not enough memory to issue editor command\n");
		exit(1);
	}
	temp = command;
	while ((c = *mfeditvalue++) != 0) {
		if (c == '%') {
			switch (c = *mfeditvalue++) {
				case 'd':
					if(ddone) {
						fprintf(stderr, "! Line number cannot appear twice in editor command\n");
						exit(1);
					}
					sprintf(temp, "%d", linenumber);
					while (*temp != 0)
						temp++;
					ddone = 1;
					break;
				case 's':
					if (sdone) {
						fprintf(stderr, "! Filename cannot appear twice in editor command\n");
						exit(1);
					}
					i = 0;
					while (i < fnlength)
						*temp++ = filename[i++];
					sdone = 1;
					break;
				case 0:
					*temp++ = '%';
					mfeditvalue--; // Back up to \0 to force termination
					break;
				default:
					*temp++ = '%';
					*temp++ = c;
					break;					
			}
		}
		else
			*temp++ = c;
	}
	*temp = 0;
	
	if (0 != system(command))
		fprintf(stderr, "! Trouble executing command %s\n", command);
	
	
	exit(1);
}

//////////////////////////////////////////////////////////////////////////////




// 4

void initialize()
{
	int i;
	int k;


	// 21
	xchr[040] = ' ';
	xchr[041] = '!';
	xchr[042] = '\"';
	xchr[043] = '#';
	xchr[044] = '$';
	xchr[045] = '%';
	xchr[046] = '&';
	xchr[047] = '\'';
	xchr[050] = '(';
	xchr[051] = ')';
	xchr[052] = '*';
	xchr[053] = '+';
	xchr[054] = ',';
	xchr[055] = '-';
	xchr[056] = '.';
	xchr[057] = '/';
	xchr[060] = '0';
	xchr[061] = '1';
	xchr[062] = '2';
	xchr[063] = '3';
	xchr[064] = '4';
	xchr[065] = '5';
	xchr[066] = '6';
	xchr[067] = '7';
	xchr[070] = '8';
	xchr[071] = '9';
	xchr[072] = ':';
	xchr[073] = ';';
	xchr[074] = '<';
	xchr[075] = '=';
	xchr[076] = '>';
	xchr[077] = '?';
	xchr[0100] = '@';
	xchr[0101] = 'A';
	xchr[0102] = 'B';
	xchr[0103] = 'C';
	xchr[0104] = 'D';
	xchr[0105] = 'E';
	xchr[0106] = 'F';
	xchr[0107] = 'G';
	xchr[0110] = 'H';
	xchr[0111] = 'I';
	xchr[0112] = 'J';
	xchr[0113] = 'K';
	xchr[0114] = 'L';
	xchr[0115] = 'M';
	xchr[0116] = 'N';
	xchr[0117] = 'O';
	xchr[0120] = 'P';
	xchr[0121] = 'Q';
	xchr[0122] = 'R';
	xchr[0123] = 'S';
	xchr[0124] = 'T';
	xchr[0125] = 'U';
	xchr[0126] = 'V';
	xchr[0127] = 'W';
	xchr[0130] = 'X';
	xchr[0131] = 'Y';
	xchr[0132] = 'Z';
	xchr[0133] = '[';
	xchr[0134] = '\\';
	xchr[0135] = ']';
	xchr[0136] = '^';
	xchr[0137] = '_';
	xchr[0140] = '`';
	xchr[0141] = 'a';
	xchr[0142] = 'b';
	xchr[0143] = 'c';
	xchr[0144] = 'd';
	xchr[0145] = 'e';
	xchr[0146] = 'f';
	xchr[0147] = 'g';
	xchr[0150] = 'h';
	xchr[0151] = 'i';
	xchr[0152] = 'j';
	xchr[0153] = 'k';
	xchr[0154] = 'l';
	xchr[0155] = 'm';
	xchr[0156] = 'n';
	xchr[0157] = 'o';
	xchr[0160] = 'p';
	xchr[0161] = 'q';
	xchr[0162] = 'r';
	xchr[0163] = 's';
	xchr[0164] = 't';
	xchr[0165] = 'u';
	xchr[0166] = 'v';
	xchr[0167] = 'w';
	xchr[0170] = 'x';
	xchr[0171] = 'y';
	xchr[0172] = 'z';
	xchr[0173] = '{';
	xchr[0174] = '|';
	xchr[0175] = '}';
	xchr[0176] = '~';


	// 22
	for (i = 0; i <= 037; i++) xchr[i] = i;
	for (i = 0177; i <= 0377; i++) xchr[i] = i;


	// 23
	for (i = first_text_char; i <= last_text_char; i++) xord[i] = 0177;
	for (i = 0200; i <= 0377; i++) xord[xchr[i]] = i;
	for (i = 0; i <= 0176; i++) xord[xchr[i]] = i;

	// 69
	interaction = error_stop_mode;
	deletions_allowed = true; error_count = 0; // history is initialized elsewhere

	// 75
	help_ptr = 0; use_err_help = false; err_help = 0;

	// 92
	interrupt = 0; OK_to_interrupt = true;

	// 98
	arith_error = false;

	// 131
	two_to_the[0] = 1;
	for (k = 1; k <= 30; k++) two_to_the[k] = 2 * two_to_the[k - 1];
	spec_log[1] = 93032640; spec_log[2] = 38612034; spec_log[3] = 17922280; spec_log[4] = 8662214;
	spec_log[5] = 4261238; spec_log[6] = 2113709; spec_log[7] = 1052693; spec_log[8] = 525315;
	spec_log[9] = 262400; spec_log[10] = 131136; spec_log[11] = 65552; spec_log[12] = 32772;
	spec_log[13] = 16385;
	for (k = 14; k <= 27; k++) spec_log[k] = two_to_the[27 - k];
	spec_log[28] = 1;

	// 138
	spec_atan[1] = 27855475; spec_atan[2] = 14718068; spec_atan[3] = 7471121; spec_atan[4] = 3750058;
	spec_atan[5] = 1876857; spec_atan[6] = 938658; spec_atan[7] = 469357; spec_atan[8] = 234682;
	spec_atan[9] = 117342; spec_atan[10] = 58671; spec_atan[11] = 29335; spec_atan[12] = 14668;
	spec_atan[13] = 7334; spec_atan[14] = 3667; spec_atan[15] = 1833; spec_atan[16] = 917;
	spec_atan[17] = 458; spec_atan[18] = 229; spec_atan[19] = 115; spec_atan[20] = 57; spec_atan[21] = 29;
	spec_atan[22] = 14; spec_atan[23] = 7; spec_atan[24] = 4; spec_atan[25] = 2; spec_atan[26] = 1;

	// 179
#ifndef NO_DEBUG
	was_mem_end = mem_min; // indicate that everything was previously free
	was_lo_max = mem_min; was_hi_min = mem_max; panicking = false;
#endif

	// 191
	for (k = 1; k <= max_given_internal; k++) internal[k] = 0;
	int_ptr = max_given_internal;

	// 199
	for (k = /*0*/48; k <= /*9*/57; k++) char_class[k] = digit_class;
	char_class[/*.*/46] = period_class;
	char_class[/* */32] = space_class;
	char_class[/*%*/37] = percent_class;
	char_class[/*"*/34] = string_class;
	char_class[/*,*/44] = 5;
	char_class[/*;*/59] = 6;
	char_class[/*(*/40] = 7;
	char_class[/*)*/41] = right_paren_class;
	for (k = /*A*/65; k <= /*Z*/90; k++) char_class[k] = letter_class;
	for (k = /*a*/97; k <= /*z*/122; k++) char_class[k] = letter_class;
	char_class[/*_*/95] = letter_class;
	char_class[/*<*/60] = 10;
	char_class[/*=*/61] = 10;
	char_class[/*>*/62] = 10;
	char_class[/*:*/58] = 10;
	char_class[/*|*/124] = 10;
	char_class[/*`*/96] = 11;
	char_class[/*'*/39] = 11;
	char_class[/*+*/43] = 12;
	char_class[/*-*/45] = 12;
	char_class[/*/*/47] = 13;
	char_class[/***/42] = 13;
	char_class[/*\*/92] = 13;
	char_class[/*!*/33] = 14;
	char_class[/*?*/63] = 14;
	char_class[/*#*/35] = 15;
	char_class[/*&*/38] = 15;
	char_class[/*@*/64] = 15;
	char_class[/*$*/36] = 15;
	char_class[/*^*/94] = 16;
	char_class[/*~*/126] = 16;
	char_class[/*[*/91] = left_bracket_class;
	char_class[/*]*/93] = right_bracket_class;
	char_class[/*{*/123] = 19;
	char_class[/*}*/125] = 19;
	for (k = 0; k <= /* */32 - 1; k++) char_class[k] = invalid_class;
	for (k = 127; k <= 255; k++) char_class[k] = invalid_class;
	char_class[tab] = space_class;
	char_class[form_feed] = space_class;

	// 202
	next(1) = 0; text(1) = 0; eq_type(1) = tag_token; equiv(1) = null;
	for (k = 2; k <= hash_end; k++) {
		hash[k] = hash[1]; eqtb[k] = eqtb[1];
	}

	// 231
	big_node_size[transform_type] = transform_node_size; big_node_size[pair_type] = pair_node_size;

	// 251
	save_ptr = null;

	// 396
	octant_dir[first_octant] = /*ENE*/256; octant_dir[second_octant] = /*NNE*/257; octant_dir[third_octant] = /*NNW*/258;
	octant_dir[fourth_octant] = /*WNW*/259; octant_dir[fifth_octant] = /*WSW*/260; octant_dir[sixth_octant] = /*SSW*/261;
	octant_dir[seventh_octant] = /*SSE*/262; octant_dir[eighth_octant] = /*ESE*/263;

	// 428
	max_rounding_ptr = 0;

	// 449
	octant_code[1] = first_octant; octant_code[2] = second_octant; octant_code[3] = third_octant;
	octant_code[4] = fourth_octant; octant_code[5] = fifth_octant; octant_code[6] = sixth_octant;
	octant_code[7] = seventh_octant; octant_code[8] = eighth_octant;
	for (k = 1; k <= 8; k++) octant_number[octant_code[k]] = k;

	// 456
	rev_turns = false;

	// 462
	x_corr[first_octant] = 0; y_corr[first_octant] = 0; xy_corr[first_octant] = 0;
	x_corr[second_octant] = 0; y_corr[second_octant] = 0; xy_corr[second_octant] = 1;
	x_corr[third_octant] = -1; y_corr[third_octant] = 1; xy_corr[third_octant] = 0;
	x_corr[fourth_octant] = 1; y_corr[fourth_octant] = 0; xy_corr[fourth_octant] = 1;
	x_corr[fifth_octant] = 0; y_corr[fifth_octant] = 1; xy_corr[fifth_octant] = 1;
	x_corr[sixth_octant] = 0; y_corr[sixth_octant] = 1; xy_corr[sixth_octant] = 0;
	x_corr[seventh_octant] = 1; y_corr[seventh_octant] = 0; xy_corr[seventh_octant] = 1;
	x_corr[eighth_octant] = -1; y_corr[eighth_octant] = 1; xy_corr[eighth_octant] = 0;
	for(k = 1; k <= 8; k++) z_corr[k] = xy_corr[k] - x_corr[k];

	// 570
	screen_started = false; screen_OK = false;

	// 573
	for (k = 0; k <= 15; k++) {
		window_open[k] = false; window_time[k] = 0;
	}

	// 593
	fix_needed = false; watch_coefs = true;

	// 739
	cond_ptr = null; if_limit = normal; cur_if = 0; if_line = 0;

	// 753
	loop_ptr = null;

	// 776
	//strcpy(MF_base_default.get_c_str(), "plain.base");

	// 797
	cur_exp = 0;

	// 822
	var_flag = 0;

	// 1078
	start_sym = 0;

	// 1085
	long_help_seen = false;

	// 1097
	for (k = 0; k <= 255; k++) {
		tfm_width[k] = 0; tfm_height[k] = 0; tfm_depth[k] = 0; tfm_ital_corr[k] = 0;
		char_exists[k] = false; char_tag[k] = no_tag; char_remainder[k] = 0; skip_table[k] = undefined_label;
	}
	for (k = 1; k <= header_size; k++) header_byte[k] = -1;
	bc = 255; ec = 0; nl = 0; nk = 0; ne = 0; np = 0;
	internal[boundary_char] = -unity; bch_label = undefined_label;
	label_loc[0] = -1; label_ptr = 0;

	// 1150
	gf_prev_ptr = 0; total_chars = 0;

	// 1153
	half_buf = gf_buf_size / 2; gf_limit = gf_buf_size; gf_ptr = 0; gf_offset = 0;

	// 1184
	base_ident = 0;
}



// 26
bool a_open_in(FILE **f, int path_specifier)
{
	if (test_access(path_specifier))
		*f = fopen(real_name_of_file.get_c_str(), "r");
	else
		return false;

	return *f;
}

bool a_open_out(FILE **f)
{
	*f = fopen(name_of_file.get_c_str(), "w");
	return *f;
}

/*
bool b_open_in(FILE **f)
{
	if (test_access(font_file_path))
		*f = fopen(real_name_of_file.get_c_str(), "rb");
	else
		return false;
	return *f;
}
*/
bool b_open_out(FILE **f)
{
	*f = fopen(name_of_file.get_c_str(), "wb");
	return *f;
}


bool w_open_in(FILE **f)
{
	if (test_access(base_file_path))
		*f = fopen(real_name_of_file.get_c_str(), "rb");
	else
		return false;
	return *f;
}

bool w_open_out(FILE **f)
{
	*f = fopen(name_of_file.get_c_str(), "wb");
	return *f;
}

void a_close(FILE *f)
{
	fclose(f);
}

void b_close(FILE *f)
{
	fclose(f);
}

void w_close(FILE *f)
{
	fclose(f);
}



// 33
void update_terminal()
{}
void clear_terminal()
{}
void wake_up_terminal()
{}



// routine called when METAFONT says goto end_of_MF
void do_end_of_MF()
{
	close_files_and_terminate();
	do_final_end();
}



// routine called when METAFONT says goto final_end
void do_final_end()
{
	wterm_cr;

	if (history <= warning_issued)
		exit(0);
	else
		exit(1);
}


// 31
bool input_ln(FILE *fp, bool /*bypass_eoln*/)
{
	int last_nonblank;

	last = first;

	last_nonblank = first;
	int c;
	c = fgetc(fp);
	if (c == EOF)
		return false;
	else
		ungetc(c, fp);
	while ((c = fgetc(fp)) != '\n' && c != EOF) {
		if (last >= max_buf_stack) {
			max_buf_stack = last + 1;
			if (max_buf_stack == buf_size)
				#pragma region <Report overflow of the input buffer, and abort 35>
			{
				if (base_ident == 0) {
					puts("Buffer size exceeded!");
					do_final_end();
				}
				else {
					cur_input.loc_field = first;
					cur_input.limit_field = last - 1;
					overflow(/*buffer size*/264, buf_size);
				}
			}
				#pragma endregion
		}
		buffer[last] = xord[c]; last++;
		if (buffer[last - 1] != /* */32)
			last_nonblank = last;
	}
	last = last_nonblank;
	return true;
}


// 37
bool init_terminal(int argc, char **argv)
{
	//////////////////////////////////////////////////////////
	// system dependent part, similar to tex-sparc/initex.ch
	if (argc > 1) {
		last = first;
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') // skip
				continue;
			size_t j = 0;
			size_t k = strlen(argv[i]) - 1;
			while (k > 0 && argv[i][k] == ' ')
				k--;
			while (j <= k) {
				buffer[last] = xord[argv[i][j]];
				j++; last++;
			}
			if (k > 0) {
				buffer[last] = xord[' '];
				last++;
			}
		}
		if (last > first) {
			loc = first;
			while (loc < last && buffer[loc] == ' ')
				loc++;
			if (loc < last)
				return true;
		}
	}
	//////////////////////////////////////////////////////////


	while (1) {
		fputs("**", term_out);
		if (!input_ln(term_in, true)) {
			fputc('\n', term_out);
			fputs("! End of file on terminal... why?", term_out);
			return false;
		}
		loc = first;
		while (loc < last && buffer[loc] == ' ')
			loc++;
		if (loc < last)
			return true;
		fputs("Please type the name of your input file.\n", term_out);
	}
}



// 43
void flush_string(str_number s)
{
	if (s < str_ptr - 1) str_ref[s] = 0;
	else do {
		str_ptr--;
	} while (!(str_ref[str_ptr - 1] != 0));
	pool_ptr = str_start[str_ptr];
}

// 44
str_number make_string()
{
	if (str_ptr == max_str_ptr) {
		if (str_ptr == max_strings) overflow(/*number of strings*/265, max_strings - init_str_ptr);
		max_str_ptr++;
	}
	str_ref[str_ptr] = 1; str_ptr++; str_start[str_ptr] = pool_ptr;
	return str_ptr - 1;
}

// 45

bool str_eq_buf(str_number s, int k)
{
	pool_pointer j; // running index
	bool result; // result of comparison
	j = str_start[s];
	while (j < str_start[s + 1]) {
		if (str_pool[j] != buffer[k]) {
			result = false; goto not_found;
		}
		j++; k++;
	}
	result = true;
not_found:
	return result;
}

// 46
int str_vs_str(str_number s, str_number t)
{
	pool_pointer j, k; // running indices
	int ls, lt; // lengths
	int l; // length remaining to test

	ls = length(s); lt = length(t);
	if (ls <= lt) l = ls; else l = lt;
	j = str_start[s]; k = str_start[t];
	while (l > 0) {
		if (str_pool[j] != str_pool[k]) {
			return str_pool[j] - str_pool[k];
		}
		j++; k++; l--;
	}
	return ls - lt;
}



// 47
//init
#ifndef NO_INIT
bool get_strings_started()
{
	int k, l; // 0..255, small indices or counters
	text_char m, n; // characters input from pool_file
	str_number g; // garbage
	int a; // accumulator for check sum
	bool c; // check sum has been checked


	pool_ptr = 0;
	str_ptr = 0;
	max_pool_ptr = 0;
	max_str_ptr = 0;
	str_start[0] = 0;

	#pragma region <Make the first 256 strings 48>
	for (k = 0; k <= 255; k++) {
		if (k < /* */32 || k > /*~*/126) //<Character k cannot be printed 49>
		{
			append_char(/*^*/94);
			append_char(/*^*/94);
			if (k < 0100)
				append_char(k + 0100);
			else if (k < 0200)
				append_char(k - 0100);
			else {
				app_lc_hex(k / 16); app_lc_hex(k % 16);
			}
		}
		else
			append_char(k);
		g = make_string();
		str_ref[g] = max_str_ref;
	}
	#pragma endregion

	#pragma region <Read the other strings from the mf.pool file and return true, or give an error message and return false 51>

	strcpy(name_of_file.get_c_str(), pool_name);
	if (a_open_in(&pool_file, pool_file_path)) {
		c = false;
		do {
			#pragma region <Read one string, but return false if the string memory space is getting too tight for comfort 52>
			{
				if (feof(pool_file))
					bad_pool("! mf.pool has no checksum.", true);
				m = fgetc(pool_file);
				n = fgetc(pool_file);
				if (m == '*')
					#pragma region <check the pool checksum 53>
				{
					a = 0;
					k = 1;
					while (1) {
						if (xord[n] < '0' || xord[n] > '9')
							bad_pool("! mf.pool check sum doesn\'t have nine digits.", true);
						a = 10 * a + xord[n] - '0';
						if (k == 9) goto done;
						k++;
						n = fgetc(pool_file);
					}
				done:
					if (a != 271969154)
						bad_pool("! mf.pool doesn\'t match; TANGLE me again.", true);
					c = true;
				}
					#pragma endregion
				else {
					if (xord[m] < /*0*/48 || xord[m]> /*9*/57 || xord[n] < /*0*/48 || xord[n] > /*9*/57)
						bad_pool("! mf.pool line does not begin with two digits.", true);
					l = xord[m] * 10 + xord[n] - /*0*/48 * 11;
					if (pool_ptr + l + string_vacancies > pool_size)
						bad_pool("! You have to increase POOLSIZE.", true);
					for (k = 1; k <= l; k++) {
						int ch = fgetc(pool_file);
						if (ch == '\n')
							m = ' ';
						else
							m = ch;
						append_char(xord[m]);
					}
					fgetc(pool_file); // read and discard new line
					make_string();

				}
			}
			#pragma endregion
		} while (!c);
		a_close(pool_file);

		return true;
	}
	else
		bad_pool("! I can\'t read mf.pool.", false);
	#pragma endregion

}
#endif
//tini

// 57
void print_ln()
{
	switch (selector) {
	case term_and_log:
		wterm_cr;
		wlog_cr;
		term_offset = 0;
		file_offset = 0;
		break;

	case log_only:
		wlog_cr;
		file_offset = 0;
		break;

	case term_only:
		wterm_cr;
		term_offset = 0;
		break;

	case no_print:
	case pseudo:
	case new_string:
		//do_nothing:
		break;

	}
}

// 58
void print_char(ASCII_code s)
{
	switch (selector) {
	case term_and_log:
		wterm_c(xchr[s]);
		wlog_c(xchr[s]);
		term_offset++;
		file_offset++;
		if (term_offset == max_print_line) {
			wterm_cr;
			term_offset = 0;
		}
		if (file_offset == max_print_line) {
			wlog_cr;
			file_offset = 0;
		}
		break;

	case log_only:
		wlog_c(xchr[s]);
		file_offset++;
		if (file_offset == max_print_line)
			print_ln();
		break;

	case term_only:
		wterm_c(xchr[s]);
		term_offset++;
		if (term_offset == max_print_line)
			print_ln();
		break;

	case no_print:
		// do_nothing
		break;

	case pseudo:
		if (tally < trick_count)
			trick_buf[tally % error_line] = s;
		break;

	case new_string:
		if (pool_ptr < pool_size)
			append_char(s);
		break;

	}
	tally++;
}


// 59
void print(int s)
{
	pool_pointer j; // current character position

	if (s < 0 || s >= str_ptr) s = /*???*/266; // this can't happen
	if (s < 256 && selector > pseudo) print_char(s);
	else {
		j = str_start[s];
		while (j < str_start[s + 1]) {
			print_char(str_pool[j]); j++;
		}
	}
}

void slow_print(int s)
{
	pool_pointer j; // current character position

	if (s < 0 || s >= str_ptr) s = /*???*/266; // this can't happen
	if (s < 256 && selector > pseudo) print_char(s);
	else {
		j = str_start[s];
		while (j < str_start[s + 1]) {
			print(str_pool[j]); j++;
		}
	}
}


// 62
void print_nl(str_number s)
{
	if ((term_offset > 0 && myodd(selector)) || (file_offset > 0 && selector >= log_only))
		print_ln();
	print(s);
}

// 63
void print_the_digs(eight_bits k) // prints dig[k-1]..dig[0]
{
	while (k > 0) {
		k--;
		print_char(/*0*/48 + dig[k]);
	}
}


// 64
void print_int(int n)
{
	int k; // 0..23, index to current digit; we assume that n < 10^23
	int m; // used to negate n in possibly dangerous cases

	k = 0;
	if (n < 0) {
		print_char(/*-*/45);
		if (n > -100000000) n = -n;
		else {
			m = -1 - n; n = m / 10; m = (m % 10) + 1; k = 1;
			if (m < 10) dig[0] = m;
			else {
				dig[0] = 0; n++;
			}

		}
	}
	do {
		dig[k] = n % 10; n = n / 10; k++;
	} while (!(n == 0));
	print_the_digs(k);
}

// 65
void print_dd(int n) // prints two least significant digits
{
	n = myabs(n) % 100; print_char(/*0*/48 + (n / 10)); print_char(/*0*/48 + (n % 10));
}

// 66
void term_input() // gets a line from the terminal
{
	int k; // 0..buf_size, index into buffer

	update_terminal(); // now the user sees the prompt for sure
	if (!input_ln(term_in, true)) fatal_error(/*End of file on the terminal!*/267);
	term_offset = 0; // the user's line ended with <return>
	selector--; // prepare to echo the input
	if (last != first)
		for (k = first; k <= last - 1; k++) print(buffer[k]);
	print_ln(); buffer[last] = /*%*/37; selector++; // restore previous status
}



// 77
void error() //{completes the job of error reporting}
{
	ASCII_code c; //{what the user types}
	int s1, s2, s3; //{used to save global variables when deleting tokens}
	pool_pointer j;// {character position being printed}
	if (history < error_message_issued) history = error_message_issued;
	print_char(/*.*/46); show_context();
	if (interaction == error_stop_mode)
		#pragma region <Get users advice and |return|>
		while (true) {
			clear_for_error_prompt(); prompt_input(/*? */268);
			if (last == first) return;
			c = buffer[first];
			if (c >= /*a*/97) c = c + /*A*/65 - /*a*/97; //{convert to uppercase}
			#pragma region <Interpret code |c| and |return| if done>
			switch (c) {
				case /*0*/48: case /*1*/49: case /*2*/50: case /*3*/51: case /*4*/52: case /*5*/53: case /*6*/54: case /*7*/55: case /*8*/56: case /*9*/57:
					if (deletions_allowed)
					#pragma region <Delete |c-"0"| tokens and |goto continue|>
					{
						s1 = cur_cmd; s2 = cur_mod; s3 = cur_sym; OK_to_interrupt = false;
						if (last > first + 1 && buffer[first + 1] >= /*0*/48 && buffer[first + 1] <= /*9*/57)
							c = c * 10 + buffer[first + 1] - /*0*/48 * 11;
						else c = c - /*0*/48;
						while (c > 0)
						{
							get_next(); //{one-level recursive call of |error| is possible}
							#pragma region <Decrease the string reference count, if the current token is a string>
							if (cur_cmd == string_token) delete_str_ref(cur_mod);
							#pragma endregion
							c--;
						}
						cur_cmd = s1; cur_mod = s2; cur_sym = s3; OK_to_interrupt = true;
						help2(/*I have just deleted some text, as you asked.*/269,
							/*You can now delete more, or insert, or whatever.*/270);
						show_context(); continue;
					}
					#pragma endregion
					break;
#ifndef NO_DEBUG 
				case /*D*/68:
					debug_help(); continue;
					break;
#endif
				case /*E*/69:
					if (file_ptr > 0)
					{
						ed_name_start = str_start[edit_file.name_field];
						ed_name_length = str_start[edit_file.name_field+1] - str_start[edit_file.name_field];
						edit_line = line;
						jump_out();
					}
					break;
				case /*H*/72:
					#pragma region <Print the help information and |goto continue|>
					if (use_err_help)
					{
						#pragma region <Print the string |err_help|, possibly on several lines>
						j = str_start[err_help];
						while (j < str_start[err_help + 1])
						{
							if (str_pool[j] != si(/*%*/37)) print(so(str_pool[j]));
							else if (j + 1 == str_start[err_help + 1]) print_ln();
							else if (str_pool[j + 1] != si(/*%*/37)) print_ln();
							else {
								j++; print_char(/*%*/37);
							}
							j++;
						}
						#pragma endregion
						use_err_help = false;
					}
					else {
						if (help_ptr == 0)
							help2(/*Sorry, I don't know how to help in this situation.*/271,
								/*Maybe you should try asking a human?*/272);
						do {
							help_ptr--; print(help_line[help_ptr]); print_ln();
						} while (!(help_ptr == 0));
					}
					help4(/*Sorry, I already gave what help I could...*/273,
						/*Maybe you should try asking a human?*/272,
						/*An error might have occurred before I noticed any problems.*/274,
						/*``If all else fails, read the instructions.''*/275);
					continue;
					#pragma endregion
					break;
				case /*I*/73:
					#pragma region <Introduce new material from the terminal and |return|>
					begin_file_reading(); //{enter a new syntactic level for terminal input}
					if (last > first + 1)
					{
						loc = first + 1; buffer[first] = /* */32;
					}
					else {
						prompt_input(/*insert>*/276); loc = first;
					}
					first = last + 1; cur_input.limit_field = last; return;
					#pragma endregion
					break;
				case /*Q*/81: case /*R*/82: case /*S*/83:
					#pragma region <Change the interaction level and |return|>
					error_count = 0; interaction = batch_mode + c - /*Q*/81;
					print(/*OK, entering */277);
					switch (c) {
					case /*Q*/81:
						print(/*batchmode*/278); selector--;
						break;
					case /*R*/82:
						print(/*nonstopmode*/279);
						break;
					case /*S*/83:
						print(/*scrollmode*/280);
						break;
					} //{there are no other cases}
					print(/*...*/281); print_ln(); update_terminal(); return;
					#pragma endregion
					break;
				case /*X*/88:
					interaction = scroll_mode; jump_out();
					break;
				default: /*do_nothing*/ break;
			}
			#pragma region <Print the menu of available options>
			print(/*Type <return> to proceed, S to scroll future error messages,*/282);
			print_nl(/*R to run without stopping, Q to run quietly,*/283);
			print_nl(/*I to insert something, */284);
			if (file_ptr > 0) print(/*E to edit your file,*/285);
			if (deletions_allowed)
				print_nl(/*1 or ... or 9 to ignore the next 1 to 9 tokens of input,*/286);
			print_nl(/*H for help, X to quit.*/287);
			#pragma endregion


			#pragma endregion
		}
	#pragma endregion
	error_count++;
	if (error_count == 100)
	{
		print_nl(/*(That makes 100 errors; please try again.)*/288);
		history = fatal_error_stop; jump_out();
	}
	#pragma region <Put help message on the transcript file>
	if (interaction > batch_mode) selector--; //{avoid terminal output}
	if (use_err_help)
	{
		print_nl(/**/289);
		#pragma region <Print the string |err_help|, possibly on several lines>
		j = str_start[err_help];
		while (j < str_start[err_help + 1])
		{
			if (str_pool[j] != si(/*%*/37)) print(so(str_pool[j]));
			else if (j + 1 == str_start[err_help + 1]) print_ln();
			else if (str_pool[j + 1] != si(/*%*/37)) print_ln();
			else {
				j++; print_char(/*%*/37);
			}
			j++;
		}
		#pragma endregion
	}
	else while (help_ptr > 0)
	{
		help_ptr--; print_nl(help_line[help_ptr]);
	}
	print_ln();
	if (interaction > batch_mode) selector++; //{re-enable terminal output}
	print_ln();
	#pragma endregion
}


// 81
void jump_out()
{
	do_end_of_MF();
}



// 87
void normalize_selector()
{
	if (log_opened) selector = term_and_log;
	else selector = term_only;
	if (job_name == 0) open_log_file();
	if (interaction == batch_mode) selector--;
}

// 88

void succumb()
{
	if (interaction == error_stop_mode)
		interaction = scroll_mode;
	if (log_opened)
		error();
	//debug
#ifndef NO_DEBUG
	if (interaction > batch_mode)
		debug_help();
#endif
	//gubed
	history = fatal_error_stop;
	jump_out();
}

void fatal_error(str_number s) // prints s, and that's it
{
	normalize_selector();
	print_err(/*Emergency stop*/290); help1(s); succumb();
}


// 89
void overflow(str_number s, int n)
{
	normalize_selector(); print_err(/*METAFONT capacity exceeded, sorry [*/291); print(s);
	print_char(/*=*/61); print_int(n); print_char(/*]*/93);
	help2(/*If you really absolutely need more capacity,*/292,
		/*you can ask a wizard to enlarge me.*/293); succumb();
}

void fix_date_and_time()
{
	time_t unixtime;
	time(&unixtime);
	struct tm *tm_struct = localtime(&unixtime);
	
	internal[_time] = tm_struct->tm_hour*60+tm_struct->tm_min;
	internal[day] = tm_struct->tm_mday;
	internal[month] = tm_struct->tm_mon + 1;
	internal[year] = 1900 + tm_struct->tm_year;

	internal[_time] = internal[_time] * unity; // minutes since midnight
	internal[day] = internal[day] * unity; // fourth day of the month
	internal[month] = internal[month] * unity; // seventh month of the year
	internal[year] = internal[year] * unity; // Anno Domini
}

// 90
void confusion(str_number s) // consistency check violated; s tells where
{
	normalize_selector();
	if (history < error_message_issued) {
		print_err(/*This can't happen (*/294); print(s); print_char(/*)*/41);
		help1(/*I'm broken. Please show this to someone who can fix can fix*/295);
	}
	else {
		print_err(/*I can't go on meeting you like this*/296);
		help2(/*One of your faux pas seems to have wounded me deeply...*/297,
			/*in fact, I'm barely conscious. Please fix it and try again.*/298);
	}
	succumb();
}

// 93
void pause_for_instructions()
{
	if (OK_to_interrupt) {
		interaction = error_stop_mode;
		if (selector == log_only || selector == no_print)
			selector++;
		print_err(/*Interruption*/299);
		help3(/*You rang?*/300,
			/*Try to insert some instructions for me (e.g.,`I show x'),*/301,
				/*unless you just want to quit by typing `X'.*/302);
		deletions_allowed = false;
		error();
		deletions_allowed = true;
		interrupt = 0;
	}
}

// 94
void missing_err(str_number s)
{
	print_err(/*Missing `*/303); print(s); print(/*' has been inserted*/304);
}


// 99
void clear_arith()
{
	print_err(/*Arithmetic overflow*/305);
	help4(/*Uh, oh. A little while ago one of the quantities that I was*/306,
		/*computing got too large, so I'm afraid your answers will be*/307,
		/*somewhat askew. You'll probably have to adopt different*/308,
		/*tactics next time. But I shall try to carry on anyway.*/309);
	error();
	arith_error = false;
}

// 100
int slow_add(int x, int y)
{
	if (x >= 0)
		if (y <= el_gordo - x) return x + y;
		else {
			arith_error = true; return el_gordo;
		}
	else if (-y <= el_gordo + x) return x + y;
	else {
		arith_error = true; return -el_gordo;
	}
}


// 102
scaled round_decimals(small_number k)
{
	int a; // the accumulator
	a = 0;
	while (k > 0) {
		k--; a = (a + dig[k]*two) / 10;
	}
	return half(a + 1);
}

// 103
void print_scaled(scaled s) //{prints scaled real, rounded to fivedigits}
{
	scaled delta; //{amount of allowable inaccuracy}
	if (s < 0)
	{ 
		print_char(/*-*/45); negate(s); //{print the sign, if negative}
	}
	print_int(s / unity); //{print the integer part}
	s = 10 * (s % unity) + 5;
	if (s != 5)
	{ 
		delta = 10; print_char(/*.*/46);
		do {
			if (delta > unity)
				s = s + 0100000 - (delta / 2); //{round the final digit}
			print_char(/*0*/48 + (s / unity)); s = 10 * (s % unity); delta = delta * 10;
		} while (!(s <= delta));
	}
}


// 104
void print_two(scaled x, scaled y) // prints '(x,y)'
{
	print_char(/*(*/40); print_scaled(x); print_char(/*,*/44); print_scaled(y); print_char(/*)*/41);
}

// 107
fraction make_fraction(int p, int q)
{
	int f; //{the fraction bits, with a leading 1 bit}
	int n; //{the integer part of $\vert p/q\vert$}
	bool negative; //{should the result be negated?}
	int be_careful; //{disables certain compiler optimizations}

	if (p >= 0) negative = false;
	else { 
		negate(p); negative = true;
	}
	if (q <= 0)
	{ 
	#ifndef NO_DEBUG
		if (q == 0) confusion(/*/*/47);
	#endif
		negate(q); negative = !negative;
	}
	n = p / q; p = p % q;
	if (n >= 8)
	{ 
		arith_error = true;
		if (negative) return -el_gordo; else return el_gordo;
	}
	else {
		n = (n - 1) * fraction_one;
		#pragma region <Compute $f=lfloor 2^{28}(1+p/q)+{1over2}rfloor$>
		f = 1;
		do { 
			be_careful = p - q; p = be_careful + p;
			if (p >= 0) f = f + f + 1;
		else {
			_double(f); p = p + q;
		}
		} while (!(f >= fraction_one));
		be_careful = p - q;
		if (be_careful + p >= 0) f++;
		#pragma endregion
		if (negative) return -(f + n); else return f + n;
	}
}


// 109
int take_fraction(int q, fraction f)
{
	int p; //{the fraction so far}
	bool negative; //{should the result be negated?}
	int n; //{additional multiple of $q$}
	int be_careful; //{disables certain compiler optimizations}
	
	#pragma region <Reduce to the case that |f>=0| and |q>0|>
	if (f >= 0) negative = false;
	else {
		negate(f); negative = true;
	}
	if (q < 0)
	{
		negate(q); negative = !negative;
	}
	#pragma endregion
	
	if (f < fraction_one) n = 0;
	else {
		n = f / fraction_one; f = f % fraction_one;
		if (q <= el_gordo / n) n = n * q;
		else {
			arith_error = true; n = el_gordo;
		}
	}
	f = f + fraction_one;
	#pragma region <Compute... 111>
	p = fraction_half; //{that's $2^{27}$; the invariants hold now with $k=28$}
	if (q < fraction_four)
		do {
			if (myodd(f)) p = half(p + q); else p = half(p);
			f = half(f);
		} while (!(f == 1));
	else
		do {
			if (myodd(f)) p = p + half(q - p); else p = half(p);
			f = half(f);
		} while (!(f == 1));
	#pragma endregion
	be_careful = n - el_gordo;
	if (be_careful + p > 0)
	{
		arith_error = true; n = el_gordo - p;
	}
	
	if (negative) return -(n + p);
	else return n + p;
}


// 112
int take_scaled(int q, scaled f)
{
	int p; //{the fraction so far}
	bool negative; //{should the result be negated?}
	int n; //{additional multiple of $q$}
	int be_careful; //{disables certain compiler optimizations}

	#pragma region<Reduce to the case that |f>=0| and |q>0|>
	if (f >= 0) negative = false;
	else {
		negate(f); negative = true;
	}
	if (q < 0)
	{
		negate(q); negative = !negative;
	}
	#pragma endregion

	if (f < unity) n = 0;
	else {
		n = f / unity; f = f % unity;
		if (q <= el_gordo / n) n = n * q;
		else {
			arith_error = true; n = el_gordo;
		}
	}
	f = f + unity;
	#pragma region <Compute... 113>
	p = half_unit; //{that's $2^{15}$; the invariants hold now with $k=16$}
	if (q < fraction_four)
		do {
			if (myodd(f)) p = half(p + q); else p = half(p);
			f = half(f);
		} while (!(f == 1));
	else 
		do {
			if (myodd(f)) p = p + half(q - p); else p = half(p);
			f = half(f);
		} while (!(f == 1));
	#pragma endregion
	be_careful= n - el_gordo;
	if (be_careful + p > 0)
	{
		arith_error = true; n = el_gordo - p;
	}
	if (negative) return -(n + p);
	else return n + p;
}


// 114
scaled make_scaled(int p, int q)
{
	int f; //{the fraction bits, with a leading 1 bit}
	int n; //{the integer part of $\vert p/q\vert$}
	bool negative; //{should the result be negated?}
	int be_careful; //{disables certain compiler optimizations}

	if (p >= 0) negative = false;
	else { 
		negate(p); negative = true;
	}
	if (q <= 0)
	{ 
#ifndef NO_DEBUG
		if (q == 0) confusion(/*/*/47);
#endif
		negate(q); negative = !negative;
	}
	n = p / q; p = p % q;
	if (n >= 0100000)
	{ 
		arith_error = true;
		if (negative) return -el_gordo; else return el_gordo;
	}
	else { 
		n = (n - 1) * unity;
		#pragma region <Compute... 115>
		f = 1;
		do {
			be_careful = p - q; p = be_careful + p;
			if (p >= 0) f = f + f + 1;
			else {
				_double(f); p = p + q;
			}
		} while (!(f >= unity));
		be_careful = p - q;
		if (be_careful + p >= 0) f++;
		#pragma endregion
		
		if (negative) return -(f + n); else return f + n;
	}
}


// 116
fraction velocity(fraction st, fraction ct, fraction sf, fraction cf, scaled t)
{
	int acc, num, denom; //{registers for intermediate calculations}
	
	acc = take_fraction(st - (sf / 16), sf - (st / 16));
	acc = take_fraction(acc,ct - cf);
	num = fraction_two + take_fraction(acc,379625062);
	//{$2^{28}\sqrt2\approx379625062.497$}
	denom = fraction_three + take_fraction(ct,497706707) + take_fraction(cf,307599661);
	//{$3\cdot2^{27}\cdot(\sqrt5-1)\approx497706706.78$ and $3\cdot2^{27}\cdot(3-\sqrt5\,)\approx307599661.22$}
	if (t != unity) num = make_scaled(num,t);
	//{|make_scaled(fraction,scaled)=fraction|}
	if (num / 4 >= denom) return fraction_four;
	else return make_fraction(num,denom);
}

// 117
int ab_vs_cd(int a,int b,int c,int d)
{
	int q, r; //{temporary registers}
	
	#pragma region <Reduce to the case that |a,c>=0|, |b,d>0|>
	if (a < 0)
	{
		negate(a); negate(b);
	}
	if (c < 0)
	{ 
		negate(c); negate(d);
	}
	if (d <= 0)
	{ 
		if (b >= 0)
			if (((a == 0) || (b == 0)) && ((c == 0) || (d == 0))) return_sign(0);
			else return_sign(1);
		if (d == 0)
			if (a == 0) return_sign(0); else return_sign(-1);
		q = a; a = c; c = q; q = -b; b = -d; d = q;
	}
	else if (b <= 0)
	{
		if (b < 0)
			if (a > 0) return_sign(-1);
		if (c == 0) return_sign(0); else return_sign(-1);
	}
	#pragma endregion
	
	
	while (true) { 
		q  = a / d; r = c / b;
		if (q != r)
			if (q > r) return_sign(1); else return_sign(-1);
		q = a % d; r = c % b;
		if (r == 0)
			if (q == 0) return_sign(0); else return_sign(1);
		if (q == 0) return_sign(-1);
		a = b; b = q; c = d; d = r;
	} //{now |a>d>0| and |c>b>0|}
}


// 119
scaled floor_scaled(scaled x) // 2^16 floor(x/2^16)
{
	int be_careful; // temporary register

	if (x >= 0) return x - (x % unity);
	else {
		be_careful = x + 1; return x + ((-be_careful) % unity) + 1 - unity;
	}
}

int floor_unscaled(scaled x) // floor(x/2^16)
{
	int be_careful; // temporary register

	if (x >= 0) return x / unity;
	else {
		be_careful = x + 1; return -(1 + ((-be_careful) / unity));
	}
}

int round_unscaled(scaled x) // floor(x/2^16 + .5)
{
	int be_careful; // temporary register
	if (x >= half_unit) return 1 + ((x - half_unit) / unity);
	else if (x >= -half_unit) return 0;
	else {
		be_careful = x + 1; return -(1 + ((-be_careful - half_unit) / unity));
	}
}

scaled round_fraction(fraction x) // floor(x/2^12 + .5)
{
	int be_careful; // temporary register
	if (x >= 2048) return 1 + ((x - 2048) / 4096);
	else if (x >= -2048) return 0;
	else {
		be_careful = x + 1; return -(1 + ((-be_careful - 2048) / 4096));
	}
}

// 121
scaled square_rt(scaled x)
{
	small_number k; //{iteration control counter}
	int y,q; //{registers for intermediate calculations}
	if (x <= 0)
		#pragma region <Handle square root of zero or negative argument 122>
	{
		if (x < 0)
		{ 
			print_err(/*Square root of */310); print_scaled(x); print(/* has been replaced by 0*/311);
			help2(/*Since I don't take square roots of negative numbers,*/312,
				/*I'm zeroing this one. Proceed, with fingers crossed.*/313); error();
		}
		return 0;
	}
		#pragma endregion
	else { 
		k = 23; q = 2;
		while (x < fraction_two) // {i.e., while x < 2 29 }
		{ 
			k--; x = x + x + x + x;
		}
		if (x < fraction_four) y = 0;
		else { 
			x = x - fraction_four; y = 1;
		};
		do {
			#pragma region <Decrease k by 1, maintaining the invariant relations between x, y, and q 123>
			_double(x); _double(y);
			if (x >= fraction_four) // {note that fraction_four = 2^30 }
			{ 
				x = x - fraction_four; y++;
			}
			_double(x); y = y + y - q; _double(q);
			if (x >= fraction_four)
			{ 
				x = x - fraction_four; y++;
			}
			if (y > q)
			{ 
				y = y - q; q = q + 2;
			}
			else if (y <= 0)
			{ 
				q = q - 2; y = y + q;
			}
			k--;
			#pragma endregion
		} while (!( k == 0));
		return half(q);
	}
}


// 124
int pyth_add(int a, int b)
{
	fraction r; //{register used to transform |a| and |b|}
	bool big; //{is the result dangerously near $2^{31}$?}
	a = myabs(a); b = myabs(b);
	if (a < b)
	{ 
	r = b; b = a; a = r;
	} //{now |0<=b<=a|}
	if (b > 0)
	{ 
		if (a < fraction_two) big = false;
		else { 
			a = a / 4; b = b / 4; big = true;
		} //{we reduced the precision to avoid arithmetic overflow}
		#pragma region <Replace |a| by an approximation to ...>
		while (true) { 
		r = make_fraction(b,a);
		r = take_fraction(r,r); //{now $r\approx b^2/a^2$}
		if (r == 0) goto done;
		r = make_fraction(r,fraction_four + r);
		a = a + take_fraction(a + a,r); b = take_fraction(b,r);
		}
	done:		
		#pragma endregion
		if (big)
			if (a < fraction_two) a = a + a + a + a;
			else { 
				arith_error = true; a = el_gordo;
			}
	}
	return a;
}



// 126
int pyth_sub(int a, int b)
{
	fraction r; //{register used to transform |a| and |b|}
	bool big; //{is the input dangerously near $2^{31}$?}
	a = myabs(a); b = myabs(b);
	if (a <= b) 
		#pragma region <Handle erroneous |pyth_sub| and set |a:=0|>
	{
		if (a < b)
		{
			print_err(/*Pythagorean subtraction */314); print_scaled(a);
			print(/*+-+*/315); print_scaled(b); print(/* has been replaced by 0*/311);
			help2(/*Since I don't take square roots of negative numbers,*/312,
				/*I'm zeroing this one. Proceed, with fingers crossed.*/313);
			error();
		}
		a = 0;
	}	
		#pragma endregion
	else {
		if (a < fraction_four) big = false;
		else {
			a = half(a); b = half(b); big = true;
		}
		#pragma region <Replace |a| by an approximation to... 127>
		while (true) {
			r = make_fraction(b,a);
			r = take_fraction(r,r); //{now $r\approx b^2/a^2$}
			if (r == 0) goto done;
			r = make_fraction(r,fraction_four-r);
			a = a - take_fraction(a + a,r); b = take_fraction(b,r);
		}
		done:		
		#pragma endregion
		if (big) a = a + a;
	}
	return a;
}



// 132
scaled m_log(scaled x)
{
	int y, z; //{auxiliary registers}
	int k; //{iteration counter}
	if (x <= 0) 
		#pragma region <Handle non-positive logarithm>
	{ 
		print_err(/*Logarithm of */316);
		print_scaled(x); print(/* has been replaced by 0*/311);
		help2(/*Since I don't take logs of non-positive numbers,*/317,
			/*I'm zeroing this one. Proceed, with fingers crossed.*/313);
		error(); return 0;
	}
		#pragma endregion
	else { 
		y = 1302456956 + 4 - 100; //{$14\times2^{27}\ln2\approx1302456956.421063$}
		z = 27595 + 6553600; //{and $2^{16}\times .421063\approx 27595$}
		while (x < fraction_four)
		{ 
			_double(x); y = y - 93032639; z = z - 48782;
		} //{$2^{27}\ln2\approx 93032639.74436163$ and $2^{16}\times.74436163\approx 48782$}
		y = y + (z / unity); k = 2;
		while (x > fraction_four + 4)
			#pragma region <Increase |k| until |x| can be multiplied by a factor of... 133>
		{ 
			z = ((x - 1) / two_to_the[k]) + 1; //{$z=\lceil x/2^k\rceil$}
			while (x < fraction_four + z)
			{ 
				z = half(z + 1); k = k + 1;
			};
			y = y + spec_log[k]; x = x - z;
		}
			#pragma endregion
		return y / 8;
	}
}


// 135
scaled m_exp(scaled x)
{
	small_number k; //{loop control index}
	int y, z; //{auxiliary registers}
	if (x > 174436200)
	//{$2^{24}\ln((2^{31}-1)/2^{16})\approx 174436199.51$}
	{
		arith_error = true; return el_gordo;
	}
	else if (x < -197694359) return 0;
	//{$2^{24}\ln(2^{-1}/2^{16})\approx-197694359.45$}
	else {
		if (x <= 0)
		{ 
			z = -8 * x; y = 04000000; //{$y=2^{20}$}
		}
		else {
			if (x <= 127919879) z = 1023359037 - 8 * x;
			//{$2^{27}\ln((2^{31}-1)/2^{20})\approx 1023359037.125$}
			else z = 8 * (174436200 - x); //{|z| is always nonnegative}
			y = el_gordo;
		}
		#pragma region <Multiply |y| by...136>
		k = 1;
		while (z > 0)
		{ 
			while (z >= spec_log[k])
			{ 
				z = z - spec_log[k];
				y = y - 1 - ((y - two_to_the[k - 1]) / two_to_the[k]);
			}
			k++;
		}
		#pragma endregion
		if (x <= 127919879) return (y + 8) / 16; else return y;
	}
}



// 139
angle n_arg(int x, int y)
{
	angle z; //{auxiliary register}
	int t; //{temporary storage}
	small_number k; //{loop counter}
	int  octant;//:first_octant..sixth_octant; //{octant code}

	if (x >= 0) octant = first_octant;
	else { 
		negate(x); octant = first_octant + negate_x;
	}
	if (y < 0)
	{ 
		negate(y); octant = octant + negate_y;
	}
	if (x < y)
	{ 
		t = y; y = x; x = t; octant = octant + switch_x_and_y;
	}
	if (x == 0) 
		#pragma region <Handle undefined arg>
	{ 
		print_err(/*angle(0,0) is taken as zero*/318);
		help2(/*The `angle' between two identical points is undefined.*/319,
			/*I'm zeroing this one. Proceed, with fingers crossed.*/313);
		error(); return 0;
	}
		#pragma endregion
	else { 
		#pragma region <Set variable |z| to the arg of $(x,y)$>
		while (x >= fraction_two)
		{ 
			x = half(x); y = half(y);
		}
		z = 0;
		if (y > 0)
		{ 
			while (x < fraction_one)
			{ 
				_double(x); _double(y);
			}
			#pragma region <Increase |z| to the arg of $(x,y)$>
			k = 0;
			do {
				_double(y); k++;
				if (y > x)
				{ 
					z = z + spec_atan[k]; t = x; x = x + (y / two_to_the[k + k]); y = y - t;
				}
			} while (!(k == 15));
			do { 
				_double(y); k++;
				if (y > x)
				{ 
					z = z + spec_atan[k]; y = y - x;
				}
			} while (!(k == 26));			
			#pragma endregion
		}
		#pragma endregion
		
		#pragma region <Return an appropriate answer based on |z| and |octant|>
		switch(octant) {
			case first_octant:return z;
			case second_octant:return ninety_deg-z;
			case third_octant:return ninety_deg+z;
			case fourth_octant:return one_eighty_deg-z;
			case fifth_octant:return z-one_eighty_deg;
			case sixth_octant:return -z-ninety_deg;
			case seventh_octant:return z-ninety_deg;
			case eighth_octant:return -z;
		} //{there are no other cases}
		
		#pragma endregion
	}
	// NOTE: should not get here, gets rid of compiler warning
	assert(false);
	return -10000;
}





// 145
void n_sin_cos(angle z) //{computes a multiple of the sine and cosine}
{
	small_number k; //{loop control variable}
	int q; //:0..7; //{specifies the quadrant}
	fraction r; //{magnitude of |(x,y)|}
	int x, y, t; //{temporary registers}
	
	while (z < 0) z = z + three_sixty_deg;
	z = z % three_sixty_deg; //{now |0<=z<three_sixty_deg|}
	q = z / forty_five_deg; z = z % forty_five_deg;
	x = fraction_one; y = x;
	if (!myodd(q)) z = forty_five_deg - z;
	#pragma region <Subtract angle |z| from |(x,y)|>
	k = 1;
	while (z > 0)
	{
		if (z >= spec_atan[k])
		{ 
			z = z - spec_atan[k]; t = x;
			x = t + y / two_to_the[k];
			y = y - t / two_to_the[k];
		}
		k++;
	}
	if (y < 0) y = 0; //{this precaution may never be needed}
	
	#pragma endregion
	
	#pragma region <Convert |(x,y)| to the octant determined by~|q|>
	switch(q) {
	case 0:
		//do_nothing;
		break;
	case 1:
		t = x; x = y; y = t;
		break;
	case 2:
		t = x; x = -y; y = t;
		break;
	case 3:
		negate(x);
		break;
	case 4:
		negate(x); negate(y);
		break;
	case 5:
		t = x; x = -y; y = -t;
		break;
	case 6:
		t = x; x = y; y = -t;
		break;
	case 7:
		negate(y);
		break;
	} //{there are no other cases}

	#pragma endregion
	r = pyth_add(x,y); n_cos = make_fraction(x,r); n_sin = make_fraction(y,r);
}


// 149
void new_randoms()
{
	int k; // 0..54, index into randoms
	fraction x;
	
	for (k = 0; k <= 23; k++) {
		x = randoms[k] - randoms[k + 31];
		if (x < 0) x = x + fraction_one;
		randoms[k] = x;
	}
	for (k = 24; k <= 54; k++) {
		x = randoms[k] - randoms[k - 24];
		if (x < 0) x = x + fraction_one;
		randoms[k] = x;
	}
	j_random = 54;
}


// 150
void init_randoms(scaled seed)
{
	fraction j, jj, k; // more or less random integers
	int i; // 0..54, index into randoms

	j = myabs(seed);
	while (j >= fraction_one) j = half(j);
	k = 1;
	for (i = 0; i <= 54; i++) {
		jj = k; k = j - k; j = jj;
		if (k < 0) k = k + fraction_one;
		randoms[(i * 21) % 55] = j;
	}
	new_randoms(); new_randoms(); new_randoms(); // "warm up" the array
}

// 151
scaled unif_rand(scaled x)
{
	scaled y; // trial value

	next_random; y = take_fraction(myabs(x), randoms[j_random]);
	if (y == myabs(x)) return 0;
	else if (x > 0) return y;
	else return -y;

}

// 152
scaled norm_rand()
{
	int x, u, l; // what the book would call 2^16X, 2^28U, and -2^24lnU

	do {
		do {
			next_random; x = take_fraction(112429, randoms[j_random] - fraction_half);
			// 2^16sqrt(8/e) approx 112428.82793
			next_random; u = randoms[j_random];
		} while (!(myabs(x) < u));
		x = make_fraction(x, u); l = 139548960 - m_log(u); // 2^24*12ln2 approx 139548959.6165
	} while (!(ab_vs_cd(1024, l, x, x) >= 0));
	return x;
}

// 157
#ifndef NO_DEBUG 
void print_word(memory_word w) // {prints w in all ways}
{
	print_int(w.an_int); print_char(/* */32);
	print_scaled(w.sc); print_char(/* */32); print_scaled(w.sc / 010000); print_ln();
	print_int(w.hh.lh); print_char(/*=*/61); print_int(w.hh.b0); print_char(/*:*/58); print_int(w.hh.b1);
	print_char(/*;*/59); print_int(w.hh.rh); print_char(/* */32);
	print_int(w.qqqq.b0); print_char(/*:*/58); print_int(w.qqqq.b1); print_char(/*:*/58); print_int(w.qqqq.b2);
	print_char(/*:*/58); print_int(w.qqqq.b3);
}
#endif



// 163
pointer get_avail() // single-word node allocation
{
	pointer p; // the new node being got
	p = avail; // get top location in the avail  stack
	if (p != null) avail = link(avail); // and pop it off
	else if (mem_end < mem_max) { // or go into virgin territory
		mem_end++; p = mem_end;
	}
	else {
		hi_mem_min--; p = hi_mem_min;
		if (hi_mem_min <= lo_mem_max) {
			runaway();
			overflow(/*main memory size*/320, mem_max + 1 - mem_min); // quit; all one-word nodes are busy
		}
	}
	link(p) = null; // provide an oft-desired initialization of the new node
#ifndef NO_STAT
	dyn_used++;
#endif
	return p;
}

// 164
void free_avail(pointer s)
{
	link(s) = avail;
	avail = s;
	/*stat*/
#ifndef NO_STAT
	dyn_used--;
#endif
	/*tats*/
}

// 167
pointer get_node(int s) // {variable-size node allocation}
{
	pointer p; //{the node currently under inspection}
	pointer q; //{the node physically after node p}
	int r; //{the newly allocated node, or a candidate for this honor}
	int t,tt; //{temporary registers}
restart: 
	p = rover; //{start at some free node in the ring}
	do { 
		#pragma region <Try to allocate within node p and its physical successors, and goto found if allocation was possible 169>
		
		q = p + node_size(p); //{find the physical successor}
		while (is_empty(q)) //{merge node p with node q }
		{ 
			t = rlink(q); tt = llink(q);
			if (q == rover) rover = t;
			llink(t) = tt; rlink(tt) = t;
			q = q + node_size(q);
		}
		r = q - s;
		if (r > p + 1) 
			#pragma region <Allocate from the top of node p and goto found 170>
			{
				node_size(p) = r - p; //{store the remaining size}
				rover = p; //{start searching here next time}
				goto found;
			}
			#pragma endregion
		if (r == p)
			if (rlink(p) != p)
				#pragma region <Allocate entire node p and goto found 171>
				{
					rover = rlink(p); t = llink(p); llink(rover) = t; rlink(t) = rover; goto found;
				}
				#pragma endregion
		node_size(p) = q - p; //{reset the size in case it grew}		
		
		
		#pragma endregion
		p = rlink(p); //{move to the next node in the ring}
	} while (!( p == rover)); //{repeat until the whole list has been traversed}
	if (s == 010000000000)
	{
		return max_halfword;
	}
	if (lo_mem_max + 2 < hi_mem_min)
		if (lo_mem_max + 2 <= mem_min + max_halfword)
			#pragma region <Grow more variable-size memory and goto restart 168>
		{
			if (hi_mem_min - lo_mem_max >= 1998) t = lo_mem_max + 1000;
			else t = lo_mem_max + 1 + (hi_mem_min - lo_mem_max) / 2; //{lo_mem_max +2 = t < hi_mem_min }
			if (t > mem_min + max_halfword) t = mem_min + max_halfword;
			p = llink(rover); q = lo_mem_max; rlink(p) = q; llink(rover) = q;
			rlink(q) = rover; llink(q) = p; link(q) = empty_flag; node_size(q) = t - lo_mem_max;
			lo_mem_max = t; link(lo_mem_max) = null; info(lo_mem_max) = null; rover = q; goto restart;
		}
			#pragma endregion
	overflow(/*main memory size*/320,mem_max + 1 - mem_min); //{sorry, nothing satisfactory is left}
	found: link(r) = null; //{this node is now nonempty}
	#ifndef NO_STAT 
	var_used = var_used + s; //{maintain usage statistics}
	#endif
	return r;
}


// 172
void free_node(pointer p, halfword s) // variable-size node liberation
{
	pointer q; // llink(rover)

	node_size(p) = s; link(p) = empty_flag; q = llink(rover); llink(p) = q; rlink(p) = rover; // set both links
	llink(rover) = p; rlink(q) = p; // insert p into the ring
#ifndef NO_STAT
	var_used = var_used - s; // maintain statistics
#endif
}

// 173
#ifndef NO_INIT

void sort_avail()
{
	pointer p, q, r;
	pointer old_rover;

	p = get_node(010000000000);
	p = rlink(rover); rlink(rover) = max_halfword; old_rover = rover;
	while (p != old_rover) 
		#pragma region <Sort p into the list starting at rover and advance p to rlink(p) 174>
	{
		if (p < rover) {
			q = p; p = rlink(q); rlink(q) = rover; rover = q;
		}
		else {
			q = rover;
			while (rlink(q) < p) q = rlink(q);
			
			r = rlink(p); rlink(p) = rlink(q); rlink(q) = p; p = r;
		}
	}
		#pragma endregion

	p = rover;
	while (rlink(p) != max_halfword) {
		llink(rlink(p)) = p; p = rlink(p);
	}
	rlink(p) = rover; llink(rover) = p;
}

#endif




// 177
void flush_list(pointer p) // makes list of single-word nodes available
{
	pointer q, r; // list traversers

	if (p >= hi_mem_min)
		if (p != sentinel) {
			r = p;
			do {
				q = r; r = link(r);
#ifndef NO_STAT
				dyn_used--;
#endif
				if (r < hi_mem_min) goto done;
			} while (!(r == sentinel));
		done: // now q is the last node on the list
			link(q) = avail; avail = p;
		}
}

void flush_node_list(pointer p)
{
	pointer q;
	while (p != null) {
		q = p; p = link(p);
		if (q < hi_mem_min) free_node(q, 2); else free_avail(q);
	}
}



// 180
#ifndef NO_DEBUG
void check_mem(bool print_locs)
{
	pointer p, q, r; // current locations of interest in mem
	bool clobbered; // is something amiss?

	for (p = mem_min; p <= lo_mem_max; p++) _free[p] = false; // you can probably do this faster
	for (p = hi_mem_min; p <= mem_end; p++) _free[p] = false; // ditto

	#pragma region <Check singleword avail list 181>
	p = avail; q = null; clobbered = false;
	while (p != null) {
		if (p > mem_end || p < hi_mem_min) clobbered = true;
		else if (_free[p]) clobbered = true;
		if (clobbered) {
			print_nl(/*AVAIL list clobbered at */321); print_int(q); goto done1;
		}
		_free[p] = true; q = p; p = link(q);
	}
done1:
	#pragma endregion

	#pragma region <Check variablesize avail list 182>
	p = rover; q = null; clobbered = false;
	do {
		if (p >= lo_mem_max || p < mem_min) clobbered = true;
		else if (rlink(p) >= lo_mem_max || rlink(p) < mem_min) clobbered = true;
		else if (!is_empty(p) || node_size(p) < 2 || (p + node_size(p)) > lo_mem_max ||
		          (llink(rlink(p)) != p)) clobbered = true;
		if (clobbered) {
			print_nl(/*Double-AVAIL list clobbered at */322); print_int(q); goto done2;
		}
		for (q = p; q <= p + node_size(p) - 1; q++) // mark all locations free
		{
			if (_free[q]) {
				print_nl(/*Doubly free location at */323); print_int(q); goto done2;
			}
			_free[q] = true;
		}
		q = p; p = rlink(p);
	} while (!(p == rover));
done2:
	;
	#pragma endregion

	#pragma region <Check flags of unavailable nodes 183>
	p = mem_min;
	while (p <= lo_mem_max) // node p should not be empty
	{
		if (is_empty(p)) {
			print_nl(/*Bad flag at */324); print_int(p);
		}
		while (p <= lo_mem_max && !_free[p]) p++;
		while (p <= lo_mem_max && _free[p]) p++;
	}
	#pragma endregion

	#pragma region <Check the list of linear dependencies 617>
	q = dep_head; p = link(q);
	while (p != dep_head) {
		if (prev_dep(p) != q) {
			print_nl(/*Bad PREVDEP at */325); print_int(p);
		}
		p = dep_list(p); r = inf_val;
		do {
			if (value(info(p)) >= value(r)) {
				print_nl(/*Out of order at */326); print_int(p);
			}
			r = info(p); q = p; p = link(q);
		} while (!(r == null));
	}
	#pragma endregion

	if (print_locs)
		#pragma region <Print newly busy locations 184>
	{
		print_nl(/*New busy locs:*/327);
		for (p = mem_min; p <= lo_mem_max; p++)
			if (!_free[p] && (p > was_lo_max || was_free[p])) {
				print_char(/* */32); print_int(p);
			}
		for (p = hi_mem_min; p <= mem_end; p++)
			if (!_free[p] && (p < was_hi_min || p > was_mem_end || was_free[p])) {
				print_char(/* */32); print_int(p);
			}
	}
		#pragma endregion
	for (p = mem_min; p <= lo_mem_max; p++) was_free[p] = _free[p];
	for (p = hi_mem_min; p <= mem_end; p++) was_free[p] = _free[p]; // was_free = free might be faster
	was_mem_end = mem_end; was_lo_max = lo_mem_max; was_hi_min = hi_mem_min;

}

#endif


// 185
#ifndef NO_DEBUG
void search_mem(pointer p) // {look for pointers to p}
{
	int q; // {current_position being searched}
	for (q = mem_min; q <= lo_mem_max; q++)
	{
		if (link(q) == p)
		{ 
			print_nl(/*LINK(*/328); print_int(q); print_char(/*)*/41);
		}
		if (info(q) == p)
		{ 
			print_nl(/*INFO(*/329); print_int(q); print_char(/*)*/41);
		}
	}
	for (q = hi_mem_min; q <= mem_end; q++)
	{ 
		if (link(q) == p)
		{ 
			print_nl(/*LINK(*/328); print_int(q); print_char(/*)*/41);
		}
		if (info(q) == p)
		{ 
			print_nl(/*INFO(*/329); print_int(q); print_char(/*)*/41);
		}
	}
	#pragma region <Search eqtb for equivalents equal to p 209>
	for (q = 1; q <= hash_end; q++)
	{
		if (equiv(q) == p)
		{ print_nl(/*EQUIV(*/330); print_int(q); print_char(/*)*/41);
		}
	}
	#pragma endregion
}
#endif


// 187
void print_type(small_number t)
{
	switch (t)
	{
	case vacuous: print(/*vacuous*/331); break;
	case boolean_type: print(/*boolean*/332); break;
	case unknown_boolean: print(/*unknown boolean*/333); break;
	case string_type: print(/*string*/334); break;
	case unknown_string: print(/*unknown string*/335); break;
	case pen_type: print(/*pen*/336); break;
	case unknown_pen: print(/*unknown pen*/337); break;
	case future_pen: print(/*future pen*/338); break;
	case path_type: print(/*path*/339); break;
	case unknown_path: print(/*unknown path*/340); break;
	case picture_type: print(/*picture*/341); break;
	case unknown_picture: print(/*unknown picture*/342); break;
	case transform_type: print(/*transform*/343); break;
	case pair_type: print(/*pair*/344); break;
	case known: print(/*known numeric*/345); break;
	case dependent: print(/*dependent*/346); break;
	case proto_dependent: print(/*proto-dependent*/347); break;
	case numeric_type: print(/*numeric*/348); break;
	case independent: print(/*independent*/349); break;
	case token_list: print(/*token list*/350); break;
	case structured: print(/*structured*/351); break;
	case unsuffixed_macro: print(/*unsuffixed macro*/352); break;
	case suffixed_macro: print(/*suffixed macro*/353); break;
	default:
		print(/*undefined*/354);
		break;
	}
}


// 189
void print_op(quarterword c)
{
	if (c <= numeric_type) print_type(c);
	else
		switch (c) {
		case true_code:
			print(/*true*/355);
			break;
		case false_code:
			print(/*false*/356);
			break;
		case null_picture_code:
			print(/*nullpicture*/357);
			break;
		case null_pen_code:
			print(/*nullpen*/358);
			break;
		case job_name_op:
			print(/*jobname*/359);
			break;
		case read_string_op:
			print(/*readstring*/360);
			break;
		case pen_circle:
			print(/*pencircle*/361);
			break;
		case normal_deviate:
			print(/*normaldeviate*/362);
			break;
		case odd_op:
			print(/*odd*/363);
			break;
		case known_op:
			print(/*known*/364);
			break;
		case unknown_op:
			print(/*unknown*/365);
			break;
		case not_op:
			print(/*not*/366);
			break;
		case decimal:
			print(/*decimal*/367);
			break;
		case reverse:
			print(/*reverse*/368);
			break;
		case make_path_op:
			print(/*makepath*/369);
			break;
		case make_pen_op:
			print(/*makepen*/370);
			break;
		case total_weight_op:
			print(/*totalweight*/371);
			break;
		case oct_op:
			print(/*oct*/372);
			break;
		case hex_op:
			print(/*hex*/373);
			break;
		case ASCII_op:
			print(/*ASCII*/374);
			break;
		case char_op:
			print(/*char*/375);
			break;
		case length_op:
			print(/*length*/376);
			break;
		case turning_op:
			print(/*turningnumber*/377);
			break;
		case x_part:
			print(/*xpart*/378);
			break;
		case y_part:
			print(/*ypart*/379);
			break;
		case xx_part:
			print(/*xxpart*/380);
			break;
		case xy_part:
			print(/*xypart*/381);
			break;
		case yx_part:
			print(/*yxpart*/382);
			break;
		case yy_part:
			print(/*yypart*/383);
			break;
		case sqrt_op:
			print(/*sqrt*/384);
			break;
		case m_exp_op:
			print(/*mexp*/385);
			break;
		case m_log_op:
			print(/*mlog*/386);
			break;
		case sin_d_op:
			print(/*sind*/387);
			break;
		case cos_d_op:
			print(/*cosd*/388);
			break;
		case floor_op:
			print(/*floor*/389);
			break;
		case uniform_deviate:
			print(/*uniformdeviate*/390);
			break;
		case char_exists_op:
			print(/*charexists*/391);
			break;
		case angle_op:
			print(/*angle*/392);
			break;
		case cycle_op:
			print(/*cycle*/393);
			break;
		case plus:
			print(/*+*/43);
			break;
		case minus:
			print(/*-*/45);
			break;
		case times:
			print(/***/42);
			break;
		case over:
			print(/*/*/47);
			break;
		case pythag_add:
			print(/*++*/394);
			break;
		case pythag_sub:
			print(/*+-+*/315);
			break;
		case or_op:
			print(/*or*/395);
			break;
		case and_op:
			print(/*and*/396);
			break;
		case less_than:
			print(/*<*/60);
			break;
		case less_or_equal:
			print(/*<=*/397);
			break;
		case greater_than:
			print(/*>*/62);
			break;
		case greater_or_equal:
			print(/*>=*/398);
			break;
		case equal_to:
			print(/*=*/61);
			break;
		case unequal_to:
			print(/*<>*/399);
			break;
		case concatenate:
			print(/*&*/38);
			break;
		case rotated_by:
			print(/*rotated*/400);
			break;
		case slanted_by:
			print(/*slanted*/401);
			break;
		case scaled_by:
			print(/*scaled*/402);
			break;
		case shifted_by:
			print(/*shifted*/403);
			break;
		case transformed_by:
			print(/*transformed*/404);
			break;
		case x_scaled:
			print(/*xscaled*/405);
			break;
		case y_scaled:
			print(/*yscaled*/406);
			break;
		case z_scaled:
			print(/*zscaled*/407);
			break;
		case intersect:
			print(/*intersectiontimes*/408);
			break;
		case substring_of:
			print(/*substring*/409);
			break;
		case subpath_of:
			print(/*subpath*/410);
			break;
		case direction_time_of:
			print(/*directiontime*/411);
			break;
		case point_of:
			print(/*point*/412);
			break;
		case precontrol_of:
			print(/*precontrol*/413);
			break;
		case postcontrol_of:
			print(/*postcontrol*/414);
			break;
		case pen_offset_of:
			print(/*penoffset*/415);
			break;
		default:
			print(/*..*/416);
			break;
		}
}

// 195
void begin_diagnostic() // prepare to do some tracing
{
	old_setting = selector;
	if (internal[tracing_online] <= 0 && selector == term_and_log) {
		selector--;
		if (history == spotless) history = warning_issued;
	}
}

void end_diagnostic(bool blank_line) // restore proper conditions after tracing
{
	print_nl(/**/289);
	if (blank_line) print_ln();
	selector = old_setting;
}

// 197
void print_diagnostic(str_number s, str_number t, bool nuline)
{
	begin_diagnostic();
	if (nuline) print_nl(s); else print(s);
	print(/* at line */417); print_int(line); print(t); print_char(/*:*/58);
}


// 205
pointer id_lookup(int j, int l)
{
	int h; // hash code
	pointer p; // index in hash array
	pointer k; // index in buffer array

	if (l == 1)
		#pragma region <Treat special case of length 1 and goto found 206>
	{
		p = buffer[j] + 1; text(p) = p - 1; goto found;
	}
		#pragma endregion

	#pragma region <Compute the hash code h 208>
	h = buffer[j];
	for (k = j + 1; k <= j + l - 1; k++) {
		h = h + h + buffer[k];
		while (h >= hash_prime) h = h - hash_prime;
	}
	#pragma endregion

	p = h + hash_base; // we start searching here; note that 0 <= h < hash_prime

	while (true) {
		if (text(p) > 0)
			if (length(text(p)) == l)
				if (str_eq_buf(text(p), j)) goto found;
		if (next(p) == 0)
			#pragma region <Insert a new symbolic token after p, then make p point to it and goto found 207>
		{
			if (text(p) > 0) {
				do {
					if (hash_is_full) overflow(/*hash size*/418, hash_size);
					hash_used--;
				} while (!(text(hash_used) == 0));
				next(p) = hash_used; p = hash_used;
			}
			str_room(l);
			for (k = j; k <= j + l - 1; k++) append_char(buffer[k]);
			text(p) = make_string(); str_ref[text(p)] = max_str_ref;
#ifndef NO_STAT
			st_count++;
#endif
			goto found;
		}
			#pragma endregion
		p = next(p);
	}
found:
	return p;
}

// 210
#ifndef NO_INIT
void primitive(str_number s, halfword c, halfword o)
{
	pool_pointer k; // index into str_pool
	small_number j; // index into buffer
	small_number l; // length of the string

	k = str_start[s]; l = str_start[s + 1] - k; // we will move s into the (empty) buffer
	for (j = 0; j <= l - 1; j++) buffer[j] = str_pool[k + j];
	cur_sym = id_lookup(0, l);
	if (s >= 256) {
		flush_string(str_ptr - 1); text(cur_sym) = s;
	}
	eq_type(cur_sym) = c; equiv(cur_sym) = o;
}
#endif

// 215
pointer new_num_tok(scaled v)
{
	pointer p; // the new node

	p = get_node(token_node_size); value(p) = v; type(p) = known; name_type(p) = token;
	return p;
}



// 216
void flush_token_list(pointer p)
{
	pointer q; //{the node being recycled}

	while (p != null)
	{ 
		q = p; p = link(p);
		if (q >= hi_mem_min) free_avail(q);
		else { 
			switch(type(q)) {
				case vacuous: case boolean_type: case known:
					//do_nothing;
					break;
				case string_type:
					delete_str_ref(value(q));
					break;
				case unknown_types: case pen_type: case path_type: case future_pen: case picture_type:
				case pair_type: case transform_type: case dependent: case proto_dependent: case independent:
					g_pointer = q; token_recycle();
					break;
		
				default: confusion(/*token*/419); break;
			}
		free_node(q,token_node_size);
		}
	}
}

// 217
void show_token_list(int p, int q, int l, int null_tally)
{
	small_number _class, c; //{the |char_class| of previous and new tokens}
	int r, v; //{temporary registers}

	_class = percent_class;
	tally = null_tally;
	while (p != null &&  tally < l)
	{
		if (p == q) 
			#pragma region <Do magic computation>
			set_trick_count;
			#pragma endregion

		#pragma region <Display token |p| and set |c| to its class; but |return| if there are problems>
		c = letter_class; //{the default}
		if (p < mem_min || p > mem_end)
		{ 
			print(/* CLOBBERED*/420); return;
		}
		if (p < hi_mem_min)
			#pragma region <Display two-word token>
			if (name_type(p) == token)
				if (type(p) == known)
					#pragma region <Display a numeric token>
				{ 
					if (_class == digit_class) print_char(/* */32);
					v = value(p);
					if (v < 0)
					{ 
						if (_class == left_bracket_class) print_char(/* */32);
						print_char(/*[*/91); print_scaled(v); print_char(/*]*/93);
						c = right_bracket_class;
					}
					else { 
						print_scaled(v); c = digit_class;
					}
				}		
					#pragma endregion
				else if (type(p) != string_type) print(/* BAD*/421);
				else { print_char(/*"*/34); slow_print(value(p)); print_char(/*"*/34);
					c = string_class;
				}
			else if (name_type(p) != capsule || type(p) < vacuous || type(p) > independent)
				print(/* BAD*/421);
			else { 
				g_pointer = p; print_capsule(); c = right_paren_class;
			}
		
			#pragma endregion
		else { 
			r = info(p);
			if (r >= expr_base)
				#pragma region <Display a parameter token>
			{ 
				if (r < suffix_base)
				{ 
					print(/*(EXPR*/422); r = r - (expr_base);
				}
				else if (r < text_base)
				{ 
					print(/*(SUFFIX*/423); r = r - (suffix_base);
				}
				else { 
					print(/*(TEXT*/424); r = r - (text_base);
				}
				print_int(r); print_char(/*)*/41); c = right_paren_class;
			}
				#pragma endregion
			else if (r < 1)
				if (r == 0)
					#pragma region <Display a collective subscript>
				{ 
					if (_class == left_bracket_class) print_char(/* */32);
					print(/*[]*/425); c = right_bracket_class;
				}
					#pragma endregion
				else print(/* IMPOSSIBLE*/426);
			else { 
				r = text(r);
				if (r < 0 || r >= str_ptr)
					print(/* NONEXISTENT*/427);
				else 
					#pragma region <Print string |r| as a symbolic token and set |c| to its class>
				{ 
					c = char_class[so(str_pool[str_start[r]])];
					if (c == _class)
					switch(c) {
						case letter_class: print_char(/*.*/46); break;
						case isolated_classes: /*do_nothing;*/ break;
						default: print_char(/* */32); break;
					}
					slow_print(r);
				}
					#pragma endregion
			}
		}		
		#pragma endregion
		_class = c; p = link(p);
	}
	if (p != null) print(/* ETC.*/428);

}


// 224
void print_capsule()
{
	print_char(/*(*/40); print_exp(g_pointer, 0); print_char(/*)*/41);
}

void token_recycle()
{
	recycle_value(g_pointer);
}


// 226
void delete_mac_ref(pointer p) // p points to the reference count of a macro list that is losing one reference
{
	if (ref_count(p) == null) flush_token_list(p);
	else ref_count(p)--;
}

// 227
void show_macro(pointer p, int q, int l)
{
	pointer r; // temporary storage
	p = link(p); // bypass the reference count
	while (info(p) > text_macro) {
		r = link(p); link(p) = null; show_token_list(p, null, l, 0); link(p) = r; p = r;
		if (l > 0) l = l - tally; else return;
	} // control printing of 'ETC.'
	tally = 0;
	switch (info(p)) {
	case general_macro: print(/*->*/429); break;
	case primary_macro: case secondary_macro: case tertiary_macro:
		print_char(/*<*/60);
		print_cmd_mod(param_type, info(p)); print(/*>->*/430);
		break;
	case expr_macro: print(/*<expr>->*/431); break;
	case of_macro: print(/*<expr>of<primary>->*/432); break;
	case suffix_macro: print(/*<suffix>->*/433); break;
	case text_macro: print(/*<text>->*/434); break;
	}
	show_token_list(link(p), q, l - tally, 0);

}


// 232
void init_big_node(pointer p)
{
	pointer q; //{the new node}
	small_number s; //{its size}
	s = big_node_size[type(p)]; q = get_node(s);
	do {
		s = s - 2; 
		#pragma region <Make variable |q+s| newly independent>
		new_indep(q+s);
		#pragma endregion
		name_type(q + s) = half(s) + x_part_sector; link(q + s) = null;
	} while (!(s == 0));
	link(q) = p; value(p) = q;
}


// 233
pointer id_transform()
{
	pointer p,q,r; //{list manipulation registers}
	p = get_node(value_node_size); type(p) = transform_type; name_type(p) = capsule;
	value(p) = null; init_big_node(p); q = value(p); r = q + transform_node_size;
	do { r = r - 2; type(r) = known; value(r) = 0;
	} while (!( r == q));
	value(xx_part_loc(q)) = unity; value(yy_part_loc(q)) = unity; return p;
}


// 234
void new_root(pointer x)
{
	pointer p; // the new node
	p = get_node(value_node_size); type(p) = undefined; name_type(p) = root; link(p) = x;
	equiv(x) = p;
}


// 235
void print_variable_name(pointer p)
{
	pointer q; // a token list that will name the variable's suffix
	pointer r; // temporary for token list creation

	while (name_type(p) >= x_part_sector)
		#pragma region <Preface the output with a part specifier; return in the case of a capsule 237>
	{
		switch (name_type(p)) {
		case x_part_sector: print_char(/*x*/120); break;
		case y_part_sector: print_char(/*y*/121); break;
		case xx_part_sector: print(/*xx*/435); break;
		case xy_part_sector: print(/*xy*/436); break;
		case yx_part_sector: print(/*yx*/437); break;
		case yy_part_sector: print(/*yy*/438); break;
		case capsule:
			print(/*%CAPSULE*/439); print_int(p - null); return;
			break;
		}
		print(/*part */440); p = link(p - 2 * (name_type(p) - x_part_sector));
	}
		#pragma endregion
	q = null;

	while (name_type(p) > saved_root)
		#pragma region <Ascend one level, pushing a token onto list q and replacing p by its parent 236>
	{
		if (name_type(p) == subscr) {
			r = new_num_tok(subscript(p));
			do {
				p = link(p);
			} while (!(name_type(p) == attr));
		}
		else if (name_type(p) == structured_root) {
			p = link(p); goto found;
		}
		else {
			if (name_type(p) != attr) confusion(/*var*/441);
			r = get_avail(); info(r) = attr_loc(p);
		}
		link(r) = q; q = r;
	found:
		p = parent(p);
	}
		#pragma endregion
	r = get_avail(); info(r) = link(p); link(r) = q;
	if (name_type(p) == saved_root) print(/*(SAVED)*/442);
	show_token_list(r, null, el_gordo, tally); flush_token_list(r);
}


// 238
bool interesting(pointer p)
{
	small_number t; // a name_type
	
	if (internal[tracing_capsules] > 0) return true;
	else {
		t = name_type(p);
		if (t >= x_part_sector)
			if (t != capsule) t = name_type(link(p - 2 * (t - x_part_sector)));
		return (t != capsule);
	}
}

// 239
pointer new_structure(pointer p)
{
	pointer q,r=max_halfword; //{list manipulation registers}
	switch(name_type(p)) {
		case root:
			q = link(p); r = get_node(value_node_size); equiv(q) = r;
			break;
		case subscr: 
			#pragma region <Link a new subscript node |r| in place of node |p|>
			q = p;
			do { 
				q = link(q);
			} while (!(name_type(q)==attr));
			q = parent(q); r = subscr_head_loc(q); //{|link(r)=subscr_head(q)|}
			do { 
				q = r; r = link(r);
			} while (!( r==p));
			r = get_node(subscr_node_size);
			link(q) = r; subscript(r) = subscript(p);
						
			#pragma endregion
			break;
		case attr: 
			#pragma region <Link a new attribute node |r| in place of node |p|>
			q = parent(p); r = attr_head(q);
			do { 
				q = r; r = link(r);
			} while (!( r==p));
			r = get_node(attr_node_size); link(q) = r;
			mem[attr_loc_loc(r)] = mem[attr_loc_loc(p)]; //{copy |attr_loc| and |parent|}
			if (attr_loc(p) == collective_subscript)
			{ 
				q = subscr_head_loc(parent(p));
				while (link(q) != p) q = link(q);
				link(q) = r;
			}
			#pragma endregion
			break;
		default: confusion(/*struct*/443); break;
	}
	link(r) = link(p); type(r) = structured; name_type(r) = name_type(p);
	attr_head(r) = p; name_type(p) = structured_root;
	q = get_node(attr_node_size); link(p) = q; subscr_head(r) = q;
	parent(q) = r; type(q) = undefined; name_type(q) = attr; link(q) = end_attr;
	attr_loc(q) = collective_subscript; return r;
}



// 242
pointer find_variable(pointer t)
{
	pointer p,q,r,s; //{nodes in the ``value'' line}
	pointer pp,qq,rr,ss; //{nodes in the ``collective'' line}
	int n; //{subscript or attribute}
	memory_word save_word; //{temporary storage for a word of |mem|}

	p = info(t); t = link(t);
	if (eq_type(p) % outer_tag != tag_token) abort_find;
	if (equiv(p) == null) new_root(p);
	p = equiv(p); pp = p;
	while (t != null)
	{
		#pragma region <Make sure that both nodes |p| and |pp| are of |structured| type>
		if (type(pp) != structured)
		{ 
			if (type(pp) > structured) abort_find;
			ss = new_structure(pp);
			if (p == pp) p = ss;
			pp = ss;
		} //{now |type(pp)=structured|}
		if (type(p) != structured) //{it cannot be |>structured|}
			p = new_structure(p); //{now |type(p)=structured|}		
		#pragma endregion
		if (t < hi_mem_min)
		#pragma region <Descend one level for the subscript |value(t)|>
		{ 
			n = value(t);
			pp = link(attr_head(pp)); //{now |attr_loc(pp)=collective_subscript|}
			q = link(attr_head(p)); save_word = mem[subscript_loc(q)];
			subscript(q) = el_gordo; s = subscr_head_loc(p); //{|link(s)=subscr_head(p)|}
			do { 
				r = s; s = link(s);
			} while (!( n <= subscript(s)));
			if (n == subscript(s)) p = s;
			else {
				p = get_node(subscr_node_size); link(r) = p; link(p) = s;
				subscript(p) = n; name_type(p) = subscr; type(p) = undefined;
			}
			mem[subscript_loc(q)] = save_word;
		}	
		#pragma endregion
		else 
			#pragma region <Descend one level for the attribute |info(t)|>
		{
			n = info(t);
			ss = attr_head(pp);
			do { 
				rr = ss; ss = link(ss);
			} while (!( n <= attr_loc(ss)));
			if (n < attr_loc(ss))
			{ 
				qq = get_node(attr_node_size); link(rr) = qq; link(qq) = ss;
				attr_loc(qq) = n; name_type(qq) = attr; type(qq) = undefined;
				parent(qq) = pp; ss = qq;
			}
			if (p == pp)
			{
				p = ss; pp = ss;
			}
			else  {
				pp = ss; s = attr_head(p);
				do { 
					r = s; s = link(s);
				} while (!( n <= attr_loc(s)));
				if (n == attr_loc(s)) p = s;
				else  {
					q = get_node(attr_node_size); link(r) = q; link(q) = s;
					attr_loc(q) = n; name_type(q) = attr; type(q) = undefined;
					parent(q) = p; p = q;
				}
			}
		}		
			#pragma endregion
			
		t = link(t);
	}
	if (type(pp) >= structured)
		if (type(pp) == structured) pp = attr_head(pp); else abort_find;
	if (type(p) == structured) p = attr_head(p);
	if (type(p) == undefined)
	{ 
		if (type(pp) == undefined)
		{ 
			type(pp) = numeric_type; value(pp) = null;
		}
		type(p) = type(pp); value(p) = null;
	}
	return p;

}


// 246
void flush_variable(pointer p, pointer t, bool discard_suffixes)
{
	pointer q, r; //{list manipulation}
	halfword n; //{attribute to match}
	while (t != null)
	{
		if (type(p) != structured) return;
		n = info(t); t = link(t);
		if (n == collective_subscript)
		{ 
			r = subscr_head_loc(p); q = link(r); //{|q=subscr_head(p)|}
			while (name_type(q) == subscr)
			{ 
				flush_variable(q,t,discard_suffixes);
				if (t == null)
				if (type(q) == structured) r = q;
				else {
					link(r) = link(q); free_node(q,subscr_node_size);
				}
				else r = q;
				q = link(r);
			}
		}
		p = attr_head(p);
		do { 
			r = p; p = link(p);
		} while (!(attr_loc(p) >= n));
		if (attr_loc(p) != n) return;
	}
	if (discard_suffixes) flush_below_variable(p);
	else  {
		if (type(p) == structured) p = attr_head(p);
		recycle_value(p);
	}
}



// 247
void flush_below_variable(pointer p)
{
	pointer q,r; //{list manipulation registers}
	if (type(p) != structured)
		recycle_value(p); //{this sets |type(p)=undefined|}
	else  { 
		q = subscr_head(p);
		while (name_type(q) == subscr)
		{ 
			flush_below_variable(q); r = q; q = link(q);
			free_node(r,subscr_node_size);
		}
		r = attr_head(p); q = link(r); recycle_value(r);
		if (name_type(p) <= saved_root) free_node(r,value_node_size);
		else free_node(r,subscr_node_size);
		//{we assume that |subscr_node_size=attr_node_size|}
		do { 
			flush_below_variable(q); r = q; q = link(q); free_node(r,attr_node_size);
		} while (!(q == end_attr));
		type(p) = undefined;
	}
}


// 248
small_number und_type(pointer p)
{
	switch(type(p)) {
		case undefined:
		case vacuous:
			return undefined;
			break;
		case boolean_type:
		case unknown_boolean:
			return unknown_boolean;
			break;
		case string_type:
		case unknown_string:
			return unknown_string;
			break;
		case pen_type:
		case unknown_pen:
		case future_pen:
			return unknown_pen;
			break;
		case path_type:
		case unknown_path:
			return unknown_path;
			break;
		case picture_type:
		case unknown_picture:
			return unknown_picture;
			break;
		case transform_type:
		case pair_type:
		case numeric_type:
			return type(p);
			break;
		case known:
		case dependent:
		case proto_dependent:
		case independent:
			return numeric_type;
			break;
	}

	// NOTE: should not get here, prevents compiler warning
	assert(false);
	return -10000;

}



// 249
void clear_symbol(pointer p, bool saving)
{
	pointer q; // equiv(p)

	q = equiv(p);
	switch(eq_type(p) % outer_tag) {
		case defined_macro: case secondary_primary_macro: case tertiary_secondary_macro:
		case expression_tertiary_macro:
			if (!saving) delete_mac_ref(q);
			break;
		case tag_token:
			if (q != null)
				if (saving) name_type(q) = saved_root;
				else {
					flush_below_variable(q); free_node(q, value_node_size);
				}
			break;
		default:
			// do_nothing
			break;
	}
	eqtb[p] = eqtb[frozen_undefined];
}

// 252
void save_variable(pointer q)
{
	pointer p; // temporary register
	if (save_ptr != null) {
		p = get_node(save_node_size); info(p) = q; link(p) = save_ptr; saved_equiv(p) = eqtb[q];
		save_ptr = p;
	}
	clear_symbol(q, (save_ptr != null));
}

// 253
void save_internal(halfword q)
{
	pointer p; // new item for the save stack
	if (save_ptr != null) {
		p = get_node(save_node_size); info(p) = hash_end + q; link(p) = save_ptr;
		value(p) = internal[q]; save_ptr = p;
	}
}


// 254
void unsave()
{
	pointer q; // index to saved item
	pointer p; // temporary register

	while (info(save_ptr) != 0) {
		q = info(save_ptr);
		if (q > hash_end) {
			if (internal[tracing_restores] > 0) {
				begin_diagnostic(); print_nl(/*{restoring */444); slow_print(int_name[q - (hash_end)]);
				print_char(/*=*/61); print_scaled(value(save_ptr)); print_char(/*}*/125); end_diagnostic(false);
			}
			internal[q - (hash_end)] = value(save_ptr);
		}
		else {
			if (internal[tracing_restores] > 0) {
				begin_diagnostic(); print_nl(/*{restoring */444); slow_print(text(q)); print_char(/*}*/125);
				end_diagnostic(false);
			}
			clear_symbol(q, false); eqtb[q] = saved_equiv(save_ptr);
			if (eq_type(q) % outer_tag == tag_token) {
				p = equiv(q);
				if (p != null) name_type(p) = root;
			}
		}
		p = link(save_ptr); free_node(save_ptr, save_node_size); save_ptr = p;
	}
	p = link(save_ptr); free_avail(save_ptr); save_ptr = p;
}


// 257
void print_path(pointer h, str_number s, bool nuline)
{
	pointer p, q; //{for list traversal}
	print_diagnostic(/*Path*/445,s,nuline); print_ln();
	p = h;
	do { 
		q = link(p);
		if (p == null || q == null)
		{ 
			print_nl(/*???*/266); goto done; //{this won't happen}
		}
		#pragma region <Print information for adjacent knots |p| and |q|>
		print_two(x_coord(p),y_coord(p));
		switch (right_type(p)) {
			case endpoint: 
				if (left_type(p) == _open_) print(/*{open?}*/446); //{can't happen}
				if (left_type(q) != endpoint || q != h) q = null; //{force an error}
				goto done1;
				break;
			case _explicit: 
				#pragma region <Print control points between |p| and |q|, then |goto done1|>
				print(/*..controls */447); print_two(right_x(p),right_y(p)); print(/* and */448);
				if (left_type(q) != _explicit) print(/*??*/449); //{can't happen}
				else print_two(left_x(q),left_y(q));
				goto done1;
				#pragma endregion
				break;
			case _open_: 
				#pragma region <Print information for a curve that begins |open|>
				if (left_type(p) != _explicit && left_type(p) != _open_)
					print(/*{open?}*/446); //{can't happen}
				#pragma endregion
				break;
			case curl: case given: 
				#pragma region <Print information for a curve that begins |curl| or |given|>
				if (left_type(p) == _open_) print(/*??*/449); //{can't happen}
				if (right_type(p) == curl)
				{ 
					print(/*{curl */450); print_scaled(right_curl(p));
				}
				else  { 
					n_sin_cos(right_given(p)); print_char(/*{*/123);
					print_scaled(n_cos); print_char(/*,*/44); print_scaled(n_sin);
				}
				print_char(/*}*/125);
				#pragma endregion
				break;
			default:
				print(/*???*/266); //{can't happen}
				break;
		}
		if (left_type(q) <= _explicit) print(/*..control?*/451); //{can't happen}
		else if (right_tension(p) != unity || left_tension(q) != unity)
		#pragma region <Print tension between |p| and |q|>
		{ 
			print(/*..tension */452);
			if (right_tension(p) < 0) print(/*atleast*/453);
			print_scaled(myabs(right_tension(p)));
			if (right_tension(p) != left_tension(q))
			{ 
				print(/* and */448);
				if (left_tension(q) < 0) print(/*atleast*/453);
				print_scaled(myabs(left_tension(q)));
			}
		}
		#pragma endregion
	done1:
		#pragma endregion
		p = q;
		if (p != h || left_type(h) != endpoint)
		#pragma region <Print two dots, followed by |given| or |curl| if present>
		{ 
			print_nl(/* ..*/454);
			if (left_type(p) == given)
			{ 
				n_sin_cos(left_given(p)); print_char(/*{*/123);
				print_scaled(n_cos); print_char(/*,*/44);
				print_scaled(n_sin); print_char(/*}*/125);
			}
			else if (left_type(p) == curl)
			{ 
				print(/*{curl */450); print_scaled(left_curl(p)); print_char(/*}*/125);
			}
		}
		#pragma endregion
	} while (!(p == h));
	if (left_type(h) != endpoint) print(/*cycle*/393);
done:
	end_diagnostic(true);
}



// 264
pointer copy_knot(pointer p)
{
	pointer q; //{the copy}
	int k;//:0..knot_node_size-1; //{runs through the words of a knot node}
	q = get_node(knot_node_size);
	for (k = 0; k <= knot_node_size - 1; k++) mem[q + k] = mem[p + k];
	return q;
}


// 265
pointer copy_path(pointer p)
{
	pointer q, pp, qq; //{for list manipulation}
	q = get_node(knot_node_size); //{this will correspond to |p|}
	qq = q; pp = p;
	while (true) {
		left_type(qq) = left_type(pp);
		right_type(qq) = right_type(pp);
		x_coord(qq) = x_coord(pp); y_coord(qq) = y_coord(pp);
		left_x(qq) = left_x(pp); left_y(qq) = left_y(pp);
		right_x(qq) = right_x(pp); right_y(qq) = right_y(pp);
		if (link(pp) == p)
		{ 
			link(qq) = q; return q;
		}
		link(qq) = get_node(knot_node_size); qq = link(qq); pp = link(pp);
	}
}


// 266
pointer htap_ypoc(pointer p)
{
	pointer q, pp, qq, rr; //{for list manipulation}
	q = get_node(knot_node_size); //{this will correspond to |p|}
	qq = q; pp = p;
	while (true) { 
		right_type(qq) = left_type(pp); left_type(qq) = right_type(pp);
		x_coord(qq) = x_coord(pp); y_coord(qq) = y_coord(pp);
		right_x(qq) = left_x(pp); right_y(qq) = left_y(pp);
		left_x(qq) = right_x(pp); left_y(qq) = right_y(pp);
		if (link(pp) == p)
		{ 
			link(q) = qq; path_tail = pp; return q;
		}
		rr = get_node(knot_node_size); 
		link(rr) = qq; qq = rr; pp = link(pp);
	}
}

// 268
void toss_knot_list(pointer p)
{
	pointer q; //{the node being freed}
	pointer r; //{the next node}
	q = p;
	do { 
		r = link(q); free_node(q,knot_node_size); q = r;
	} while (!(q == p));
}


// 269
void make_choices(pointer knots)
{
	pointer h; //{the first breakpoint}
	pointer p,q; //{consecutive breakpoints being processed}
	#pragma region <Other local variables for |make_choices|>
	int k, n; //:0..path_size; //{current and final knot numbers}
	pointer s, t; //{registers for list traversal}
	scaled delx, dely; //{directions where |open| meets |explicit|}
	fraction sine, cosine; //{trig functions of various angles}
	#pragma endregion

	check_arith; //{make sure that |arith_error=false|}
	if (internal[tracing_choices] > 0)
		print_path(knots,/*, before choices*/455,true);
	#pragma region <If consecutive knots are equal, join them explicitly>
	p = knots;
	do {
		q = link(p);
		if (x_coord(p) == x_coord(q))
			if (y_coord(p) == y_coord(q))
				if (right_type(p) > _explicit)
				{
					right_type(p) = _explicit;
					if (left_type(p) == _open_)
					{
						left_type(p) = curl; left_curl(p) = unity;
					}
					left_type(q) = _explicit;
					if (right_type(q) == _open_)
					{
						right_type(q) = curl; right_curl(q) = unity;
					}
					right_x(p) = x_coord(p); left_x(q) = x_coord(p);
					right_y(p) = y_coord(p); left_y(q) = y_coord(p);
				}
		p = q;
	} while (!(p == knots));
	#pragma endregion

	#pragma region <Find the first breakpoint, |h|, on the path; insert an artificial breakpoint if the path is an unbroken cycle>
	h = knots;
	while (true) {
		if (left_type(h) != _open_) goto done;
		if (right_type(h) != _open_) goto done;
		h = link(h);
		if (h == knots)
		{
			left_type(h) = end_cycle; goto done;
		}
	}
	done:
	#pragma endregion
	p = h;
	do {
	#pragma region <Fill in the control points between |p| and the next breakpoint, then advance |p| to that breakpoint>
	q = link(p);
	if (right_type(p) >= given)
	{
		while (left_type(q) == _open_ && right_type(q) == _open_) q = link(q);
		#pragma region <Fill in the control information between consecutive breakpoints |p| and |q|>
		
		#pragma region <Calculate the turning angles $psi_k$ and the distances $d_{k,k+1}$; set $n$ to the length of the path>;
		k = 0; s = p; n = path_size;
		do {
			t = link(s);
			delta_x[k] = x_coord(t) - x_coord(s);
			delta_y[k] = y_coord(t) - y_coord(s);
			delta[k] = pyth_add(delta_x[k],delta_y[k]);
			if (k > 0)
			{
				sine = make_fraction(delta_y[k - 1],delta[k - 1]);
				cosine = make_fraction(delta_x[k - 1],delta[k - 1]);
				psi[k] = n_arg(take_fraction(delta_x[k],cosine) + take_fraction(delta_y[k],sine),
							take_fraction(delta_y[k],cosine) - take_fraction(delta_x[k],sine));
			}
			k++; s = t;
			if (k == path_size) overflow(/*path size*/456,path_size);
			if (s == q) n = k;
		} while (!( (k >= n && left_type(s) != end_cycle)));
		if (k == n) psi[n] =0; else psi[k] = psi[1];

		#pragma endregion

		#pragma region <Remove |open| types at the breakpoints>
		if (left_type(q) == _open_)
		{
			delx = right_x(q) - x_coord(q); dely = right_y(q) - y_coord(q);
			if (delx == 0 && dely == 0)
			{ 
				left_type(q) = curl; left_curl(q) = unity;
			}
			else  {
				left_type(q) = given; left_given(q) = n_arg(delx,dely);
			}
		}
		if (right_type(p) == _open_ && left_type(p) == _explicit)
		{
			delx = x_coord(p) - left_x(p); dely = y_coord(p) - left_y(p);
			if (delx == 0 && dely == 0)
			{ 
				right_type(p) = curl; right_curl(p) = unity;
			}
			else {
				right_type(p) = given; right_given(p) = n_arg(delx,dely);
			}
		}
		#pragma endregion
		solve_choices(p,q,n);
		#pragma endregion
	}
	p = q;
	#pragma endregion
	} while (!(p == h));
	if (internal[tracing_choices] > 0)
		print_path(knots,/*, after choices*/457,true);
	if (arith_error)
	#pragma region <Report an unexpected problem during the choice-making>
	{
		print_err(/*Some number got too big*/458);
		help2(/*The path that I just computed is out of range.*/459,
		/*So it will probably look funny. Proceed, for a laugh.*/460);
		put_get_error(); arith_error = false;
	}
	#pragma endregion
}

// 284
void solve_choices(pointer p, pointer q, halfword n)
{
	int k; //:0..path_size; //{current knot number}
	pointer r=max_halfword, s, t; //{registers for list traversal}

	#pragma region <Other local variables for |solve_choices|>
	fraction aa, bb, cc, ff, acc; //{temporary registers}
	scaled dd, ee; //{likewise, but |scaled|}
	scaled lt, rt; //{tension values}
	#pragma endregion

	k = 0; s = p;
	while (true) {
		t = link(s);
		if (k == 0)
			#pragma region <Get the linear equations started; or |return| with the control points in place, if linear equations neednt be solved>
			switch(right_type(s)) {
				case given:
					if (left_type(t) == given)
					#pragma region <Reduce to simple case of two givens and |return|>
					{
						aa = n_arg(delta_x[0],delta_y[0]);
						n_sin_cos(right_given(p) - aa); ct = n_cos; st = n_sin;
						n_sin_cos(left_given(q) - aa); cf = n_cos; sf = -n_sin;
						set_controls(p,q,0); return;
					}			
					#pragma endregion
					else
						#pragma region <Set up the equation for a given value of $theta_0$>
					{ 
						vv[0] = right_given(s) - n_arg(delta_x[0],delta_y[0]);
						reduce_angle(vv[0]);
						uu[0] = 0; ww[0] = 0;
					}				
						#pragma endregion
					break;
				case curl: 
					if (left_type(t) == curl)
						#pragma region <Reduce to simple case of straight line and |return|>
					
					{
						right_type(p) = _explicit; left_type(q) = _explicit;
						lt = myabs(left_tension(q)); rt = myabs(right_tension(p));
						if (rt == unity)
						{
							if (delta_x[0] >= 0) right_x(p) = x_coord(p) + ((delta_x[0] + 1) / 3);
							else right_x(p) = x_coord(p) + ((delta_x[0] - 1) / 3);
							if (delta_y[0] >= 0) right_y(p) = y_coord(p) + ((delta_y[0] + 1) / 3);
							else right_y(p) = y_coord(p) + ((delta_y[0] - 1) / 3);
						}
						else  {
							ff = make_fraction(unity,3 * rt); //{$alpha/3$}
							right_x(p) = x_coord(p) + take_fraction(delta_x[0],ff);
							right_y(p) = y_coord(p) + take_fraction(delta_y[0],ff);
						}
						if (lt == unity)
						{
							if (delta_x[0] >= 0) left_x(q) = x_coord(q) - ((delta_x[0] + 1) / 3);
							else left_x(q) = x_coord(q) - ((delta_x[0] - 1) / 3);
							if (delta_y[0] >= 0) left_y(q) = y_coord(q) - ((delta_y[0] + 1) / 3);
							else left_y(q) = y_coord(q) - ((delta_y[0] - 1) / 3);
						}
						else  {
							ff = make_fraction(unity,3 * lt); //{$beta/3$}
							left_x(q) = x_coord(q) - take_fraction(delta_x[0],ff);
							left_y(q) = y_coord(q) - take_fraction(delta_y[0],ff);
						}
						return;
					}				
						#pragma endregion
					else
						#pragma region <Set up the equation for a curl at $theta_0$>
					{
						cc = right_curl(s); lt = myabs(left_tension(t)); rt = myabs(right_tension(s));
						if (rt == unity && lt == unity)
							uu[0] = make_fraction(cc + cc + unity,cc + two);
						else uu[0] = curl_ratio(cc,rt,lt);
						vv[0] = -take_fraction(psi[1],uu[0]); ww[0] = 0;
					}				
						#pragma endregion
					break;
				case _open_:
					uu[0] = 0; vv[0] = 0; ww[0] = fraction_one; //{this begins a cycle}
					break;
			} //{there are no other cases}
			#pragma endregion
		else switch(left_type(s)) {
			case end_cycle: case _open_:
				#pragma region <Set up equation to match mock curvatures ... 287>
				#pragma region <Calculate the values ... 288>
				if (myabs(right_tension(r)) == unity)
				{ 
					aa = fraction_half; dd = 2 * delta[k];
				}
				else  {
					aa = make_fraction(unity,3 * myabs(right_tension(r)) - unity);
					dd = take_fraction(delta[k],fraction_three - make_fraction(unity,myabs(right_tension(r))));
				}
				if (myabs(left_tension(t)) == unity)
				{ 
					bb = fraction_half; ee = 2 * delta[k - 1];
				}
				else  {
					bb = make_fraction(unity,3 * myabs(left_tension(t)) - unity);
					ee = take_fraction(delta[k - 1],fraction_three - make_fraction(unity,myabs(left_tension(t))));
				}
				cc = fraction_one - take_fraction(uu[k - 1],aa);
				#pragma endregion

				#pragma region <Calculate the ratio... 289>
				dd = take_fraction(dd,cc); lt = myabs(left_tension(s)); rt = myabs(right_tension(s));
				if (lt != rt) //{$\beta_k^{-1}\ne\alpha_k^{-1}$}
					if (lt < rt)
					{ 
						ff = make_fraction(lt,rt);
						ff = take_fraction(ff,ff); //{$\alpha_k^2/\beta_k^2$}
						dd = take_fraction(dd,ff);
					}
					else  {
						ff = make_fraction(rt,lt);
						ff = take_fraction(ff,ff); //{$\beta_k^2/\alpha_k^2$}
						ee = take_fraction(ee,ff);
					}
				ff = make_fraction(ee,ee + dd);
				#pragma endregion
				uu[k] = take_fraction(ff,bb);
				#pragma region <Calculate the values of $v_k$ and $w_k$>
				acc = -take_fraction(psi[k + 1],uu[k]);
				if (right_type(r) == curl)
				{ 
					ww[k] = 0;
					vv[k] = acc - take_fraction(psi[1],fraction_one - ff);
				}
				else  {
					ff = make_fraction(fraction_one - ff,cc); //{this is $B_k/(C_k+B_k-u_{k-1}A_k)<5$}
					acc = acc - take_fraction(psi[k],ff);
					ff = take_fraction(ff,aa); //{this is $A_k/(C_k+B_k-u_{k-1}A_k)$}
					vv[k] = acc - take_fraction(vv[k - 1],ff);
					if (ww[k - 1] == 0) ww[k] = 0;
					else ww[k] = -take_fraction(ww[k - 1],ff);
				}
				#pragma endregion
				if (left_type(s) == end_cycle)
				#pragma region <Adjust $theta_n$ to equal $theta_0$ and |goto found|>
				{
					aa = 0; bb = fraction_one; //{we have |k=n|}
					do { 
						k--;
						if (k == 0) k = n;
						aa = vv[k]-take_fraction(aa,uu[k]);
						bb = ww[k]-take_fraction(bb,uu[k]);
					} while (!(k == n)); //{now $\theta_n=\\{aa}+\\{bb}\cdot\theta_n$}
					aa = make_fraction(aa,fraction_one - bb);
					theta[n] = aa; vv[0] = aa;
					for (k = 1; k <= n - 1; k++) vv[k] = vv[k] + take_fraction(aa,ww[k]);
					goto found;
				}
				#pragma endregion
				#pragma endregion
				break;
			case curl:
				#pragma region <Set up equation for a curl at $theta_n$ and |goto found|>
				cc = left_curl(s); lt = myabs(left_tension(s)); rt = myabs(right_tension(r));
				if (rt == unity && lt == unity)
				ff = make_fraction(cc + cc + unity,cc + two);
				else ff = curl_ratio(cc,lt,rt);
				theta[n] = -make_fraction(take_fraction(vv[n - 1],ff), fraction_one - take_fraction(ff,uu[n - 1]));
				goto found;
				#pragma endregion
				break;
			case given:
				#pragma region <Calculate the given value of $theta_n$ and |goto found|>
				{
					theta[n] = left_given(s) - n_arg(delta_x[n - 1],delta_y[n - 1]);
					reduce_angle(theta[n]);
					goto found;
				}
				#pragma endregion
				break;
		} //{there are no other cases}
		r = s; s = t; k++;
	}
	found:
	#pragma region <Finish choosing angles and assigning control points>
	for (k = n - 1; k >= 0; k--) theta[k] = vv[k] - take_fraction(theta[k + 1],uu[k]);
	s = p; k = 0;
	do {
		t = link(s);
		n_sin_cos(theta[k]); st = n_sin; ct = n_cos;
		n_sin_cos(-psi[k + 1] - theta[k + 1]); sf = n_sin; cf = n_cos;
		set_controls(s,t,k);
		k++; s = t;
	} while (!(k == n));
	#pragma endregion
}


// 299
void set_controls(pointer p, pointer q, int k)
{
	fraction rr,ss; //{velocities, divided by thrice the tension}
	scaled lt,rt; //{tensions}
	fraction sine; //{$\sin(\theta+\phi)$}
	lt = myabs(left_tension(q)); rt = myabs(right_tension(p));
	rr = velocity(st,ct,sf,cf,rt);
	ss = velocity(sf,cf,st,ct,lt);
	if (right_tension(p) < 0 || left_tension(q) < 0)
		#pragma region <Decrease the velocities, if necessary, to stay inside the bounding triangle>
		if((st >= 0 && sf >= 0) || (st <= 0 && sf <= 0))
		{ 
			sine = take_fraction(myabs(st),cf) + take_fraction(myabs(sf),ct);
			if (sine > 0)
			{
				sine = take_fraction(sine,fraction_one + unity); //{safety factor}
				if (right_tension(p) < 0)
					if (ab_vs_cd(myabs(sf),fraction_one,rr,sine) < 0)
						rr = make_fraction(myabs(sf),sine);
				if (left_tension(q) < 0)
					if (ab_vs_cd(myabs(st),fraction_one,ss,sine) < 0)
						ss = make_fraction(myabs(st),sine);
			}
		}
		#pragma endregion
	right_x(p) = x_coord(p) + take_fraction(take_fraction(delta_x[k],ct)-take_fraction(delta_y[k],st),rr);
	right_y(p) = y_coord(p)+take_fraction(take_fraction(delta_y[k],ct)+take_fraction(delta_x[k],st),rr);
	left_x(q) = x_coord(q)-take_fraction(take_fraction(delta_x[k],cf)+take_fraction(delta_y[k],sf),ss);
	left_y(q) = y_coord(q)-take_fraction(take_fraction(delta_y[k],cf)-take_fraction(delta_x[k],sf),ss);
	right_type(p) = _explicit; left_type(q) = _explicit;
}


// 296
fraction curl_ratio(scaled gamma, scaled a_tension, scaled b_tension)
{
	fraction alpha,beta,num,denom,ff; //{registers}

	alpha = make_fraction(unity,a_tension);
	beta = make_fraction(unity,b_tension);
	if (alpha <= beta)
	{ 
		ff = make_fraction(alpha,beta); ff = take_fraction(ff,ff);
		gamma = take_fraction(gamma,ff);
		beta = beta / 010000; //{convert |fraction| to |scaled|}
		denom = take_fraction(gamma,alpha) + three - beta;
		num = take_fraction(gamma,fraction_three-alpha) + beta;
	}
	else { 
		ff = make_fraction(beta,alpha); ff = take_fraction(ff,ff);
		beta = take_fraction(beta,ff) / 010000; //{convert |fraction| to |scaled|}
		denom = take_fraction(gamma,alpha) + (ff / 1365) - beta;
		//{$1365\approx 2^{12}/3$}
		num = take_fraction(gamma,fraction_three - alpha) + beta;
	}
	if (num >= denom + denom + denom + denom) return fraction_four;
	else return make_fraction(num,denom);
}



// 311
void make_moves(scaled xx0,scaled xx1,scaled xx2,scaled xx3,scaled yy0,scaled yy1,scaled yy2,scaled yy3,small_number xi_corr,small_number eta_corr)
{
	int x1, x2, x3, m, r, y1, y2, y3, n, s, l;
	//{bisection variables explained above}
	int q, t, u, x2a, x3a, y2a, y3a; //{additional temporary registers}

	if (xx3 < xx0 || yy3 < yy0) confusion(/*m*/109);
	l = 16; bisect_ptr = 0;
	x1 = xx1 - xx0; x2 = xx2 - xx1; x3 = xx3 - xx2;
	if (xx0 >= xi_corr) r = (xx0 - xi_corr) % unity;
	else r = unity - 1 - ((-xx0 + xi_corr - 1) % unity);
	m = (xx3 - xx0 + r) / unity;
	y1 = yy1 - yy0; y2 = yy2 - yy1; y3 = yy3 - yy2;
	if (yy0 >= eta_corr) s = (yy0 - eta_corr) % unity;
	else s = unity - 1 -((-yy0 + eta_corr - 1) % unity);
	n = (yy3 - yy0 + s) / unity;
	if (xx3 - xx0 >= fraction_one || yy3 - yy0 >= fraction_one)
		#pragma region <Divide the variables by two, to avoid overflow problems>
	{ 
		x1 = half(x1+xi_corr); x2 = half(x2+xi_corr); x3 = half(x3+xi_corr);
		r = half(r+xi_corr);
		y1 = half(y1+eta_corr); y2 = half(y2+eta_corr); y3 = half(y3+eta_corr);
		s = half(s+eta_corr);
		l = 15;
	}
		
		#pragma endregion
	while (true) { 
	mycontinue:
		#pragma region <Make moves for current subinterval; if bisection is necessary, push the second subinterval onto the stack, and |goto continue| in order to handle the first subinterval>
		if (m == 0) 
			#pragma region <Move upward |n| steps>
			while (n > 0)
			{ 
				move_ptr++; move[move_ptr] = 1; n--;
			}
			#pragma endregion
		else if (n == 0) 
			#pragma region <Move to the right |m| steps>
			move[move_ptr] = move[move_ptr] + m;
			#pragma endregion
		else if (m + n == 2) 
			#pragma region <Make one move of each kind>
		{ 
			r = two_to_the[l] - r; s = two_to_the[l] - s;
			while (l < 30)
			{
				x3a = x3; x2a = half(x2+x3+xi_corr); x2 = half(x1+x2+xi_corr);
				x3 = half(x2+x2a+xi_corr);
				t = x1+x2+x3; r = r+r-xi_corr;
				y3a = y3; y2a = half(y2+y3+eta_corr); y2 = half(y1+y2+eta_corr);
				y3 = half(y2+y2a+eta_corr);
				u = y1+y2+y3; s = s+s-eta_corr;
				if (t < r) 
					if (u < s) 
						#pragma region <Switch to the right subinterval>
					{ 
						x1 = x3; x2 = x2a; x3 = x3a; r = r-t;
						y1 = y3; y2 = y2a; y3 = y3a; s = s-u;
					}
						#pragma endregion
					else { 
						#pragma region <Move up then right>
						{ 
							move_ptr++; move[move_ptr] = 2;
						}
						#pragma endregion
						goto done;
					}
				else if (u < s)
				{ 
					#pragma region <Move right then up>
					{ 
						move[move_ptr]++; move_ptr++; move[move_ptr] = 1;
					}
					#pragma endregion
					goto done;
				}
				l++;
			}
		r = r-xi_corr; s = s-eta_corr;
		if (ab_vs_cd(x1+x2+x3,s,y1+y2+y3,r)-xi_corr >= 0) 
			#pragma region <Move right then up>
		{ 
			move[move_ptr]++; move_ptr++; move[move_ptr] = 1;
		}
			#pragma endregion
		else 
			#pragma region <Move up then right>
		{ 
			move_ptr++; move[move_ptr] = 2;
		}
			#pragma endregion
		done:
			;
		}		
			#pragma endregion
		else { 
			l++; stack_l = l;
			stack_x3 = x3; stack_x2 = half(x2+x3+xi_corr); x2 = half(x1+x2+xi_corr);
			x3 = half(x2 + stack_x2 + xi_corr); stack_x1 = x3;
			r = r + r + xi_corr; t = x1 + x2 + x3 + r;
			q = t / two_to_the[l]; stack_r = t % two_to_the[l];
			stack_m = m - q; m = q;
			stack_y3 = y3; stack_y2 = half(y2 + y3 + eta_corr); y2 = half(y1 + y2 + eta_corr);
			y3 = half(y2 + stack_y2 + eta_corr); stack_y1 = y3;
			s = s + s + eta_corr; u = y1 + y2 + y3 + s;
			q = u / two_to_the[l]; stack_s = u % two_to_the[l];
			stack_n = n - q; n = q;
			bisect_ptr = bisect_ptr + move_increment; goto mycontinue;
		}

		#pragma endregion
		if (bisect_ptr == 0) return;
		#pragma region <Remove a subproblem for |make_moves| from the stack>
		bisect_ptr = bisect_ptr-move_increment;
		x1 = stack_x1; x2 = stack_x2; x3 = stack_x3; r = stack_r; m = stack_m;
		y1 = stack_y1; y2 = stack_y2; y3 = stack_y3; s = stack_s; n = stack_n;
		l = stack_l;
		#pragma endregion
	}
}

// 321
void smooth_moves(int b, int t)
{
	int k; //:1..move_size; //{index into |move|}
	int a, aa, aaa; //{original values of |move[k],move[k-1],move[k-2]|}
	if (t - b >= 3)
	{ 
		k = b + 2; aa = move[k - 1]; aaa = move[k - 2];
		do { 
			a = move[k];
			if (myabs(a - aa) > 1)
				#pragma region <Increase and decrease |move[k-1]| and |move[k]| by $delta_k$>
				if (a > aa)
				{ 
					if (aaa >= aa)
						if (a >= move[k + 1])
						{
							move[k - 1]++; move[k] = a - 1;
						}
				}
				else { 
					if (aaa <= aa) 
						if (a <= move[k + 1])
						{ 
							move[k - 1]--; move[k] = a + 1;
						}
				}	
				#pragma endregion
			k++; aaa = aa; aa = a;
		} while (!(k == t));
	}
}


// 326
void init_edges(pointer h) //{initialize an edge header to null values}
{
	knil(h) = h; link(h) = h;
	n_min(h) = zero_field + 4095; n_max(h) = zero_field - 4095;
	m_min(h) = zero_field + 4095; m_max(h) = zero_field - 4095;
	m_offset(h) = zero_field;
	last_window(h) = 0; last_window_time(h) = 0;
	n_rover(h) = h; n_pos(h) = 0;
}


// 328
void fix_offset()
{
	pointer p, q; //{list traversers}
	int delta; //{the amount of change}
	delta = 8 * (m_offset(cur_edges) - zero_field);
	m_offset(cur_edges) = zero_field;
	q = link(cur_edges);
	while (q != cur_edges)
	{ 
		p = sorted(q);
		while (p != sentinel)
		{ 
			info(p) = info(p) - delta; p = link(p);
		}
		p = unsorted(q);
		while (p > _void)
		{
			info(p) = info(p) - delta; p = link(p);
		}
		q = link(q);
	}
}



// 329
void edge_prep(int ml, int mr, int nl,int nr)
{
	halfword delta; //{amount of change}
	pointer p, q; //{for list manipulation}

	ml = ml + zero_field; mr = mr + zero_field;
	nl = nl + zero_field; nr = nr - 1 + zero_field;
	if (ml < m_min(cur_edges)) m_min(cur_edges) = ml;
	if (mr > m_max(cur_edges)) m_max(cur_edges) = mr;
	if (!valid_range(m_min(cur_edges) + m_offset(cur_edges) - zero_field) || !valid_range(m_max(cur_edges) + m_offset(cur_edges) - zero_field))
		fix_offset();
	if (empty_edges(cur_edges)) //{there are no rows}
	{ 
		n_min(cur_edges) = nr + 1; n_max(cur_edges) = nr;
	}
	if (nl < n_min(cur_edges))
		#pragma region <Insert exactly |n_min(cur_edges)-nl| empty rows at the bottom>
	{ 
		delta = n_min(cur_edges) - nl; n_min(cur_edges) = nl;
		p = link(cur_edges);
		do { 
			q = get_node(row_node_size); 
			sorted(q) = sentinel; unsorted(q) = _void;
			knil(p) = q; link(q) = p; p = q; delta--;
		} while (!(delta == 0));
		knil(p) = cur_edges; link(cur_edges) = p;
		if (n_rover(cur_edges) == cur_edges) n_pos(cur_edges) = nl - 1;
	}
		#pragma endregion
	if (nr > n_max(cur_edges))
		#pragma region <Insert exactly |nr-n_max(cur_edges)| empty rows at the top>
		{ 
			delta = nr - n_max(cur_edges); n_max(cur_edges) = nr;
			p = knil(cur_edges);
			do { 
				q = get_node(row_node_size); 
				sorted(q) = sentinel; unsorted(q) = _void;
				link(p) = q; knil(q) = p; p = q; delta--;
			} while (!(delta == 0));
			link(p) = cur_edges; knil(cur_edges) = p;
			if (n_rover(cur_edges) == cur_edges)
				n_pos(cur_edges) = nr + 1;
		}	
		#pragma endregion
}




// 332
void print_edges(str_number s, bool nuline, int x_off, int y_off)
{
	pointer p, q, r; //{for list traversal}
	int n; //{row number}
	print_diagnostic(/*Edge structure*/461,s,nuline);
	p = knil(cur_edges); n = n_max(cur_edges) - zero_field;
	while (p != cur_edges)
	{ 
		q = unsorted(p); r = sorted(p);
		if(q > _void || r != sentinel)
		{ 
			print_nl(/*row */462); print_int(n+y_off); print_char(/*:*/58);
			while (q > _void)
			{ 
				print_weight(q,x_off); q = link(q);
			}
			print(/* |*/463);
			while (r != sentinel)
			{
				print_weight(r,x_off); r = link(r);
			}
		}
		p = knil(p); n--;
	}
	end_diagnostic(true);
}


// 333
void print_weight(pointer q, int x_off)
{
	int w,m; //{unpacked weight and coordinate}
	int d; //{temporary data register}
	d = ho(info(q)); w = d % 8; m = (d / 8) - m_offset(cur_edges);
	if (file_offset > max_print_line - 9) print_nl(/* */32);
	else print_char(/* */32);
	print_int(m + x_off);
	while (w > zero_w)
	{
		print_char(/*+*/43); w--;
	}
	while (w < zero_w)
	{ 
		print_char(/*-*/45); w++;
	}
}



// 334
pointer copy_edges(pointer h)
{
	pointer p,r; //{variables that traverse the given structure}
	pointer hh,pp,qq,rr,ss; //{variables that traverse the new structure}
	
	hh = get_node(edge_header_size);
	mem[hh + 1] = mem[h + 1]; mem[hh + 2] = mem[h + 2];
	mem[hh + 3] = mem[h + 3]; mem[hh + 4] = mem[h + 4]; //{we've now copied |n_min|, |n_max|, |m_min|, |m_max|, |m_offset|, |last_window|, and |last_window_time|}
	n_pos(hh) = n_max(hh) + 1; n_rover(hh) = hh;
	p = link(h); qq = hh;
	while (p != h)
	{ 
		pp = get_node(row_node_size); 
		link(qq) = pp; knil(pp) = qq;
		#pragma region <Copy both |sorted| and |unsorted| lists of |p| to |pp|>
		r = sorted(p); rr = sorted_loc(pp); //{|link(rr)=sorted(pp)|}
		while (r != sentinel)
		{ 
			ss = get_avail(); link(rr) = ss; rr = ss; info(rr) = info(r);
			r = link(r);
		}
		link(rr) = sentinel;
		r = unsorted(p); rr = temp_head;
		while (r > _void)
		{ 
			ss = get_avail(); link(rr) = ss; rr = ss; info(rr) = info(r);
			r = link(r);
		}
		link(rr) = r; unsorted(pp) = link(temp_head);
		#pragma endregion
		p = link(p); qq = pp;
	}
	link(qq) = hh; knil(hh) = qq;
	return hh;
}


// 336
void y_reflect_edges()
{
	pointer p,q,r; //{list manipulation registers}
	p = n_min(cur_edges); n_min(cur_edges) = zero_field + zero_field - 1 - n_max(cur_edges);
	n_max(cur_edges) = zero_field + zero_field - 1 - p;
	n_pos(cur_edges) = zero_field + zero_field - 1 - n_pos(cur_edges);
	p = link(cur_edges); q = cur_edges; //{we assume that p != q }
	do { 
		r = link(p); link(p) = q; knil(q) = p; q = p; p = r;
	} while (!( q == cur_edges));
	last_window_time(cur_edges) = 0;
}


// 337
void x_reflect_edges()
{
	pointer p,q,r,s; //{list manipulation registers}
	int m; //{info_fields will be reflected with respect to this number}

	p = m_min(cur_edges); m_min(cur_edges) = zero_field + zero_field - m_max(cur_edges);
	m_max(cur_edges) = zero_field + zero_field - p;
	m = (zero_field + m_offset(cur_edges)) * 8 + zero_w + min_halfword + zero_w + min_halfword;
	m_offset(cur_edges) = zero_field; p = link(cur_edges);
	do { 
		#pragma region <Reflect the edge-and-weight data in sorted(p) 339>
		q = sorted(p); r = sentinel;
		while (q != sentinel)
		{ s = link(q); link(q) = r; r = q; info(r) = m - info(q); q = s;
		}
		sorted(p) = r;
		#pragma endregion

		#pragma region <Reflect the edge-and-weight data in unsorted(p) 338>
		q = unsorted(p);
		while (q > _void)
		{ 
			info(q) = m - info(q); q = link(q);
		}
		#pragma endregion
		p = link(p);
	} while (!( p == cur_edges));
	last_window_time(cur_edges) = 0;
}

// 340
void y_scale_edges(int s)
{
	pointer p,q,pp,r,rr,ss; //{list manipulation registers}
	int t; //{replication counter}
	if (s*(n_max(cur_edges)+1-zero_field) >= 4096 || s*(n_min(cur_edges)-zero_field) <= -4096)

	{ 
		print_err(/*Scaled picture would be too big*/464);
		help3(/*I can't yscale the picture as requested---it would*/465,
			/*make some coordinates too large or too small.*/466,
			/*Proceed, and I'll omit the transformation.*/467); put_get_error();
	}
	else { 
		n_max(cur_edges) = s * (n_max(cur_edges) + 1 - zero_field) - 1 + zero_field;
		n_min(cur_edges) = s * (n_min(cur_edges) - zero_field) + zero_field;
		#pragma region <Replicate every row exactly s times 341>
		p = cur_edges;
		do { 
			q = p; p = link(p);
			for (t = 2; t <= s; t++)
			{
				pp = get_node(row_node_size); 
				link(q) = pp; knil(p) = pp; link(pp) = p; knil(pp) = q;
				q = pp; 
				#pragma region <Copy both sorted and unsorted lists of p to pp 335>
				r = sorted(p); rr = sorted_loc(pp); //{link(rr) = sorted(pp)}
				while (r != sentinel)
				{ 
					ss = get_avail(); link(rr) = ss; rr = ss; info(rr) = info(r);
					r = link(r);
				}
				link(rr) = sentinel;
				r = unsorted(p); rr = temp_head;
				while (r > _void)
				{ 
					ss = get_avail(); link(rr) = ss; rr = ss; info(rr) = info(r);
					r = link(r);
				}
				link(rr) = r; unsorted(pp) = link(temp_head);
				#pragma endregion
			}
		} while (!( link(p) == cur_edges));
		#pragma endregion
		last_window_time(cur_edges) = 0;
	}
}

// 342
void x_scale_edges(int s)
{
	pointer p,q; //{list manipulation registers}
	int t; //0 .. 65535; {unpacked info field}
	int w; //0 .. 7; {unpacked weight}
	int delta; //{amount added to scaled info }

	if (s * (m_max(cur_edges) - zero_field) >= 4096 || s * (m_min(cur_edges) - zero_field) <= -4096)
	{ 
		print_err(/*Scaled picture would be too big*/464);
		help3(/*I can't xscale the picture as requested---it would*/468,
			/*make some coordinates too large or too small.*/466,
			/*Proceed, and I'll omit the transformation.*/467); put_get_error();
	}
	else if (m_max(cur_edges) != zero_field || m_min(cur_edges) != zero_field)
	{ 
		m_max(cur_edges) = s * (m_max(cur_edges) - zero_field) + zero_field;
		m_min(cur_edges) = s * (m_min(cur_edges) - zero_field) + zero_field;
		delta = 8 * (zero_field - s * m_offset(cur_edges)) + min_halfword; m_offset(cur_edges) = zero_field;
		#pragma region <Scale the x coordinates of each row by s 343>
		q = link(cur_edges);
		do { 
			p = sorted(q);
			while (p != sentinel)
			{ 
				t = ho(info(p)); w = t % 8; info(p) = (t - w) * s + w + delta; p = link(p);
			}
			p = unsorted(q);
			while (p > _void)
			{ 
				t = ho(info(p)); w = t % 8; info(p) = (t - w) * s + w + delta; p = link(p);
			}
			q = link(q);
		} while (!( q == cur_edges));
		#pragma endregion
		last_window_time(cur_edges) = 0;
	}
}


// 344
void negate_edges(pointer h)
{
	pointer p,q,r,s,t,u; //{structure traversers}
	p = link(h);
	while (p != h)
	{ 
		q = unsorted(p);
		while (q > _void)
		{ 
			info(q) = 8 - 2 * ((ho(info(q))) % 8) + info(q); q = link(q);
		}
		q = sorted(p);
		if (q != sentinel)
		{ 
			do { 
				info(q) = 8 - 2 * ((ho(info(q))) % 8) + info(q); q = link(q);
			} while (!(q == sentinel));
			#pragma region <Put the list |sorted(p)| back into sort>
			u = sorted_loc(p); q = link(u); r = q; s = link(r); //{|q=sorted(p)|}
			while (true) 
				if (info(s) > info(r))
				{ 
					link(u) = q;
					if (s == sentinel) goto done;
					u = r; q = s; r = q; s = link(r);
				}
				else {
					t = s; s = link(t); link(t) = q; q = t;
				}
		done:
			link(r) = sentinel;
			#pragma endregion
		}
		p = link(p);
	}
	last_window_time(h) = 0;
}


// 346
void sort_edges(pointer h) //{h is a row header}
{
	halfword k; //{key register that we compare to info(q)}
	pointer p,q,r,s;

	r = unsorted(h); unsorted(h) = null; p = link(r); link(r) = sentinel; link(temp_head) = r;
	while (p > _void) // {sort node p into the list that starts at temp_head }
	{ 
		k = info(p); q = temp_head;
		do { 
			r = q; q = link(r);
		} while (!(k <= info(q)));
		link(r) = p; r = link(p); link(p) = q; p = r;
	}
	#pragma region <Merge the temp head list into sorted(h) 347>
	{ 
		r = sorted_loc(h); q = link(r); p = link(temp_head);
		while (true) { 
			k = info(p);
			while (k > info(q))
			{ 
				r = q; q = link(r);
			}
			link(r) = p; s = link(p); link(p) = q;
			if (s == sentinel) goto done;
			r = p; p = s;
		}
	done:
		;
	}
	#pragma endregion
}

// 348
void cull_edges(int w_lo,int w_hi,int w_out,int w_in)
{
	pointer p, q, r, s; //{for list manipulation}
	int w = INT_MAX; //{new weight after culling}
	int d; //{data register for unpacking}
	int m; //{the previous column number, including |m_offset|}
	int mm; //{the next column number, including |m_offset|}
	int ww; //{accumulated weight before culling}
	int prev_w; //{value of |w| before column |m|}
	pointer n,min_n,max_n; //{current and extreme row numbers}
	pointer min_d,max_d; //{extremes of the new edge-and-weight data}
	
	min_d = max_halfword; max_d = min_halfword;
	min_n = max_halfword; max_n = min_halfword;
	p = link(cur_edges); n = n_min(cur_edges);
	while (p != cur_edges)
	{ 
		if (unsorted(p) > _void) sort_edges(p);
		if (sorted(p) != sentinel)
		#pragma region <Cull superfluous edge-weight entries from |sorted(p)|>
		{
			r = temp_head; q = sorted(p); ww = 0; m = 1000000; prev_w = 0;
			while (true)  { 
				if (q == sentinel) mm = 1000000;
				else { 
					d = ho(info(q)); mm = d / 8; ww = ww + (d % 8) - zero_w;
				}
				if (mm > m)
				{
					#pragma region <Insert an edge-weight for edge |m|, if the new pixel weight has changed>
					if (w != prev_w)
					{ 
						s = get_avail(); link(r) = s;
						info(s) = 8 * m + min_halfword + zero_w + w - prev_w;
						r = s; prev_w = w;
					}			
					#pragma endregion
					if (q == sentinel) goto done;
				}
				m = mm;
				if (ww >= w_lo) 
					if (ww <= w_hi) w = w_in;
					else w = w_out;
				else w = w_out;
				s = link(q); free_avail(q); q = s;
			}
		done:
			link(r) = sentinel; sorted(p) = link(temp_head);
			if (r != temp_head) 
				#pragma region <Update the max/min amounts>
			{ 
				if (min_n == max_halfword) min_n = n;
				max_n = n;
				if (min_d > info(link(temp_head))) min_d = info(link(temp_head));
				if (max_d < info(r)) max_d = info(r);
			}	
				#pragma endregion
		}
		#pragma endregion
		p = link(p); n++;
	}
	#pragma region <Delete empty rows at the top and/or bottom; update the boundary values in the header>
	if (min_n > max_n) 
		#pragma region <Delete all the row headers>
	{ 
		p = link(cur_edges);
		while (p != cur_edges)
		{ 
			q = link(p); free_node(p,row_node_size); p = q;
		}
		init_edges(cur_edges);
	}
		#pragma endregion
	else  { 
		n = n_min(cur_edges); n_min(cur_edges) = min_n;
		while (min_n > n)
		{ 
			p = link(cur_edges); link(cur_edges) = link(p);
			knil(link(p)) = cur_edges;
			free_node(p,row_node_size); n++;
		}
		n = n_max(cur_edges); n_max(cur_edges) = max_n;
		n_pos(cur_edges) = max_n + 1; n_rover(cur_edges) = cur_edges;
		while (max_n < n)
		{ 
			p = knil(cur_edges); knil(cur_edges) = knil(p);
			link(knil(p)) = cur_edges;
			free_node(p,row_node_size); n--;
		}
		m_min(cur_edges) = ((ho(min_d)) / 8) - m_offset(cur_edges) + zero_field;
		m_max(cur_edges) = ((ho(max_d)) / 8) - m_offset(cur_edges) + zero_field;
	}
	#pragma endregion
	last_window_time(cur_edges) = 0;
}



// 354
void xy_swap_edges() //{interchange |x| and |y| in |cur_edges|}
{
	int m_magic,n_magic; //{special values that account for offsets}
	pointer p, q, r, s; //{pointers that traverse the given structure}
	#pragma region <Other local variables for |xy_swap_edges|>
	int m_spread; //{the difference between |m_max| and |m_min|}
	int j,jj; //:0..move_size; //{indices into |move|}
	int m = INT_MAX, mm = INT_MAX; //{|m| values at vertical edges}
	int pd, rd; //{data fields from edge-and-weight nodes}
	int pm, rm; //{|m| values from edge-and-weight nodes}
	int w; //{the difference in accumulated weight}
	int ww; //{as much of |w| that can be stored in a single node}
	int dw; //{an increment to be added to |w|}

	int extras; //{the number of additional nodes to make weights |>3|}
	int xw = INT_MAX; //:-3..3; //{the additional weight in extra nodes}
	int k; //{loop counter for inserting extra nodes}
	#pragma endregion

	#pragma region <Initialize the array of new edge list heads>
	m_spread = m_max(cur_edges) - m_min(cur_edges); //{this is |>=0| by assumption}
	if (m_spread > move_size) overflow(/*move table size*/469,move_size);
	for (j = 0; j<= m_spread; j++) move[j] = sentinel;
	#pragma endregion

	#pragma region<Insert blank rows at the top and bottom, and set |p| to the new top row>
	p = get_node(row_node_size); 
	sorted(p) = sentinel; unsorted(p) = null;
	knil(p) = cur_edges; knil(link(cur_edges)) = p; //{the new bottom row}
	p = get_node(row_node_size); 
	sorted(p) = sentinel;
	knil(p) = knil(cur_edges); //{the new top row}
	#pragma endregion

	#pragma region <Compute the magic offset values>
	m_magic = m_min(cur_edges) + m_offset(cur_edges) - zero_field;
	n_magic = 8 * n_max(cur_edges) + 8 + zero_w + min_halfword;
	#pragma endregion

	do { 
		q = knil(p);
		if (unsorted(q) > _void) sort_edges(q);
		#pragma region <Insert the horizontal edges defined by adjacent rows |p,q|, and destroy row~|p|>
		r = sorted(p); free_node(p,row_node_size); p = r;
		pd = ho(info(p)); pm = pd / 8;
		r = sorted(q); rd = ho(info(r)); rm = rd / 8; w = 0;
		while(true) { 
			if (pm < rm) mm = pm; else mm = rm;
			if (w != 0)
				#pragma region <Insert horizontal edges of weight |w| between |m| and~|mm|>
				if (m != mm)
				{ 
					if (mm - m_magic >= move_size) confusion(/*xy*/436);
					extras = (myabs(w) - 1) / 3;
					if (extras > 0)
					{ 
						if (w > 0) xw = +3; else xw = -3;
						ww = w - extras * xw;
					}
					else ww = w;
					do { 
						j = m - m_magic;
						for (k = 1; k <= extras; k++)
						{ 
							s = get_avail(); info(s) = n_magic + xw;
							link(s) = move[j]; move[j] = s;
						}
						s = get_avail(); info(s) = n_magic + ww;
						link(s) = move[j]; move[j] = s;
						m++;
					} while (!(m == mm));
				}
				#pragma endregion
			if (pd < rd)
			{ 
				dw = (pd % 8) - zero_w;
				#pragma region <Advance pointer |p| to the next vertical edge,after destroying the previous one>
				s = link(p); free_avail(p); p = s; pd = ho(info(p)); pm = pd / 8;
				#pragma endregion
			}
			else { 
				if (r == sentinel) goto done; //{|rd=pd=ho(max_halfword)|}
				dw = -((rd % 8) - zero_w);
				#pragma region <Advance pointer |r| to the next vertical edge>
				r = link(r); rd = ho(info(r)); rm = rd / 8;
				#pragma endregion
			}
			m = mm; w = w + dw;
		}
	done:	
		#pragma endregion
		p = q; n_magic = n_magic - 8;
	} while(!(knil(p) == cur_edges));
	free_node(p,row_node_size); //{now all original rows have been recycled}
	#pragma region <Adjust the header to reflect the new edges>
	move[m_spread] = 0; j = 0;
	while (move[j] == sentinel) j++;
	if (j == m_spread) init_edges(cur_edges); //{all edge weights are zero}
	else { 
		mm = m_min(cur_edges);
		m_min(cur_edges) = n_min(cur_edges);
		m_max(cur_edges) = n_max(cur_edges) + 1;
		m_offset(cur_edges) = zero_field;
		jj = m_spread - 1;
		while (move[jj] == sentinel) jj--;
		n_min(cur_edges) = j + mm; n_max(cur_edges) = jj + mm; q = cur_edges;
		do { 
			p = get_node(row_node_size); link(q) = p; knil(p) = q;
			sorted(p) = move[j]; unsorted(p) = null; j++; q = p;
		} while(!(j > jj));
		link(q) = cur_edges; knil(cur_edges) = q;
		n_pos(cur_edges) = n_max(cur_edges) + 1; n_rover(cur_edges) = cur_edges;
		last_window_time(cur_edges) = 0;
	}
	#pragma endregion
}



// 366
void merge_edges(pointer h)
{
	pointer p, q, r, pp, qq, rr; //{list manipulation registers}
	int n; //{row number}
	halfword k; //{key register that we compare to |info(q)|}
	int delta; //{change to the edge/weight data}
	if (link(h) != h)
	{ 
		if (m_min(h) < m_min(cur_edges) || m_max(h) > m_max(cur_edges) || n_min(h) < n_min(cur_edges) || n_max(h) > n_max(cur_edges))
			edge_prep(m_min(h) - zero_field,m_max(h) - zero_field, n_min(h) - zero_field,n_max(h) - zero_field + 1);
		if (m_offset(h) != m_offset(cur_edges))
			#pragma region <Adjust the data of |h| to account for a difference of offsets>
		{ 
			pp = link(h); delta = 8 * (m_offset(cur_edges) - m_offset(h));
			do { qq = sorted(pp);
			while (qq != sentinel)
			{ 
				info(qq) = info(qq) + delta; qq = link(qq);
			}
			qq = unsorted(pp);
			while (qq > _void)
			{ 
				info(qq) = info(qq) + delta; qq = link(qq);
			}
			pp = link(pp);
			} while (!(pp == h));
		}		
			#pragma endregion
		n = n_min(cur_edges); p = link(cur_edges); pp = link(h);
		while (n < n_min(h))
		{
			n++; p = link(p);
		}
		do {
			#pragma region <Merge row |pp| into row |p|>
			qq = unsorted(pp);
			if (qq > _void)
				if (unsorted(p) <= _void) unsorted(p) = qq;
				else { 
					while (link(qq) > _void) qq = link(qq);
					link(qq) = unsorted(p); unsorted(p) = unsorted(pp);
				}
			unsorted(pp) = null; qq = sorted(pp);
			if (qq != sentinel)
			{ 
				if (unsorted(p) == _void) unsorted(p) = null;
				sorted(pp) = sentinel; r = sorted_loc(p); q = link(r); //{|q=sorted(p)|}
				if (q == sentinel) sorted(p) = qq;
				else while (true) {
					k = info(qq);
					while (k > info(q))
					{ 
						r = q; q = link(r);
					}
					link(r) = qq; rr = link(qq); link(qq) = q;
					if (rr == sentinel) goto done;
					r = qq; qq = rr;
				}
			}
		done:			
			#pragma endregion
			pp = link(pp); p = link(p);
		} while (!(pp == h));
	}
}



// 369
int total_weight(pointer h) //{|h| is an edge header}
{
	pointer p, q; //{variables that traverse the given structure}
	int n; //{accumulated total so far}
	int m; //:0..65535; {packed $x$ and $w$ values, including offsets}
	n = 0; p = link(h);
	while (p != h)
	{ 
		q = sorted(p);
		while (q != sentinel)
			#pragma region <Add the contribution of node |q| to the total weight, and set |q:=link(q)|>
		{ 
			m = ho(info(q)); n = n - ((m % 8) - zero_w) * (m / 8);
			q = link(q);
		}		
			#pragma endregion

		q = unsorted(p);

		while (q > _void)
			#pragma region <Add the contribution of node |q| to the total weight, and set |q:=link(q)|>
		{ 
			m = ho(info(q)); n = n - ((m % 8) - zero_w) * (m / 8);
			q = link(q);
		}		
			#pragma endregion
		p = link(p);
	}
	return n;
}

// 372

void begin_edge_tracing()
{
	print_diagnostic(/*Tracing edges*/470,/**/289,true); print(/* (weight */471); print_int(cur_wt);
	print_char(/*)*/41); trace_x = -4096;
}

void trace_a_corner()
{
	if (file_offset > max_print_line - 13) print_nl(/**/289);
	print_char(/*(*/40); print_int(trace_x); print_char(/*,*/44); print_int(trace_yy); print_char(/*)*/41);
	trace_y = trace_yy;
}

void end_edge_tracing()
{
	if (trace_x == -4096) print_nl(/*(No new edges added.)*/472);
	else { 
		trace_a_corner(); print_char(/*.*/46);
	}
	end_diagnostic(true);
}

// 373
void trace_new_edge(pointer r, int n)
{
	int d; //{temporary data register}
	int w; //:-3..3; //{weight associated with an edge transition}
	int m, n0, n1; //{column and row numbers}
	d = ho(info(r)); w = (d % 8) - zero_w; m = (d / 8) - m_offset(cur_edges);
	if (w == cur_wt)
	{
		n0 = n + 1; n1 = n;
	}
	else {
		n0 = n; n1 = n + 1;
	} //{the edges run from |(m,n0)| to |(m,n1)|}
	if (m != trace_x)
	{ 
		if (trace_x == -4096)
		{
			print_nl(/**/289); trace_yy = n0;
		}
		else if (trace_yy != n0) print_char(/*?*/63); //{shouldn't happen}
		else trace_a_corner();
		trace_x = m; trace_a_corner();
	}
	else {
		if (n0 != trace_yy) print_char(/*!*/33); //{shouldn't happen}
		if (((n0 < n1) && (trace_y > trace_yy)) || ((n0 > n1) && (trace_y < trace_yy)))
			trace_a_corner();
	}
	trace_yy = n1;
}



// 374
void line_edges(scaled x0, scaled y0, scaled x1, scaled y1)
{
	int m0, n0, m1, n1; //{rounded and unscaled coordinates}
	scaled delx, dely; //{the coordinate differences of the line}
	scaled yt; //{smallest |y| coordinate that rounds the same as |y0|}
	scaled tx; //{tentative change in |x|}
	pointer p, r; //{list manipulation registers}
	int base; //{amount added to edge-and-weight data}
	int n; //{current row number}
	n0 = round_unscaled(y0);
	n1 = round_unscaled(y1);
	if (n0 != n1)
	{
		m0 = round_unscaled(x0); m1 = round_unscaled(x1);
		delx = x1 - x0; dely = y1 - y0;
		yt = n0 * unity - half_unit; y0 = y0 - yt; y1 = y1 - yt;
		if (n0 < n1) 
			#pragma region <Insert upward edges for a line>
		{
			base = 8 * m_offset(cur_edges) + min_halfword + zero_w - cur_wt;
			if (m0 <= m1) edge_prep(m0,m1,n0,n1); else edge_prep(m1,m0,n0,n1);
			#pragma region <Move to row |n0|, pointed to by |p|>
			n = n_pos(cur_edges) - zero_field; p = n_rover(cur_edges);
			if (n != n0)
				if (n < n0)
					do { 
						n++; p = link(p);
					} while (!(n == n0));
				else 
					do {
						n--; p = knil(p);
					} while(!(n == n0));	
			#pragma endregion
			y0 = unity - y0;
			while (true) { 
				r = get_avail(); link(r) = unsorted(p); unsorted(p) = r;
				tx = take_fraction(delx,make_fraction(y0,dely));
				if (ab_vs_cd(delx,y0,dely,tx) < 0) tx--;
				//{now $|tx|=\lfloor|y0|\cdot|delx|/|dely|\rfloor$}
				info(r) = 8 * round_unscaled(x0 + tx) + base;
				y1 = y1 - unity;
				if (internal[tracing_edges] > 0) trace_new_edge(r,n);
				if (y1 < unity) goto done;
				p = link(p); y0 = y0 + unity; n++;
			}
		done:
			;
		}
			#pragma endregion
		else 
			#pragma region <Insert downward edges for a line>
		{ 
			base = 8 * m_offset(cur_edges) + min_halfword + zero_w + cur_wt;
			if (m0 <= m1) edge_prep(m0,m1,n1,n0); else edge_prep(m1,m0,n1,n0);
			n0--; 
			#pragma region <Move to row |n0|, pointed to by |p|>
			n = n_pos(cur_edges) - zero_field; p = n_rover(cur_edges);
			if (n != n0)
				if (n < n0)
					do {
						n++; p = link(p);
					} while (!(n == n0));
				else 
					do { 
						n--; p = knil(p);
					} while (!(n == n0));			
			#pragma endregion
			while (true) {
				r = get_avail(); link(r) = unsorted(p); unsorted(p) = r;
				tx = take_fraction(delx,make_fraction(y0,dely));
				if (ab_vs_cd(delx,y0,dely,tx) < 0) tx++;
				//{now $|tx|=\lceil|y0|\cdot|delx|/|dely|\rceil$, since |dely<0|}
				info(r) = 8 * round_unscaled(x0 - tx) + base;
				y1 = y1 + unity;
				if (internal[tracing_edges] > 0) trace_new_edge(r,n);
				if (y1 >= 0) goto done1;
				p = knil(p); y0 = y0 + unity; n--;
			}
		done1:
			;
		}
			#pragma endregion
		n_rover(cur_edges) = p; n_pos(cur_edges) = n + zero_field;
	}
}

// 378
void move_to_edges(int m0, int n0, int m1, int n1)
{
	int delta;//:0..move_size; //{extent of |move| data}
	int k;//:0..move_size; //{index into |move|}
	pointer p, r; //{list manipulation registers}
	int dx = INT_MAX; //{change in edge-weight |info| when |x| changes by 1}
	int edge_and_weight; //{|info| to insert}
	int j; //{number of consecutive vertical moves}
	int n; //{the current row pointed to by |p|}
	#ifndef NO_DEBUG
	int sum;
	#endif
	delta = n1 - n0;
	#ifndef NO_DEBUG
	sum = move[0]; for (k = 1; k <= delta; k++) sum = sum + myabs(move[k]);
	if (sum != m1 - m0) confusion(/*0*/48);
	#endif
	#pragma region <Prepare for and switch to the appropriate case, based on |octant|>
	switch(octant) {
	case first_octant:
		dx = 8; edge_prep(m0,m1,n0,n1); goto fast_case_up;
		break;
	case second_octant:
		dx = 8; edge_prep(n0,n1,m0,m1); goto slow_case_up;
		break;
	case third_octant:
		dx = -8; edge_prep(-n1,-n0,m0,m1); negate(n0);
		goto slow_case_up;
		break;
	case fourth_octant:
		dx = -8; edge_prep(-m1,-m0,n0,n1); negate(m0);
		goto fast_case_up;
		break;
	case fifth_octant:
		dx = -8; edge_prep(-m1,-m0,-n1,-n0); negate(m0);
		goto fast_case_down;
		break;
	case sixth_octant:
		dx = -8; edge_prep(-n1,-n0,-m1,-m0); negate(n0);
		goto slow_case_down;
		break;
	case seventh_octant:
		dx = 8; edge_prep(n0,n1,-m1,-m0); goto slow_case_down;
		break;
	case eighth_octant:
		dx = 8; edge_prep(m0,m1,-n1,-n0); goto fast_case_down;
		break;
	} //{there are only eight octants}

	#pragma endregion
fast_case_up:
	#pragma region <Add edges for first or fourth octants, then |goto done|>
	
	#pragma region <Move to row |n0|, pointed to by |p|>
	n = n_pos(cur_edges) - zero_field; p = n_rover(cur_edges);
	if (n != n0)
		if (n < n0)
			do { 
				n++; p = link(p);
			} while (!(n == n0));
		else 
			do {
				n--; p = knil(p);
			} while(!(n == n0));	
	#pragma endregion
	if (delta > 0)
	{ 
		k = 0;
		edge_and_weight = 8 * (m0 + m_offset(cur_edges)) + min_halfword + zero_w - cur_wt;
		do { 
			edge_and_weight = edge_and_weight + dx * move[k];
			fast_get_avail(r); link(r) = unsorted(p); info(r) = edge_and_weight;
			if (internal[tracing_edges] > 0) trace_new_edge(r,n);
			unsorted(p) = r; p = link(p); k++; n++;
		} while (!(k == delta));
	}
	goto done;
	#pragma endregion
fast_case_down:
	#pragma region <Add edges for fifth or eighth octants, then |goto done|>
	n0 = -n0 - 1; 
	#pragma region <Move to row |n0|, pointed to by |p|>
	n = n_pos(cur_edges) - zero_field; p = n_rover(cur_edges);
	if (n != n0)
		if (n < n0) 
			do { 
				n++; p = link(p);
			} while (!(n == n0));
		else 
			do {
				n--; p = knil(p);
			} while(!(n == n0));	
	#pragma endregion
	if (delta > 0)
	{ 
		k = 0;
		edge_and_weight = 8 * (m0 + m_offset(cur_edges)) + min_halfword + zero_w + cur_wt;
		do { 
			edge_and_weight = edge_and_weight + dx * move[k];
			fast_get_avail(r); link(r) = unsorted(p); info(r) = edge_and_weight;
			if (internal[tracing_edges] > 0) trace_new_edge(r,n);
			unsorted(p) = r; p = knil(p); k++; n--;
		} while (!(k == delta));
	}
	goto done;
	#pragma endregion
slow_case_up:
	#pragma region <Add edges for second or third octants, then |goto done|>
	edge_and_weight = 8 * (n0 + m_offset(cur_edges)) + min_halfword + zero_w - cur_wt;
	n0 = m0; k = 0; 
	#pragma region <Move to row |n0|, pointed to by |p|>
	n = n_pos(cur_edges) - zero_field; p = n_rover(cur_edges);
	if (n != n0)
		if (n < n0) 
			do { 
				n++; p = link(p);
			} while (!(n == n0));
		else 
			do {
				n--; p = knil(p);
			} while(!(n == n0));
	#pragma endregion	
	do { 
		j = move[k];
		while (j > 0)
		{
			fast_get_avail(r); link(r) = unsorted(p); info(r) = edge_and_weight;
			if (internal[tracing_edges] > 0) trace_new_edge(r,n);
			unsorted(p) = r; p = link(p); j--; n++;
		}
		edge_and_weight = edge_and_weight + dx; k++;
	} while (!(k > delta));
	goto done;
	#pragma endregion
slow_case_down:
	#pragma region <Add edges for sixth or seventh octants, then |goto done|>
	edge_and_weight = 8 * (n0 + m_offset(cur_edges)) + min_halfword + zero_w + cur_wt;
	n0 = -m0 - 1; k = 0;
	#pragma region <Move to row |n0|, pointed to by |p|>
	n = n_pos(cur_edges) - zero_field; p = n_rover(cur_edges);
	if (n != n0)
		if (n < n0) 
			do { 
				n++; p = link(p);
			} while (!(n == n0));
		else
			do {
				n--; p = knil(p);
			} while(!(n == n0));
	#pragma endregion	
	do { j = move[k];
	while (j > 0)
	{ 
		fast_get_avail(r); link(r) = unsorted(p); info(r) = edge_and_weight;
		if (internal[tracing_edges] > 0) trace_new_edge(r,n);
		unsorted(p) = r; p = knil(p); j--; n--;
	}
	edge_and_weight = edge_and_weight + dx; k++;
	} while (!(k > delta));
	goto done;
	#pragma endregion
done:
	n_pos(cur_edges) = n + zero_field; n_rover(cur_edges) = p;
}




// 385
void toss_edges(pointer h)
{
	pointer p, q; //{for list manipulation}
	q = link(h);
	while (q != h)
	{ 
		flush_list(sorted(q));
		if (unsorted(q) > _void) flush_list(unsorted(q));
		p = q; q = link(q); free_node(p,row_node_size);
	}
	free_node(h,edge_header_size);
}



// 388
void unskew(scaled x, scaled y, small_number octant)
{
	switch(octant) {
		case first_octant:
			set_two(x + y, y);
			break;
		case second_octant:
			set_two(y, x + y);
			break;
		case third_octant:
			set_two(-y, x + y);
			break;
		case fourth_octant:
			set_two(-x - y, y);
			break;
		case fifth_octant:
			set_two(-x - y, -y);
			break;
		case sixth_octant:
			set_two(-y, -x - y);
			break;
		case seventh_octant:
			set_two(y, -x - y);
			break;
		case eighth_octant:
			set_two(x + y, -y);
			break;
	}
}




// 387
void skew(scaled x, scaled y, small_number octant)
{
	switch(octant) {
		case first_octant: set_two(x - y, y); break;
		case second_octant: set_two(y - x, x); break;
		case third_octant: set_two(y + x, -x); break;
		case fourth_octant: set_two(-x - y, y); break;
		case fifth_octant: set_two(-x + y, -y); break;
		case sixth_octant: set_two(-y + x, -x); break;
		case seventh_octant: set_two(-y - x, x); break;
		case eighth_octant: set_two(x + y, -y); break;
	}
}

// 390
void abnegate(scaled x, scaled y, small_number octant_before, small_number octant_after)
{
	if (myodd(octant_before) == myodd(octant_after)) cur_x = x;
	else cur_x = -x;
	if (octant_before > negate_y == octant_after > negate_y) cur_y = y;
	else cur_y = -y;
}

// 391
fraction crossing_point(int a, int b, int c)
{
	int d; //{recursive counter}
	int x,xx,x0,x1,x2; //{temporary registers for bisection}
	if (a < 0) zero_crossing;
	if (c >= 0)
	{ 
		if (b >= 0)
			if (c > 0) no_crossing;
			else if (a == 0 && b == 0) no_crossing;
			else one_crossing;
		if (a == 0) zero_crossing;
	}
	else if (a == 0)
		if (b <= 0) zero_crossing;
	#pragma region <Use bisection to find the crossing point, if one exists>
	d = 1; x0 = a; x1 = a - b; x2 = b - c;
	do { 
		x = half(x1 + x2);
		if (x1 - x0 > x0)
		{ x2 = x; _double(x0); _double(d);
		}
		else { 
			xx = x1 + x - x0;
			if (xx > x0)
			{ 
				x2 = x; _double(x0); _double(d);
			}
			else { 
				x0 = x0 - xx;
				if (x <= x0) if (x + x2 <= x0) no_crossing;
				x1 = x; d = d + d + 1;
			}
		}
	} while (!(d >= fraction_one));
	return d - fraction_one;

	#pragma endregion
}


// 394
void  print_spec(str_number s)
{
	pointer p,q; // {for list traversal}
	small_number octant; // {the current octant code}
	print_diagnostic(/*Cycle spec*/473,s,true); p = cur_spec; octant = left_octant(p); print_ln();
	print_two_true(x_coord(cur_spec),y_coord(cur_spec)); print(/* % beginning in octant `*/474);
	while (true) { 
		print(octant_dir[octant]); print_char(/*'*/39);
		while (true) { 
			q = link(p);
			if (right_type(p) == endpoint) goto not_found;

			#pragma region <Print the cubic between p and q 397>
			print_nl(/*   ..controls */475); print_two_true(right_x(p), right_y(p)); print(/* and */448);
			print_two_true(left_x(q), left_y(q)); print_nl(/* ..*/454); print_two_true(x_coord(q), y_coord(q));
			print(/* % segment */476); print_int(left_type(q) - 1);
			#pragma endregion

			p = q;
		}
	not_found: 
		if (q == cur_spec) goto done;
		p = q; octant = left_octant(p); print_nl(/*% entering octant `*/477);
	}
done: 
	print_nl(/* & cycle*/478); end_diagnostic(true);
}

// 398
void print_strange(str_number s)
{
	pointer p; //{for list traversal}
	pointer f = max_halfword; //{starting point in the cycle}
	pointer q; //{octant_boundary to be printed}
	int t; //{segment number, plus 1}
	if (interaction == error_stop_mode) wake_up_terminal();
	print_nl(/*>*/62);
	
	
	#pragma region <Find the starting point, f 399>
	
	p = cur_spec; t = max_quarterword + 1;
	do { 
		p = link(p);
		if (left_type(p) != endpoint)
		{ 
			if (left_type(p) < t) f = p;
			t = left_type(p);
		}
	} while (!( p == cur_spec));	
	
	#pragma endregion

	#pragma region <Determine the octant_boundary q that precedes f 400>
	p = cur_spec; q = p;
	do {
		p = link(p);
		if (left_type(p) == endpoint) q = p;
	} while (!( p == f));
	#pragma endregion
	t = 0;
	do {
		if (left_type(p) != endpoint)
		{ 
			if (left_type(p) != t)
			{ 
				t = left_type(p); print_char(/* */32); print_int(t - 1);
			}
			if (q != null)
			{ 
				#pragma region <print_the turns, if any, that start at q, and advance q 401>
				if (left_type(link(q)) == endpoint)
				{ 
					print(/* (*/479); print(octant_dir[left_octant(q)]); q = link(q);
					while (left_type(link(q)) == endpoint)
					{ 
						print_char(/* */32); print(octant_dir[left_octant(q)]); q = link(q);
					}
					print_char(/*)*/41);
				}				
				#pragma endregion
				print_char(/* */32); print(octant_dir[left_octant(q)]); q = null;
			}
		}
		else if (q == null) q = p;
		p = link(p);
	} while (!( p == f));
	print_char(/* */32); print_int(left_type(p) - 1);
	if (q != null)
		#pragma region <print_the turns, if any, that start at q, and advance q 401>
		if (left_type(link(q)) == endpoint)
		{ 
			print(/* (*/479); print(octant_dir[left_octant(q)]); q = link(q);
			while (left_type(link(q)) == endpoint)
			{ 
				print_char(/* */32); print(octant_dir[left_octant(q)]); q = link(q);
			}
			print_char(/*)*/41);
		}
		#pragma endregion
	print_err(s);
}



// 402
pointer make_spec(pointer h,scaled safety_margin,int tracing)//{converts a path to a cycle spec}
{
	pointer p,q,r,s; //{for traversing the lists}
	int k; //{serial number of path segment, or octant code}
	int chopped; //{positive if data truncated, negative if data dangerously large}
	#pragma region <Other local variables for |make_spec|>
	small_number o1, o2; //{octant numbers}
	bool clockwise = false; //{should we turn clockwise?}
	int dx1, dy1, dx2, dy2; //{directions of travel at a cusp}
	int dmax, del; //{temporary registers}
	#pragma endregion

	cur_spec = h;
	if (tracing > 0)
		print_path(cur_spec,/*, before subdivision into octants*/480,true);
	max_allowed = fraction_one -half_unit - 1 - safety_margin;
	#pragma region <Truncate the values of all coordinates that exceed |max_allowed|, and stamp segment numbers in each |left_type| field>
	p = cur_spec; k = 1; chopped = 0; dmax = half(max_allowed);
	do { 
		procrustes(left_x(p)); procrustes(left_y(p));
		procrustes(x_coord(p)); procrustes(y_coord(p));
		procrustes(right_x(p)); procrustes(right_y(p));
		p = link(p); left_type(p) = k;
		if (k < max_quarterword) k++; else k = 1;
	} while (!(p == cur_spec));
	if (chopped > 0)
	{ 
		print_err(/*Curve out of range*/481);
		help4(/*At least one of the coordinates in the path I'm about to*/482,
			/*digitize was really huge (potentially bigger than 4095).*/483,
			/*So I've cut it back to the maximum size.*/484,
			/*The results will probably be pretty wild.*/485);
		put_get_error();
	}	
	#pragma endregion
	quadrant_subdivide(); //{subdivide each cubic into pieces belonging to quadrants}
	if (internal[autorounding] > 0 && chopped == 0) xy_round();
	octant_subdivide(); //{complete the subdivision}
	if (internal[autorounding] > unity && chopped == 0) diag_round();
	#pragma region <Remove dead cubics>
	p = cur_spec;
	do {
mycontinue: 
		q = link(p);
		if (p != q)
		{ 
			if (x_coord(p) == right_x(p))
				if (y_coord(p) == right_y(p))
					if (x_coord(p) == left_x(q))
						if (y_coord(p) == left_y(q))
						{ 
							unskew(x_coord(q),y_coord(q),right_type(q));
							skew(cur_x,cur_y,right_type(p));
							if (x_coord(p) == cur_x) 
								if (y_coord(p) == cur_y)
								{ 
									remove_cubic(p); //{remove the cubic following |p|}
									if (q != cur_spec) goto mycontinue;
									cur_spec = p; q = p;
								}
						}
		}
		p = q;
	} while (!(p == cur_spec));
	#pragma endregion
	
	#pragma region <Insert octant boundaries and compute the turning number>
	turning_number = 0;
	p = cur_spec; q = link(p);
	do {
		r = link(q);
		if (right_type(p) != right_type(q) || q == r)
			#pragma region <Insert one or more octant boundary nodes just before~|q|>
		{ 
			new_boundary(p,right_type(p)); s = link(p);
			o1 = octant_number[right_type(p)]; o2 = octant_number[right_type(q)];
			switch(o2 - o1) {
				case 1: case -7: case 7: case -1: goto done;
				case 2: case -6: clockwise = false; break;
				case 3: case -5: case 4: case -4: case 5: case -3: 
					#pragma region <Decide whether or not to go clockwise>
					#pragma region <Compute the incoming and outgoing directions>
					dx1 = x_coord(s) - left_x(s); dy1 = y_coord(s)-left_y(s);
					if (dx1 == 0) 
						if (dy1 == 0)
						{ 
							dx1 = x_coord(s) - right_x(p); dy1 = y_coord(s) - right_y(p);
							if (dx1 == 0)
								if (dy1 == 0)
								{ 
									dx1 = x_coord(s) - x_coord(p); dy1 = y_coord(s)-y_coord(p);
								}  //{and they {\sl can't} both be zero}
						}
					dmax = myabs(dx1);
					if (myabs(dy1) > dmax) dmax = myabs(dy1);
					while (dmax < fraction_one)
					{ 
						_double(dmax); _double(dx1); _double(dy1);
					}
					dx2 = right_x(q) - x_coord(q); dy2 = right_y(q) - y_coord(q);
					if (dx2 == 0)
						if (dy2 == 0)
						{ 
							dx2 = left_x(r) - x_coord(q); dy2 = left_y(r) - y_coord(q);
							if (dx2 == 0)
								if (dy2 == 0)
								{ 
									if (right_type(r) == endpoint)
									{ 
										cur_x = x_coord(r); cur_y = y_coord(r);
									}
									else { 
										unskew(x_coord(r),y_coord(r),right_type(r));
										skew(cur_x,cur_y,right_type(q));
									}
									dx2 = cur_x - x_coord(q); dy2 = cur_y - y_coord(q);
								}  //{and they {\sl can't} both be zero}
						}
					dmax = myabs(dx2);
					if (myabs(dy2) > dmax) dmax = myabs(dy2);
					while (dmax < fraction_one)
					{
						_double(dmax); _double(dx2); _double(dy2);
					}
					#pragma endregion
					unskew(dx1,dy1,right_type(p)); del = pyth_add(cur_x,cur_y);
					dx1 = make_fraction(cur_x,del); dy1 = make_fraction(cur_y,del);
					//{$\cos\theta_1$ and $\sin\theta_1$}
					unskew(dx2,dy2,right_type(q)); del = pyth_add(cur_x,cur_y);
					dx2 = make_fraction(cur_x,del); dy2 = make_fraction(cur_y,del);
					//{$\cos\theta_2$ and $\sin\theta_2$}
					del = take_fraction(dx1,dy2) - take_fraction(dx2,dy1); //{$\sin(\theta_2-\theta_1)$}
					if (del > 4684844) clockwise = false;
					else if (del < -4684844) clockwise = true;
					//{$2^{28}\cdot\sin 1^\circ\approx4684844.68$}
					else clockwise = rev_turns;
					#pragma endregion
					break;
				case 6: case -2: clockwise = true; break;
				case 0: clockwise = rev_turns; break;
			} //{there are no other cases}
			#pragma region <Insert additional boundary nodes, then |goto done|>
			while (true) {
				if (clockwise)
					if (o1 == 1) o1 = 8; else o1--;
					else if (o1 == 8) o1 = 1; else o1++;
				if (o1 == o2) goto done;
				new_boundary(s,octant_code[o1]);
				s = link(s); left_octant(s) = right_octant(s);
			}
			#pragma endregion
		done: 
			if (q == r)
			{ 
				q = link(q); r = q; p = s; link(s) = q; left_octant(q) = right_octant(q);
				left_type(q) = endpoint; free_node(cur_spec,knot_node_size); cur_spec = q;
			}
			#pragma region <Fix up the transition fields and adjust the turning number>
			p = link(p);
			do { 
				s = link(p);
				o1 = octant_number[right_octant(p)]; o2 = octant_number[left_octant(s)];
				if (myabs(o1 - o2) == 1)
				{ 
					if (o2 < o1) o2 = o1;
					if (myodd(o2)) right_transition(p) = axis;
					else right_transition(p) = diagonal;
				}
				else { 
					if (o1 == 8) turning_number++; else turning_number--;
					right_transition(p) = axis;
				}
				left_transition(s) = right_transition(p);
				p = s;
			} while (!(p == q));
			#pragma endregion
		}
			#pragma endregion
		p = q; q = r;
	} while (!(p == cur_spec));

	#pragma endregion
	while (left_type(cur_spec) != endpoint) cur_spec = link(cur_spec);
	if (tracing > 0)
		if (internal[autorounding] <= 0 || chopped != 0)
			print_spec(/*, after subdivision*/486);
		else if (internal[autorounding] > unity)
			print_spec(/*, after subdivision and double autorounding*/487);
		else print_spec(/*, after subdivision and autorounding*/488);
	return cur_spec;
}

// 405
void remove_cubic(pointer p) //{removes the cubic following~|p|}
{
	pointer q; //{the node that disappears}
	q = link(p); right_type(p) = right_type(q); link(p) = link(q);
	x_coord(p) = x_coord(q); y_coord(p) = y_coord(q);
	right_x(p) = right_x(q); right_y(p) = right_y(q);
	free_node(q,knot_node_size);
}

// 406
void quadrant_subdivide()
{
	pointer p, q, r, s, pp, qq; //{for traversing the lists}
	scaled first_x, first_y; //{unnegated coordinates of node |cur_spec|}
	scaled del1, del2, del3, del, dmax; //{proportional to the control points of a quadratic derived from a cubic}
	fraction t; //{where a quadratic crosses zero}
	scaled dest_x, dest_y; //{final values of |x| and |y| in the current cubic}
	bool constant_x; //{is |x| constant between |p| and |q|?}

	p = cur_spec; first_x = x_coord(cur_spec); first_y = y_coord(cur_spec);
	do {
	mycontinue:
		q = link(p);
		#pragma region <Subdivide the cubic between |p| and |q| so that the results travel toward the right halfplane>
		if (q == cur_spec)
		{
			dest_x = first_x; dest_y = first_y;
		}
		else {
			dest_x = x_coord(q); dest_y = y_coord(q);
		}
		del1 = right_x(p) - x_coord(p); del2 = left_x(q) - right_x(p);
		del3 = dest_x - left_x(q);
		#pragma region <Scale up |del1|, |del2|, and |del3| for greater accuracy; also set |del| to the first nonzero element of |(del1,del2,del3)|>
		if (del1 != 0) del = del1;
		else if (del2 != 0) del = del2;
		else del = del3;
		if (del != 0)
		{
			dmax = myabs(del1);
			if (myabs(del2) > dmax) dmax = myabs(del2);
			if (myabs(del3) > dmax) dmax = myabs(del3);
			while (dmax < fraction_half)
			{
				_double(dmax); _double(del1); _double(del2); _double(del3);
			}
		}
		#pragma endregion
		if (del == 0) constant_x = true;
		else {
			constant_x = false;
			if (del < 0)
				#pragma region <Complement the |x| coordinates of the cubic between |p| and~|q|>
			{
				negate(x_coord(p)); negate(right_x(p));
				negate(left_x(q));
				negate(del1); negate(del2); negate(del3);
				negate(dest_x);
				right_type(p) = first_octant + negate_x;
			}
				#pragma endregion
			t = crossing_point(del1, del2, del3);
			if (t < fraction_one)
				#pragma region <Subdivide the cubic with respect to $x$, possibly twice>
			{
				split_cubic(p, t, dest_x, dest_y); r = link(p);
				if (right_type(r) > negate_x) right_type(r) = first_octant;
				else right_type(r) = first_octant + negate_x;
				if (x_coord(r) < x_coord(p)) x_coord(r) = x_coord(p);
				left_x(r) = x_coord(r);
				if (right_x(p) > x_coord(r)) right_x(p) = x_coord(r);
				//{we always have |x_coord(p)<=right_x(p)|}
				negate(x_coord(r)); right_x(r) = x_coord(r);
				negate(left_x(q)); negate(dest_x);
				del2 = t_of_the_way(del2, del3);
				//{now |0,del2,del3| represent $x'$ on the remaining interval}
				if (del2 > 0) del2 = 0;
				t = crossing_point(0, -del2, -del3);
				if (t < fraction_one)
					#pragma region <Subdivide the cubic a second time with respect to $x$>
				{
					split_cubic(r, t, dest_x, dest_y); s = link(r);
					if (x_coord(s) < dest_x) x_coord(s) = dest_x;
					if (x_coord(s) < x_coord(r)) x_coord(s) = x_coord(r);
					right_type(s) = right_type(p);
					left_x(s) = x_coord(s); //{now |x_coord(r)=right_x(r)<=left_x(s)|}
					if (left_x(q) < dest_x) left_x(q) = -dest_x;
					else if (left_x(q) > x_coord(s)) left_x(q) = -x_coord(s);
					else negate(left_x(q));
					negate(x_coord(s)); right_x(s) = x_coord(s);
				}
					#pragma endregion
				else {
					if (x_coord(r) > dest_x)
					{
						x_coord(r) = dest_x; left_x(r) = -x_coord(r); right_x(r) = x_coord(r);
					}
					if (left_x(q) > dest_x) left_x(q) = dest_x;
					else if (left_x(q) < x_coord(r)) left_x(q) = x_coord(r);
				}
			}
				#pragma endregion
		}

		#pragma endregion


		#pragma region <Subdivide all cubics between |p| and |q| so that the results travel toward the first quadrant; but |return| or |goto continue| if the cubic from |p| to |q| was dead>
		pp = p;
		do {
			qq = link(pp);
			abnegate(x_coord(qq), y_coord(qq), right_type(qq), right_type(pp));
			dest_x = cur_x; dest_y = cur_y;
			del1 = right_y(pp) - y_coord(pp); del2 = left_y(qq) - right_y(pp);
			del3 = dest_y - left_y(qq);
			#pragma region <Scale up |del1|, |del2|, and |del3| for greater accuracy; also set |del| to the first nonzero element of |(del1,del2,del3)|>
			if (del1 != 0) del = del1;
			else if (del2 != 0) del = del2;
			else del = del3;
			if (del != 0)
			{
				dmax = myabs(del1);
				if (myabs(del2) > dmax) dmax = myabs(del2);
				if (myabs(del3) > dmax) dmax = myabs(del3);
				while (dmax < fraction_half)
				{
					_double(dmax); _double(del1); _double(del2); _double(del3);
				}
			}
			#pragma endregion
			if (del != 0) //{they weren't all zero}
			{
				if (del < 0)
					#pragma region <Complement the |y| coordinates of the cubic between |pp| and~|qq|>
				{
					negate(y_coord(pp)); negate(right_y(pp));
					negate(left_y(qq));
					negate(del1); negate(del2); negate(del3);
					negate(dest_y);
					right_type(pp) = right_type(pp) + negate_y;
				}
					#pragma endregion
				t = crossing_point(del1, del2, del3);
				if (t < fraction_one)
					#pragma region <Subdivide the cubic with respect to $y$, possibly twice>
				{
					split_cubic(pp, t, dest_x, dest_y); r = link(pp);
					if (right_type(r) > negate_y) right_type(r) = right_type(r) - negate_y;
					else right_type(r) = right_type(r) + negate_y;
					if (y_coord(r) < y_coord(pp)) y_coord(r) = y_coord(pp);
					left_y(r) = y_coord(r);
					if (right_y(pp) > y_coord(r)) right_y(pp) = y_coord(r);
					//{we always have |y_coord(pp)<=right_y(pp)|}
					negate(y_coord(r)); right_y(r) = y_coord(r);
					negate(left_y(qq)); negate(dest_y);
					if (x_coord(r) < x_coord(pp)) x_coord(r) = x_coord(pp);
					else if (x_coord(r) > dest_x) x_coord(r) = dest_x;
					if (left_x(r) > x_coord(r))
					{
						left_x(r) = x_coord(r);
						if (right_x(pp) > x_coord(r)) right_x(pp) = x_coord(r);
					}
					if (right_x(r) < x_coord(r))
					{
						right_x(r) = x_coord(r);
						if (left_x(qq) < x_coord(r)) left_x(qq) = x_coord(r);
					}
					del2 = t_of_the_way(del2, del3);
					//{now |0,del2,del3| represent $y'$ on the remaining interval}
					if (del2 > 0) del2 = 0;
					t = crossing_point(0, -del2, -del3);
					if (t < fraction_one)
						#pragma region <Subdivide the cubic a second time with respect to $y$>
					{
						split_cubic(r, t, dest_x, dest_y); s = link(r);
						if (y_coord(s) < dest_y) y_coord(s) = dest_y;
						if (y_coord(s) < y_coord(r)) y_coord(s) = y_coord(r);
						right_type(s) = right_type(pp);
						left_y(s) = y_coord(s); //{now |y_coord(r)=right_y(r)<=left_y(s)|}
						if (left_y(qq) < dest_y) left_y(qq) = -dest_y;
						else if (left_y(qq) > y_coord(s)) left_y(qq) = -y_coord(s);
						else negate(left_y(qq));
						negate(y_coord(s)); right_y(s) = y_coord(s);
						if (x_coord(s) < x_coord(r)) x_coord(s) = x_coord(r);
						else if (x_coord(s) > dest_x) x_coord(s) = dest_x;
						if (left_x(s) > x_coord(s))
						{
							left_x(s) = x_coord(s);
							if (right_x(r) > x_coord(s)) right_x(r) = x_coord(s);
						}
						if (right_x(s) < x_coord(s))
						{
							right_x(s) = x_coord(s);
							if (left_x(qq) < x_coord(s)) left_x(qq) = x_coord(s);
						}
					}

						#pragma endregion
					else {
						if (y_coord(r) > dest_y)
						{
							y_coord(r) = dest_y; left_y(r) = -y_coord(r); right_y(r) = y_coord(r);
						}
						if (left_y(qq) > dest_y) left_y(qq) = dest_y;
						else if (left_y(qq) < y_coord(r)) left_y(qq) = y_coord(r);
					}
				}

					#pragma endregion
			}
			else
				#pragma region <Do any special actions needed when |y| is constant; |return| or |goto continue| if a dead cubic from |p| to |q| is removed>
				if (constant_x) //{|p=pp|, |q=qq|, and the cubic is dead}
				{
					if (q != p)
					{
						remove_cubic(p); //{remove the dead cycle and recycle node |q|}
						if (cur_spec != q) goto mycontinue;
						else {
							cur_spec = p; return;
						} //{the final cubic was dead and is gone}
					}
				}
				else if (!myodd(right_type(pp))) //{the $x$ coordinates were negated}
					#pragma region <Complement the |y| coordinates...>
				{
					negate(y_coord(pp)); negate(right_y(pp));
					negate(left_y(qq));
					negate(del1); negate(del2); negate(del3);
					negate(dest_y);
					right_type(pp) = right_type(pp) + negate_y;
				}
					#pragma endregion

				#pragma endregion
			pp = qq;
		} while (!(pp == q));
		if (constant_x)
			#pragma region <Correct the octant code in segments with decreasing |y|>
		{
			pp = p;
			do {
				qq = link(pp);
				if (right_type(pp) > negate_y) //{the $y$ coordinates were negated}
				{
					right_type(pp) = right_type(pp) + negate_x;
					negate(x_coord(pp)); negate(right_x(pp)); negate(left_x(qq));
				}
				pp = qq;
			} while (!(pp == q));
		}
			#pragma endregion

		#pragma endregion


		p = q;
	} while (!(p == cur_spec));
}


// 410
void split_cubic(pointer p, fraction t, scaled xq, scaled yq) //{splits the cubic after |p|}
{
	scaled v; //{an intermediate value}
	pointer q, r; //{for list manipulation}
	q = link(p); r = get_node(knot_node_size); link(p) = r; link(r) = q;
	left_type(r) = left_type(q); right_type(r) = right_type(p);
	v = t_of_the_way(right_x(p),left_x(q));
	right_x(p) = t_of_the_way(x_coord(p),right_x(p));
	left_x(q) = t_of_the_way(left_x(q),xq);
	left_x(r) = t_of_the_way(right_x(p),v);
	right_x(r) = t_of_the_way(v,left_x(q));
	x_coord(r) = t_of_the_way(left_x(r),right_x(r));
	v = t_of_the_way(right_y(p),left_y(q));
	right_y(p) = t_of_the_way(y_coord(p),right_y(p));
	left_y(q) = t_of_the_way(left_y(q),yq);
	left_y(r) = t_of_the_way(right_y(p),v);
	right_y(r) = t_of_the_way(v,left_y(q));
	y_coord(r) = t_of_the_way(left_y(r),right_y(r));
}



// 419
void octant_subdivide()
{
	pointer  p, q, r, s; //{for traversing the lists}
	scaled del1, del2, del3, del, dmax; //{proportional to the control points of a quadratic derived from a cubic}
	fraction t; //{where a quadratic crosses zero}
	scaled dest_x, dest_y; //{final values of |x| and |y| in the current cubic}
	p = cur_spec;
	do { 
		q = link(p);
		x_coord(p) = x_coord(p) - y_coord(p);
		right_x(p) = right_x(p) - right_y(p);
		left_x(q) = left_x(q) - left_y(q);
		#pragma region <Subdivide the cubic between |p| and |q| so that the results travel toward the first octant>
		
		#pragma region <Set up the variables |(del1,del2,del3)| to represent $x-y$>
		if (q == cur_spec)
		{ 
			unskew(x_coord(q),y_coord(q),right_type(q));
			skew(cur_x,cur_y,right_type(p)); dest_x = cur_x; dest_y = cur_y;
		}
		else { 
			abnegate(x_coord(q),y_coord(q),right_type(q),right_type(p));
			dest_x = cur_x - cur_y; dest_y = cur_y;
		}
		del1 = right_x(p) - x_coord(p); del2 = left_x(q) - right_x(p);
		del3 = dest_x - left_x(q);
		
		#pragma endregion 
		
		
		#pragma region <Scale up |del1|, |del2|, and |del3| for greater accuracy; also set |del| to the first nonzero element of |(del1,del2,del3)|>
		if (del1 != 0) del = del1;
		else if (del2 != 0) del = del2;
		else del = del3;
		if (del != 0)
		{
			dmax = myabs(del1);
			if (myabs(del2) > dmax) dmax = myabs(del2);
			if (myabs(del3) > dmax) dmax = myabs(del3);
			while (dmax < fraction_half)
			{
				_double(dmax); _double(del1); _double(del2); _double(del3);
			}
		}
		#pragma endregion 
		
		if (del != 0) //{they weren't all zero}
		{ 
			if (del < 0) 
				#pragma region <Swap the |x| and |y| coordinates of the cubic between |p| and~|q|>
			{
				y_coord(p) = x_coord(p) + y_coord(p); negate(x_coord(p));
				right_y(p) = right_x(p) + right_y(p); negate(right_x(p));
				left_y(q) = left_x(q) + left_y(q); negate(left_x(q));
				negate(del1); negate(del2); negate(del3);
				dest_y = dest_x + dest_y; negate(dest_x);
				right_type(p) = right_type(p) + switch_x_and_y;
			}
				#pragma endregion
			t = crossing_point(del1,del2,del3);
			if (t < fraction_one)
				#pragma region <Subdivide the cubic with respect to $x'-y'$, possibly twice>
			{
				split_cubic(p,t,dest_x,dest_y); r = link(p);
				if (right_type(r) > switch_x_and_y) right_type(r) = right_type(r) - switch_x_and_y;
				else right_type(r) = right_type(r) + switch_x_and_y;
				if (y_coord(r) < y_coord(p)) y_coord(r) = y_coord(p);
				else if (y_coord(r) > dest_y) y_coord(r) = dest_y;
				if (x_coord(p) + y_coord(r) > dest_x + dest_y)
				y_coord(r) = dest_x + dest_y - x_coord(p);
				if (left_y(r) > y_coord(r))
				{ 
					left_y(r) = y_coord(r);
					if (right_y(p) > y_coord(r)) right_y(p) = y_coord(r);
				}
				if (right_y(r) < y_coord(r))
				{ 
					right_y(r) = y_coord(r);
					if (left_y(q) < y_coord(r)) left_y(q) = y_coord(r);
				}
				if (x_coord(r) < x_coord(p)) x_coord(r) = x_coord(p);
				else if (x_coord(r) + y_coord(r) > dest_x + dest_y)
				x_coord(r) = dest_x + dest_y - y_coord(r);
				left_x(r) = x_coord(r);
				if (right_x(p) > x_coord(r)) right_x(p) = x_coord(r);
				//{we always have |x_coord(p)<=right_x(p)|}
				y_coord(r) = y_coord(r) + x_coord(r); right_y(r) = right_y(r) + x_coord(r);
				negate(x_coord(r)); right_x(r) = x_coord(r);
				left_y(q) = left_y(q) + left_x(q); negate(left_x(q));
				dest_y = dest_y + dest_x; negate(dest_x);
				if (right_y(r) < y_coord(r))
				{ 
					right_y(r) = y_coord(r);
					if (left_y(q) < y_coord(r)) left_y(q) = y_coord(r);
				}
				del2 = t_of_the_way(del2,del3);
				//{now |0,del2,del3| represent $x'-y'$ on the remaining interval}
				if (del2 > 0) del2 = 0;
				t = crossing_point(0,-del2,-del3);
				if (t < fraction_one)
					#pragma region <Subdivide the cubic a second time with respect to $x-y$>
				{ 
					split_cubic(r,t,dest_x,dest_y); s = link(r);
					if (y_coord(s) < y_coord(r)) y_coord(s) = y_coord(r);
					else if (y_coord(s) > dest_y) y_coord(s) = dest_y;
					if (x_coord(r) + y_coord(s) > dest_x + dest_y)
						y_coord(s) = dest_x + dest_y - x_coord(r);
					if (left_y(s) > y_coord(s))
					{ 
						left_y(s) = y_coord(s);
						if (right_y(r) > y_coord(s)) right_y(r) = y_coord(s);
					}
					if (right_y(s) < y_coord(s))
					{ 
						right_y(s) = y_coord(s);
						if (left_y(q) < y_coord(s)) left_y(q) = y_coord(s);
					}
					if (x_coord(s) + y_coord(s) > dest_x + dest_y) x_coord(s) = dest_x + dest_y - y_coord(s);
					else {
						if (x_coord(s) < dest_x) x_coord(s) = dest_x;
						if (x_coord(s) < x_coord(r)) x_coord(s) = x_coord(r);
					}
					right_type(s) = right_type(p);
					left_x(s) = x_coord(s); //{now |x_coord(r)=right_x(r)<=left_x(s)|}
					if (left_x(q) < dest_x)
					{ 
						left_y(q) = left_y(q) + dest_x; left_x(q) = -dest_x;
					}
					else if (left_x(q) > x_coord(s))
					{ 
						left_y(q) = left_y(q) + x_coord(s); left_x(q) = -x_coord(s);
					}
					else { 
						left_y(q) = left_y(q) + left_x(q); negate(left_x(q));
					}
					y_coord(s) = y_coord(s) + x_coord(s); right_y(s) = right_y(s) + x_coord(s);
					negate(x_coord(s)); right_x(s) = x_coord(s);
					if (right_y(s) < y_coord(s))
					{ 
						right_y(s) = y_coord(s);
						if (left_y(q) < y_coord(s)) left_y(q) = y_coord(s);
					}
				}				
					#pragma endregion
				else { 
					if (x_coord(r) > dest_x)
					{ 
						x_coord(r) = dest_x; left_x(r) = -x_coord(r); right_x(r) = x_coord(r);
					}
					if (left_x(q) > dest_x) left_x(q) = dest_x;
					else if (left_x(q) < x_coord(r)) left_x(q) = x_coord(r);
				}
			}		
				#pragma endregion
		}
		#pragma endregion
		p = q;
	} while (!(p == cur_spec));
}


// 426
void make_safe()
{
	int k; // 0 .. max wiggle; {runs through the list of inputs}
	bool all_safe; //{does everything look OK so far?}
	scaled next_a; //{after[k] before it might have changed}
	scaled delta_a, delta_b; //{after[k + 1] - after[k] and before[k + 1] - before[k]}


	before[cur_rounding_ptr] = before[0]; // {wrap around}
	node_to_round[cur_rounding_ptr] = node_to_round[0];
	do { after[cur_rounding_ptr] = after[0]; all_safe = true; next_a = after[0];
		for (k = 0; k <= cur_rounding_ptr - 1; k++)
		{ 
			delta_b = before[k + 1] - before[k];
			if (delta_b >= 0) delta_a = after[k + 1] - next_a;
			else delta_a = next_a - after[k + 1];
			next_a = after[k + 1];
			if (delta_a < 0 || delta_a > myabs(delta_b + delta_b))
			{ 
				all_safe = false; after[k] = before[k];
				if (k == cur_rounding_ptr - 1) after[0] = before[0];
				else after[k + 1] = before[k + 1];
			}
		}
	} while (!( all_safe));
}

// 429
void before_and_after(scaled b, scaled a, pointer p)
{
	if (cur_rounding_ptr == max_rounding_ptr)
		if (max_rounding_ptr < max_wiggle) max_rounding_ptr++;
		else overflow(/*rounding table size*/489, max_wiggle);
	after[cur_rounding_ptr] = a; before[cur_rounding_ptr] = b; node_to_round[cur_rounding_ptr] = p;
	cur_rounding_ptr++;
}

// 431
scaled good_val(scaled b,scaled o)
{
	scaled a; //{accumulator}
	a = b + o;
	if (a >= 0) a = a - (a % cur_gran) - o;
	else a = a + ((-(a + 1)) % cur_gran) - cur_gran + 1 - o;
	if (b - a < a + cur_gran - b) return a;
	else return a + cur_gran;
}

// 432
scaled compromise(scaled u, scaled v)
{
	return half(good_val(u + u, -u - v));
}


// 433
void xy_round()
{
	pointer p,q; //{list manipulation registers}
	scaled b, a; //{before and after values}
	scaled pen_edge = INT_MAX; //{offset that governs rounding}
	fraction alpha; //{coefficient of linear transformation}
	
	cur_gran = myabs(internal[granularity]);
	if (cur_gran == 0) cur_gran = unity;
	p = cur_spec; cur_rounding_ptr = 0;
	do { 
		q = link(p);
		#pragma region <If node |q| is a transition point for |x| coordinates, compute and save its before-and-after coordinates>
		if (myodd(right_type(p)) != myodd(right_type(q)))
		{ 
			if (myodd(right_type(q))) b = x_coord(q); else b = -x_coord(q);
			if (myabs(x_coord(q) - right_x(q)) < 655 || myabs(x_coord(q) + left_x(q))<655)
				#pragma region <Compute before-and-after |x| values based on the current pen>
			{ 
				if (cur_pen == null_pen) pen_edge = 0;
				else if (cur_path_type == double_path_code)
					pen_edge = compromise(east_edge(cur_pen),west_edge(cur_pen));
				else if (myodd(right_type(q))) pen_edge = west_edge(cur_pen);
				else pen_edge = east_edge(cur_pen);
				a = good_val(b,pen_edge);
			}
				#pragma endregion
			else a = b;
			if (myabs(a) > max_allowed)
				if (a > 0) a = max_allowed; else a = -max_allowed;
			before_and_after(b,a,q);
		}
		#pragma endregion
		p = q;
	} while(!(p == cur_spec));
	if (cur_rounding_ptr > 0) 
		#pragma region <Transform the |x| coordinates>
	{ 
		make_safe();
		do { 
			cur_rounding_ptr--;
			if (after[cur_rounding_ptr] != before[cur_rounding_ptr] || after[cur_rounding_ptr+1] != before[cur_rounding_ptr+1])
			{ 
				p = node_to_round[cur_rounding_ptr];
				if (myodd(right_type(p)))
				{ 
					b = before[cur_rounding_ptr]; a = after[cur_rounding_ptr];
				}
				else { 
					b = -before[cur_rounding_ptr]; a = -after[cur_rounding_ptr];
				}
				if (before[cur_rounding_ptr] == before[cur_rounding_ptr + 1])
					alpha = fraction_one;
				else alpha = make_fraction(after[cur_rounding_ptr + 1]-after[cur_rounding_ptr],before[cur_rounding_ptr + 1]-before[cur_rounding_ptr]);
				do { 
					x_coord(p) = take_fraction(alpha,x_coord(p) - b) + a;
					right_x(p) = take_fraction(alpha,right_x(p) - b) + a;
					p = link(p); left_x(p) = take_fraction(alpha,left_x(p) - b) + a;
				} while (!(p == node_to_round[cur_rounding_ptr + 1]));
			}
		} while (!(cur_rounding_ptr == 0));
	}
		#pragma endregion
	p = cur_spec; cur_rounding_ptr = 0;
	do {
		q = link(p);
		#pragma region <If node |q| is a transition point for |y| coordinates, compute and save its before-and-after coordinates>
		if (right_type(p) > negate_y != right_type(q) > negate_y)
		{ 
			if (right_type(q) <= negate_y) b = y_coord(q); else b = -y_coord(q);
			if (myabs(y_coord(q) - right_y(q)) < 655 || myabs(y_coord(q)+left_y(q)) < 655)
				#pragma region <Compute before-and-after |y| values based on the current pen>
			{ 
				if (cur_pen == null_pen) pen_edge = 0;
				else if (cur_path_type == double_path_code)
					pen_edge = compromise(north_edge(cur_pen),south_edge(cur_pen));
				else if (right_type(q) <= negate_y) pen_edge = south_edge(cur_pen);
				else pen_edge = north_edge(cur_pen);
				a = good_val(b,pen_edge);
			}			
				#pragma endregion
			else a = b;
			if (myabs(a) > max_allowed)
				if (a > 0) a = max_allowed; else a = -max_allowed;
			before_and_after(b,a,q);
		}
		#pragma endregion
		p = q;
	} while (!(p == cur_spec));
	if (cur_rounding_ptr > 0) 
		#pragma region <Transform the |y| coordinates>
	{ 
		make_safe();
		do { 
			cur_rounding_ptr--;
			if (after[cur_rounding_ptr] != before[cur_rounding_ptr] || after[cur_rounding_ptr+1] != before[cur_rounding_ptr+1])
			{ 
				p = node_to_round[cur_rounding_ptr];
				if (right_type(p) <= negate_y)
				{ 
					b = before[cur_rounding_ptr]; a = after[cur_rounding_ptr];
				}
				else  { 
					b = -before[cur_rounding_ptr]; a = -after[cur_rounding_ptr];
				}
				if (before[cur_rounding_ptr] == before[cur_rounding_ptr + 1])
					alpha = fraction_one;
				else alpha = make_fraction(after[cur_rounding_ptr+1]-after[cur_rounding_ptr],before[cur_rounding_ptr+1]-before[cur_rounding_ptr]);
				do { 
					y_coord(p) = take_fraction(alpha,y_coord(p) - b) + a;
					right_y(p) = take_fraction(alpha,right_y(p) - b) + a;
					p = link(p); left_y(p) = take_fraction(alpha,left_y(p) - b) + a;
				} while (!(p == node_to_round[cur_rounding_ptr + 1]));
			}
		} while (!(cur_rounding_ptr == 0));
	}

		#pragma endregion
}


// 440
void diag_round()
{
	pointer p,q,pp; //{list manipulation registers}
	scaled b,a,bb,aa,d = INT_MAX,c = INT_MAX,dd = INT_MAX,cc = INT_MAX; //{before and after values}
	scaled pen_edge = INT_MAX; //{offset that governs rounding}
	fraction alpha,beta; //{coefficients of linear transformation}
	scaled next_a; //{after[k] before it might have changed}
	bool all_safe; //{does everything look OK so far?}
	int k; // 0 .. max_wiggle, {runs through before-and-after values}
	scaled first_x,first_y; //{coordinates before rounding}
		
	p = cur_spec; cur_rounding_ptr = 0;
	do { 
		q = link(p);
		#pragma region <If node q is a transition point between octants, compute and save its before-and-after coordinates 441>

		if (right_type(p) != right_type(q))
		{ 
			if (right_type(q) > switch_x_and_y) b = -x_coord(q);
			else b = x_coord(q);
			if (myabs(right_type(q) - right_type(p)) == switch_x_and_y)
				if (myabs(x_coord(q) - right_x(q)) < 655 || myabs(x_coord(q) + left_x(q)) < 655)
					#pragma region <Compute a good coordinate at a diagonal transition 442>
				{
					if (cur_pen == null_pen) pen_edge = 0;
					else if (cur_path_type == double_path_code)
						#pragma region <Compute a compromise pen_edge 443>
					{
						switch( right_type(q) )
						{
							case first_octant: 
							case second_octant: 
								pen_edge = compromise(diag_offset(first_octant),-diag_offset(fifth_octant));
								break;
							case fifth_octant: 
							case sixth_octant: 
								pen_edge = -compromise(diag_offset(first_octant),-diag_offset(fifth_octant));
								break;
							case third_octant: case fourth_octant: 
								pen_edge = compromise(diag_offset(fourth_octant),
													-diag_offset(eighth_octant));
								break;
							case seventh_octant: 
							case eighth_octant: 
								pen_edge = -compromise(diag_offset(fourth_octant),
													-diag_offset(eighth_octant));
								break;
						} // {there are no other cases}
					}
						#pragma endregion
					else if (right_type(q) <= switch_x_and_y) pen_edge = diag_offset(right_type(q));
					else pen_edge = -diag_offset(right_type(q));
					if (myodd(right_type(q))) a = good_val(b,pen_edge + half(cur_gran));
					else a = good_val(b - 1,pen_edge + half(cur_gran));					
				}
					#pragma endregion
				else a = b;
			else a = b;
			before_and_after(b,a,q);
		}
		#pragma endregion
		p = q;
	} while (!( p == cur_spec));
	if (cur_rounding_ptr > 0)
		#pragma region <Transform the skewed coordinates 444>
	{


		 p = node_to_round[0]; first_x = x_coord(p); first_y = y_coord(p);
		#pragma region <Make sure that all the diagonal roundings are safe 446>

		before[cur_rounding_ptr] = before[0]; // {cf. make safe }
		node_to_round[cur_rounding_ptr] = node_to_round[0];
		do { 
			after[cur_rounding_ptr] = after[0]; all_safe = true; next_a = after[0];
			for (k = 0; k <= cur_rounding_ptr - 1; k++)
			{ 
				a = next_a; b = before[k]; next_a = after[k + 1]; aa = next_a; bb = before[k + 1];
				if (a != b || aa != bb)
				{ 
					p = node_to_round[k]; pp = node_to_round[k + 1];
					#pragma region <Determine the before-and-after values of both coordinates 445>

					if (aa == bb)
					{ 
						if (pp == node_to_round[0]) unskew(first_x,first_y,right_type(pp));
						else unskew(x_coord(pp),y_coord(pp),right_type(pp));
						skew(cur_x,cur_y,right_type(p)); bb = cur_x; aa = bb; dd = cur_y; cc = dd;
						if (right_type(p) > switch_x_and_y)
						{ 
							b = -b; a = -a;
						}
					}
					else { 
						if (right_type(p) > switch_x_and_y)
						{ 
							bb = -bb; aa = -aa; b = -b; a = -a;
						}
						if (pp == node_to_round[0]) dd = first_y - bb; else dd = y_coord(pp) - bb;
						if (myodd(aa - bb))
							if (right_type(p) > switch_x_and_y) cc = dd - half(aa - bb + 1);
							else cc = dd - half(aa - bb - 1);
						else cc = dd - half(aa - bb);
					}
					d = y_coord(p);
					if (myodd(a - b))
						if (right_type(p) > switch_x_and_y) c = d - half(a - b - 1);
						else c = d - half (a - b + 1);
					else c = d - half (a - b);


					#pragma endregion
					if (aa < a || cc < c || aa - a > 2 * (bb - b) || cc - c > 2 * (dd - d))
					{ 
						all_safe = false; after[k] = before[k];
						if (k == cur_rounding_ptr - 1) after[0] = before[0];
						else after[k + 1] = before[k + 1];
					}
				}
			}
		} while(!( all_safe));


		#pragma endregion
		for (k = 0; k <= cur_rounding_ptr - 1; k++)
		{ 
			a = after[k]; b = before[k]; aa = after[k + 1]; bb = before[k + 1];
			if (a != b || aa != bb)
			{ 
				p = node_to_round[k]; pp = node_to_round[k + 1];
				#pragma region <Determine the before-and-after values of both coordinates 445>
				if (aa == bb)
				{ 
					if (pp == node_to_round[0]) unskew(first_x,first_y,right_type(pp));
					else unskew(x_coord(pp),y_coord(pp),right_type(pp));
					skew(cur_x,cur_y,right_type(p)); bb = cur_x; aa = bb; dd = cur_y; cc = dd;
					if (right_type(p) > switch_x_and_y)
					{ 
						b = -b; a = -a;
					}
				}
				else { 
					if (right_type(p) > switch_x_and_y)
					{ 
						bb = -bb; aa = -aa; b = -b; a = -a;
					}
					if (pp == node_to_round[0]) dd = first_y - bb; else dd = y_coord(pp) - bb;
					if (myodd(aa - bb))
						if (right_type(p) > switch_x_and_y) cc = dd - half(aa - bb + 1);
						else cc = dd - half(aa - bb - 1);
					else cc = dd - half(aa - bb);
				}
				d = y_coord(p);
				if (myodd(a - b))
					if (right_type(p) > switch_x_and_y) c = d - half(a - b - 1);
					else c = d - half (a - b + 1);
				else c = d - half (a - b);
				#pragma endregion

				if (b == bb) alpha = fraction_one;
				else alpha = make_fraction(aa - a,bb - b);
				if (d == dd) beta = fraction_one;
				else beta = make_fraction(cc - c,dd - d);
				do { 
					x_coord(p) = take_fraction(alpha,x_coord(p) - b) + a;
					y_coord(p) = take_fraction(beta,y_coord(p) - d) + c;
					right_x(p) = take_fraction(alpha,right_x(p) - b) + a;
					right_y(p) = take_fraction(beta,right_y(p) - d) + c; p = link(p);
					left_x(p) = take_fraction(alpha,left_x(p)-b)+a; left_y(p) = take_fraction(beta,left_y(p)-d)+c;
				} while (!( p == pp));
			}
		}


	}
		#pragma endregion
}


// 451
void new_boundary(pointer p, small_number octant)
{
	pointer q,r; //{for list manipulation}
	q = link(p); //{we assume that |right_type(q)<>endpoint|}
	r = get_node(knot_node_size); link(r) = q; link(p) = r;
	left_type(r) = left_type(q); //{but possibly |left_type(q)=endpoint|}
	left_x(r) = left_x(q); left_y(r) = left_y(q);
	right_type(r) = endpoint; left_type(q) = endpoint;
	right_octant(r) = octant; left_octant(q) = right_type(q);
	unskew(x_coord(q),y_coord(q),right_type(q));
	skew(cur_x,cur_y,octant); x_coord(r) = cur_x; y_coord(r) = cur_y;
}


// 463
void end_round(scaled x, scaled y)
{
	y = y + half_unit - y_corr[octant];
	x = x + y - x_corr[octant];
	m1 = floor_unscaled(x); n1 = floor_unscaled(y);
	if (x - unity * m1 >= y - unity * n1 + z_corr[octant]) d1 = 1; else d1 = 0;
}

// 465
void fill_spec(pointer h)
{
	pointer p,q,r,s; // {for list traversal}
	
	if (internal[tracing_edges] > 0) begin_edge_tracing();
	p = h; // {we assume that left type(h) = endpoint }
	do { 
		octant = left_octant(p); 
		#pragma region <Set variable q to the node at the end of the current octant 466>
		q = p;
		while (right_type(q) != endpoint) q = link(q);
		#pragma endregion
		if (q != p)
		{ 
			#pragma region <Determine the starting and ending lattice points (m0,n0) and (m1,n1) 467>
			end_round(x_coord(p), y_coord(p)); m0 = m1; n0 = n1; d0 = d1;
			end_round(x_coord(q), y_coord(q));
			#pragma endregion
			
			#pragma region <Make the moves for the current octant 468>

			if (n1 - n0 >= move_size) overflow(/*move table size*/469,move_size);
			move[0] = d0; move_ptr = 0; r = p;
			do { 
				s = link(r);
				make_moves(x_coord(r),right_x(r),left_x(s),x_coord(s),
				y_coord(r) + half_unit,right_y(r) + half_unit,left_y(s) + half_unit,y_coord(s) + half_unit,
				xy_corr[octant],y_corr[octant]); r = s;
			} while (!( r == q));
			move[move_ptr] = move[move_ptr] - d1;
			if (internal[smoothing] > 0) smooth_moves(0,move_ptr);


			#pragma endregion
			
			move_to_edges(m0,n0,m1,n1);
		}
		p = link(q);
	} while (!( p == h));
	toss_knot_list(h);
	if (internal[tracing_edges] > 0) end_edge_tracing();
}


// 473
void print_pen(pointer p, str_number s, bool nuline)
{
	bool nothing_printed; // has there been any action yet?
	int k; // 1..8, octant number
	pointer h; // offset list head
	int m, n; // offset indices
	pointer w, ww; // pointers that traverse the offset list

	print_diagnostic(/*Pen polygon*/490, s, nuline); nothing_printed = true; print_ln();
	for (k = 1; k <= 8; k++) {
		octant = octant_code[k]; h = p + octant; n = info(h); w = link(h);
		if (!myodd(k)) w = knil(w); // in even octants, start at w_{n+1}
		for (m = 1; m <= n + 1; m++) {
			if (myodd(k)) ww = link(w); else ww = knil(w);
			if (x_coord(ww) != x_coord(w) || y_coord(ww) != y_coord(w))
				#pragma region <Print the unskewed and unrotated coordinates of node ww 474>
			{
				if (nothing_printed) nothing_printed = false;
				else print_nl(/* .. */491);
				print_two_true(x_coord(ww), y_coord(ww));
			}
				#pragma endregion

			w = ww;
		}
	}
	if (nothing_printed) {
		w = link(p + first_octant); print_two(x_coord(w) + y_coord(w), y_coord(w));
	}
	print_nl(/* .. cycle*/492); end_diagnostic(true);

}

// 476
void dup_offset(pointer w)
{
	pointer r; //  the new node
	r = get_node(coord_node_size); x_coord(r) = x_coord(w); y_coord(r) = y_coord(w);
	link(r) = link(w); knil(link(w)) = r; knil(r) = w; link(w) = r;
}


// 477
pointer make_pen(pointer h)
{
	small_number o, oo, k; //{octant numbers---old, new, and current}
	pointer p; //{top-level node for the new pen}
	pointer q, r, s, w = max_halfword, hh; //{for list manipulation}
	int n; //{offset counter}
	scaled dx, dy; //{polygon direction}
	scaled mc; //{the largest coordinate}

	#pragma region <Stamp all nodes with an octant code, compute the maximum offset, and set |hh| to the node that begins the first octant; |goto not_found| if theres a problem>
	q = h; r = link(q); mc = myabs(x_coord(h));
	if (q == r)
	{ 
		hh = h; right_type(h) = 0; //{this trick is explained below}
		if (mc < myabs(y_coord(h))) mc = myabs(y_coord(h));
	}
	else {
		o = 0; hh = null;
		while (true) {
			s = link(r);
			if (mc < myabs(x_coord(r))) mc = myabs(x_coord(r));
			if (mc < myabs(y_coord(r))) mc = myabs(y_coord(r));
			dx = x_coord(r) - x_coord(q); dy = y_coord(r) - y_coord(q);
			if (dx == 0)
				if (dy == 0) goto not_found; //{double point}
			if (ab_vs_cd(dx,y_coord(s) - y_coord(r), dy, x_coord(s) - x_coord(r)) < 0)
				goto not_found; //{right turn}
			#pragma region <Determine the octant code for direction |(dx,dy)|>
			if (dx > 0) octant = first_octant;
			else if (dx == 0)
				if (dy > 0) octant = first_octant; else octant = first_octant + negate_x;
			else {
				negate(dx); octant = first_octant + negate_x;
			}
			if (dy < 0)
			{
				negate(dy); octant = octant + negate_y;
			}
			else if (dy == 0)
				if (octant > first_octant) octant = first_octant + negate_x + negate_y;
			if (dx < dy) octant = octant + switch_x_and_y;
			
			#pragma endregion
			right_type(q) = octant; oo = octant_number[octant];
			if (o > oo)
			{
				if (hh != null) goto not_found; //{$>360^\circ$}
				hh = q;
			}
			o = oo;
			if (q == h && hh != null) goto done;
			q = r; r = s;
		}
	done:
		;
	}
	#pragma endregion
	if (mc >= fraction_one - half_unit) goto not_found;
	p = get_node(pen_node_size); q = hh; max_offset(p) = mc; ref_count(p) = null;
	if (link(q) != q) link(p) = null + 1;
	for (k = 1; k <= 8; k++) 
		#pragma region <Construct the offset list for the |k|th octant>
	{ 
		octant = octant_code[k]; n = 0; h = p + octant;
		while (true) { 
			r = get_node(coord_node_size);
			skew(x_coord(q),y_coord(q),octant); x_coord(r) = cur_x; y_coord(r) = cur_y;
			if (n == 0) link(h) = r;
			else 
				#pragma region <Link node |r| to the previous node>
				if (myodd(k))
				{ 
					link(w) = r; knil(r) = w;
				}
				else { 
					knil(w) = r; link(r) = w;
				}
				#pragma endregion
			w = r;
			if (right_type(q) != octant) goto done1;
			q = link(q); n++;
		}
	done1: 
		#pragma region <Finish linking the offset nodes, and duplicate the borderline offset nodes if necessary>
		r = link(h);
		if (myodd(k))
		{
			link(w) = r; knil(r) = w;
		}
		else {
			knil(w) = r; link(r) = w; link(h) = w; r = w;
		}
		if ((y_coord(r) != y_coord(link(r))) || (n == 0))
		{
			dup_offset(r); n++;
		}
		r = knil(r);
		if (x_coord(r) != x_coord(knil(r))) dup_offset(r);
		else n--;
		#pragma endregion
		if (n >= max_quarterword) overflow(/*pen polygon size*/493,max_quarterword);
		info(h) = n;
	}
		#pragma endregion
	goto found;
not_found:
	p = null_pen; 
	#pragma region <Complain about a bad pen path>
	if (mc >= fraction_one - half_unit)
	{
		print_err(/*Pen too large*/494);
		help2(/*The cycle you specified has a coordinate of 4095.5 or more.*/495,
			/*So I've replaced it by the trivial path `(0,0)..cycle'.*/496);
	}
	else {
		print_err(/*Pen cycle must be convex*/497);
		help3(/*The cycle you specified either has consecutive equal points*/498,
			/*or turns right or turns through more than 360 degrees.*/499,
			/*So I've replaced it by the trivial path `(0,0)..cycle'.*/496);
	}
	put_get_error();
	#pragma endregion
found: 
	if (internal[tracing_pens] > 0) print_pen(p,/* (newly created)*/500,true);
	return p;
}


// 484
pointer make_path(pointer pen_head)
{
	pointer p; //{the most recently copied knot}
	int k; //1 .. 8, {octant number}
	pointer h; //{offset list head}
	int m,n; //{offset indices}
	pointer w,ww; //{pointers that traverse the offset list}
	p = temp_head;
	for (k = 1; k <= 8; k++)
	{ 
		octant = octant_code[k]; h = pen_head + octant; n = info(h); w = link(h);
		if (!myodd(k)) w = knil(w); // {in even octants, start at w_{n+1} }
		for (m = 1; m <=  n + 1; m++)
		{ 
			if (myodd(k)) ww = link(w); else ww = knil(w);
			if (x_coord(ww) != x_coord(w) || y_coord(ww) != y_coord(w))
				#pragma region <Copy the unskewed and unrotated coordinates of node ww 485>
			{
				unskew(x_coord(ww), y_coord(ww), octant); link(p) = trivial_knot(cur_x, cur_y); p = link(p);
			}
				#pragma endregion

			w = ww;
		}
	}
	if (p == temp_head)
	{ 
		w = link(pen_head + first_octant); p = trivial_knot(x_coord(w) + y_coord(w),y_coord(w));
		link(temp_head) = p;
	}
	link(p) = link(temp_head); return link(temp_head);
}

//
pointer trivial_knot(scaled x, scaled y)
{
	pointer p; // a new knot for explicit coordinates x and y

	p = get_node(knot_node_size); left_type(p) = _explicit; right_type(p) = _explicit;
	x_coord(p) = x; left_x(p) = x; right_x(p) = x;
	y_coord(p) = y; left_y(p) = y; right_y(p) = y;
	return p;
}


// 487
void toss_pen(pointer p)
{
	int k; // 1..8, relative header locations
	pointer w, ww; // pointers to the offset nodes

	if (p != null_pen) {
		for (k = 1; k <= 8; k++) {
			w = link(p + k);
			do {
				ww = link(w); free_node(w, coord_node_size); w = ww;
			} while (!(w == link(p + k)));
		}
		free_node(p, pen_node_size);
	}
}

// 488
void find_offset(scaled x, scaled y, pointer p)
{
	int octant;//:first_octant..sixth_octant; {octant code for |(x,y)|}
	int s;//:-1..+1; {sign of the octant}
	int n; //{number of offsets remaining}
	pointer h, w, ww; //{list traversal registers}

	#pragma region <Compute the octant code; skew and rotate the coordinates |(x,y)|>
	if (x > 0) octant = first_octant;
	else if (x == 0)
		if (y <= 0)
			if (y == 0)
			{ 
				cur_x = 0; cur_y = 0; return;
			}
			else octant = first_octant + negate_x;
		else octant = first_octant;
	else { 
		x = -x;
		if (y == 0) octant = first_octant + negate_x + negate_y;
		else octant = first_octant + negate_x;
	}
	if (y < 0)
	{ 
		octant = octant + negate_y; y = -y;
	}
	if (x >= y) x = x - y;
	else { 
		octant = octant + switch_x_and_y; x = y - x; y = y - x;
	}
	#pragma endregion
	if (myodd(octant_number[octant])) s = -1; else s = +1;
	h = p + octant; w = link(link(h)); ww = link(w); n = info(h);
	while (n > 1)
	{ 
		if (ab_vs_cd(x,y_coord(ww) - y_coord(w),y,x_coord(ww)-x_coord(w)) != s) goto done;
		w = ww; ww = link(w); n--;
	}
done:
	unskew(x_coord(w),y_coord(w),octant);
}

// 491
void offset_prep(pointer c, pointer h)
{
	halfword n; //{the number of pen offsets}
	pointer p, q, r, lh, ww = max_halfword; //{for list manipulation}
	halfword k; //{the current offset index}
	pointer w; //{a pointer to offset $w_k$}

	#pragma region <Other local variables for |offset_prep|>
	int x0, x1, x2, y0, y1, y2; //{representatives of derivatives}
	int t0, t1 = INT_MAX, t2 = INT_MAX; //{coefficients of polynomial for slope testing}
	int du, dv, dx, dy; //{for slopes of the pen and the curve}
	int max_coef; //{used while scaling}
	int x0a, x1a, x2a, y0a, y1a, y2a; //{intermediate values}
	fraction t; //{where the derivative passes through zero}
	fraction s; //{slope or reciprocal slope}
		
	#pragma endregion

	p = c; n = info(h); lh = link(h); //{now |lh| points to $w_0$}
	while (right_type(p) != endpoint)
	{
		q = link(p);
		#pragma region <Split the cubic between |p| and |q|, if necessary, into cubics associated with single offsets, after which |q| should point to the end of the final such cubic>
		if (n <= 1) right_type(p) = 1; //{this case is easy}
		else { 
			#pragma region <Prepare for derivative computations; |goto not_found| if the current cubic is dead>
			x0 = right_x(p) - x_coord(p); //{should be |>=0|}
			x2 = x_coord(q) - left_x(q); //{likewise}
			x1 = left_x(q) - right_x(p); //{but this might be negative}
			y0 = right_y(p) - y_coord(p); y2 = y_coord(q) - left_y(q);
			y1 = left_y(q) - right_y(p);
			max_coef = myabs(x0); //{we take |abs| just to make sure}
			if (myabs(x1) > max_coef) max_coef = myabs(x1);
			if (myabs(x2) > max_coef) max_coef = myabs(x2);
			if (myabs(y0) > max_coef) max_coef = myabs(y0);
			if (myabs(y1) > max_coef) max_coef = myabs(y1);
			if (myabs(y2) > max_coef) max_coef = myabs(y2);
			if (max_coef == 0) goto not_found;
			while (max_coef < fraction_half)
			{
				_double(max_coef);
				_double(x0); _double(x1); _double(x2);
				_double(y0); _double(y1); _double(y2);
			}			
			#pragma endregion
			
			#pragma region <Find the initial slope, |dy/dx|>
			dx = x0; dy = y0;
			if (dx == 0)
				if (dy == 0)
				{ 
					dx = x1; dy = y1;
					if (dx == 0) 
						if (dy == 0)
						{ 
							dx = x2; dy = y2;
						}
				}
			#pragma endregion
			
			if (dx == 0) 
				#pragma region <Handle the special case of infinite slope>
				fin_offset_prep(p,n,knil(knil(lh)),-x0,-x1,-x2,-y0,-y1,-y2,false,n);
				#pragma endregion
		else { 
			#pragma region <Find the index |k| such that $s_{k-1}L{dy}/{dx}<s_k$>
			k = 1; w = link(lh);
			while (true) { 
				if (k == n) goto done;
				ww = link(w);
				if (ab_vs_cd(dy,myabs(x_coord(ww) - x_coord(w)),dx,myabs(y_coord(ww) - y_coord(w))) >= 0)
				{ 
					k++; w = ww;
				}
				else goto done;
			}
		done:			
			#pragma endregion
			
			#pragma region <Complete the offset splitting process>
			if (k == 1) t = fraction_one + 1;
			else { 
				ww = knil(w); 
				#pragma region <Compute test coeff...>
				du = x_coord(ww) - x_coord(w); dv = y_coord(ww) - y_coord(w);
				if (myabs(du) >= myabs(dv)) //{$s_{k-1}\le1$ or $s_k\le1$}
				{ 
					s = make_fraction(dv,du);
					t0 = take_fraction(x0,s) - y0;
					t1 = take_fraction(x1,s) - y1;
					t2 = take_fraction(x2,s) - y2;
				}
				else { 
					s = make_fraction(du,dv);
					t0 = x0 - take_fraction(y0,s);
					t1 = x1 - take_fraction(y1,s);
					t2 = x2 - take_fraction(y2,s);
				}		
				#pragma endregion
				t = crossing_point(-t0,-t1,-t2);
			}
			if (t >= fraction_one) fin_offset_prep(p,k,w,x0,x1,x2,y0,y1,y2,true,n);
			else { 
				split_for_offset(p,t); r = link(p);
				x1a = t_of_the_way(x0,x1); x1 = t_of_the_way(x1,x2);
				x2a = t_of_the_way(x1a,x1);
				y1a = t_of_the_way(y0,y1); y1 = t_of_the_way(y1,y2);
				y2a = t_of_the_way(y1a,y1);
				fin_offset_prep(p,k,w,x0,x1a,x2a,y0,y1a,y2a,true,n); x0 = x2a; y0 = y2a;
				t1 = t_of_the_way(t1,t2);
				if (t1 < 0) t1 = 0;
				t = crossing_point(0,t1,t2);
				if (t < fraction_one)
					#pragma region <Split off another |rising| cubic for |fin_offset_prep|>
				{ 
					split_for_offset(r,t);
					x1a = t_of_the_way(x1,x2); x1 = t_of_the_way(x0,x1);
					x0a = t_of_the_way(x1,x1a);
					y1a = t_of_the_way(y1,y2); y1 = t_of_the_way(y0,y1);
					y0a = t_of_the_way(y1,y1a);
					fin_offset_prep(link(r),k,w,x0a,x1a,x2,y0a,y1a,y2,true,n);
					x2 = x0a; y2 = y0a;
				}				
					#pragma endregion
				fin_offset_prep(r,k-1,ww,-x0,-x1,-x2,-y0,-y1,-y2,false,n);
			}			
			#pragma endregion
		}
	not_found:
			;
		}
		#pragma endregion
		
		#pragma region <Advance |p| to node |q|, removing any dead cubics that might have been introduced by the splitting process>
		do {
			r = link(p);
			if (x_coord(p) == right_x(p))
				if (y_coord(p) == right_y(p))
					if (x_coord(p) == left_x(r))
						if (y_coord(p) == left_y(r))
							if (x_coord(p) == x_coord(r))
								if (y_coord(p) == y_coord(r))
								{ 
									remove_cubic(p);
									if (r == q) q = p;
									r = p;
								}
			p = r;
		} while (!(p == q));		
		#pragma endregion
	}
}

// 493
void split_for_offset(pointer p, fraction t)
{
	pointer q; //{the successor of p}
	pointer r; //{the new node}
	q = link(p); split_cubic(p,t,x_coord(q),y_coord(q)); r = link(p);
	if (y_coord(r) < y_coord(p)) y_coord(r) = y_coord(p);
	else if (y_coord(r) > y_coord(q)) y_coord(r) = y_coord(q);
	if (x_coord(r) < x_coord(p)) x_coord(r) = x_coord(p);
	else if (x_coord(r) > x_coord(q)) x_coord(r) = x_coord(q);
}


quarterword halfword_to_quarterword(halfword n)
{
	if (n > max_quarterword || n < min_quarterword) {
		fprintf(stderr, "Overflow half_word_to_quarterword\n");
		exit(1);
	}
	return (quarterword)n;
}

eight_bits halfword_to_eight_bits(halfword n)
{
	if (n > 255 || n < 0)
	{
		fprintf(stderr, "Overflow halfword_to_eight_bits\n");
		exit(1);
	}
	return (eight_bits)n;
}


// 497
void fin_offset_prep(pointer p, halfword k, pointer w, int x0, int x1, int x2, int y0, int y1, int y2,
					bool rising, int n)
{
	pointer ww; //{for list manipulation}
	scaled du,dv; //{for slope calculation}
	int t0,t1,t2; //{test coefficients}
	fraction t; //{place where the derivative passes a critical slope}
	fraction s; //{slope or reciprocal slope}
	int v; //{intermediate value for updating x0 .. y2 }

	while (true)
	{ 
		right_type(p) = halfword_to_quarterword(k);
		if (rising)
			if (k == n) return;
			else ww = link(w); //{a pointer to w k+1 }
		else if (k == 1) return;
		else ww = knil(w); //{a pointer to w k-1 }
		#pragma region <Compute test coefficients (t0,t1,t2) for s(t) versus s k or s k-1 498>
		du = x_coord(ww) - x_coord(w); dv = y_coord(ww) - y_coord(w);
		if (myabs(du) >= myabs(dv)) //{s k-1 = 1 or s k = 1}
		{ 
			s = make_fraction(dv,du); t0 = take_fraction(x0,s) - y0; t1 = take_fraction(x1,s) - y1;
			t2 = take_fraction(x2,s) - y2;
		}
		else { 
			s = make_fraction(du,dv); t0 = x0 - take_fraction(y0,s); t1 = x1 - take_fraction(y1,s);
			t2 = x2 - take_fraction(y2,s);
		}
		#pragma endregion
		t = crossing_point(t0,t1,t2);
		if (t >= fraction_one) return;
		#pragma region <Split the cubic at t, and split off another cubic if (the derivative crosses back 499>
		{ 
			split_for_offset(p,t); right_type(p) = halfword_to_quarterword(k); p = link(p);
			v = t_of_the_way(x0,x1); x1 = t_of_the_way(x1,x2); x0 = t_of_the_way(v,x1);
			v = t_of_the_way(y0,y1); y1 = t_of_the_way(y1,y2); y0 = t_of_the_way(v,y1);
			t1 = t_of_the_way(t1,t2);
			if (t1 > 0) t1 = 0; //{without rounding error, t1 would be = 0}
			t = crossing_point(0,-t1,-t2);
			if (t < fraction_one)
			{ 
				split_for_offset(p,t); right_type(link(p)) = (quarterword)k;
				v = t_of_the_way(x1,x2); x1 = t_of_the_way(x0,x1); x2 = t_of_the_way(x1,v);
				v = t_of_the_way(y1,y2); y1 = t_of_the_way(y0,y1); y2 = t_of_the_way(y1,v);
			}
		}
		#pragma endregion
		if (rising) k++; else k--;
		w = ww;
	}
}


// 506
void fill_envelope(pointer spec_head)
{
	pointer p, q, r, s; //{for list traversal}
	pointer h; //{head of pen offset list for current octant}
	pointer www; //{a pen offset of temporary interest}

	#pragma region <Other local variables for |fill_envelope|>
	int m,n; //{current lattice position}
	int mm0,mm1; //{skewed equivalents of |m0| and |m1|}
	int k; //{current offset number}
	pointer w, ww; //{pointers to the current offset and its neighbor}
	int smooth_bot = INT_MAX, smooth_top = INT_MAX; //:0..move_size; //{boundaries of smoothing}
	scaled xx, yy, xp, yp, delx, dely, tx, ty;
	//{registers for coordinate calculations}
	#pragma endregion
	
	
	if (internal[tracing_edges] > 0) begin_edge_tracing();
	p = spec_head; //{we assume that |left_type(spec_head)=endpoint|}
	do {
		octant = left_octant(p); h = cur_pen + octant;
		#pragma region <Set variable |q| to the node at the end of the current octant>
		q = p;
		while (right_type(q) != endpoint) q = link(q);
		#pragma endregion


		#pragma region <Determine the envelopes starting and ending lattice points |(m0,n0)| and |(m1,n1)|>
		w = link(h); if (left_transition(p) == diagonal) w = knil(w);
#ifndef NO_STAT 
		if (internal[tracing_edges] > unity)
			#pragma region <Print a line of diagnostic info to introduce this octant>
		{ 
			print_nl(/*@ Octant */501); print(octant_dir[octant]);
			print(/* (*/479); print_int(info(h)); print(/* offset*/502);
			if (info(h) != 1) print_char(/*s*/115);
			print(/*), from */503);
			print_two_true(x_coord(p)+x_coord(w),y_coord(p)+y_coord(w));
			ww = link(h);
			if (right_transition(q) == diagonal) ww = knil(ww);
			print(/* to */504);
			print_two_true(x_coord(q) + x_coord(ww),y_coord(q) + y_coord(ww));
		}		
			#pragma endregion 
#endif
		ww = link(h); www = ww; //{starting and ending offsets}
		if (myodd(octant_number[octant])) www = knil(www); else ww = knil(ww);
		if (w != ww) skew_line_edges(p,w,ww);
		end_round(x_coord(p) + x_coord(ww),y_coord(p) + y_coord(ww));
		m0 = m1; n0 = n1; d0 = d1;
		end_round(x_coord(q) + x_coord(www),y_coord(q) + y_coord(www));
		if (n1 - n0 >= move_size) overflow(/*move table size*/469,move_size);
		#pragma endregion
		offset_prep(p,h); //{this may clobber node~|q|, if it becomes ``dead''}
		#pragma region <Set variable |q| to the node at the end of the current octant>
		q = p;
		while (right_type(q) != endpoint) q = link(q);
		#pragma endregion

		
		#pragma region <Make the envelope moves for the current octant and insert them in the pixel data>
		if (myodd(octant_number[octant]))
		{
			#pragma region <Initialize for ordinary envelope moves>
			k = 0; w = link(h); ww = knil(w);
			mm0 = floor_unscaled(x_coord(p) + x_coord(w) - xy_corr[octant]);
			mm1 = floor_unscaled(x_coord(q) + x_coord(ww) - xy_corr[octant]);
			for (n = 0; n <= n1 - n0; n++) env_move[n] = mm0;
			env_move[n1 - n0] = mm1; move_ptr = 0; m = mm0;
			#pragma endregion
			
			r = p; right_type(q) = info(h) + 1;
			while (true) { 
				if (r == q) smooth_top = move_ptr;
				while (right_type(r) != k)
					#pragma region <Insert a line segment to approach the correct offset>
				
				{ 
					xx = x_coord(r) + x_coord(w); yy = y_coord(r) + y_coord(w) + half_unit;
#ifndef NO_STAT
					if (internal[tracing_edges] > unity)
					{ 
						print_nl(/*@ transition line */505); print_int(k); print(/*, from */506);
						print_two_true(xx, yy - half_unit);
					}
#endif
					if (right_type(r) > k)
					{
						k++; w = link(w);
						xp = x_coord(r) + x_coord(w); yp = y_coord(r) + y_coord(w) + half_unit;
						if (yp != yy)
							#pragma region <Record a line segment from |(xx,yy)| to |(xp,yp)| in |env_move|>
						{
							ty = floor_scaled(yy - y_corr[octant]); dely = yp - yy; yy = yy - ty;
							ty = yp - y_corr[octant] - ty;
							if (ty >= unity)
							{ 
								delx = xp - xx; yy = unity - yy;
								while (true)  { 
									tx = take_fraction(delx,make_fraction(yy,dely));
									if (ab_vs_cd(tx,dely,delx,yy) + xy_corr[octant] > 0) tx--;
									m = floor_unscaled(xx+tx);
									if (m > env_move[move_ptr]) env_move[move_ptr] = m;
									ty = ty - unity;
									if (ty < unity) goto done1;
									yy = yy + unity; move_ptr++;
								}
							done1:
								;
							}
						}
							#pragma endregion
					}
					else { 
						k--; w = knil(w);
						xp = x_coord(r) + x_coord(w); yp = y_coord(r) + y_coord(w) + half_unit;
					}
#ifndef NO_STAT
					if (internal[tracing_edges] > unity)
					{ 
						print(/* to */504);
						print_two_true(xp,yp - half_unit);
						print_nl(/**/289);
					}
#endif
					m = floor_unscaled(xp - xy_corr[octant]);
					move_ptr = floor_unscaled(yp - y_corr[octant]) - n0;
					if (m > env_move[move_ptr]) env_move[move_ptr] = m;
				}	
				
					#pragma endregion 
				if (r == p) smooth_bot = move_ptr;
				if (r == q) goto done;
				move[move_ptr] = 1; n = move_ptr; s = link(r);
				make_moves(x_coord(r)+x_coord(w),right_x(r)+x_coord(w),left_x(s)+x_coord(w),
							x_coord(s)+x_coord(w),y_coord(r)+y_coord(w)+half_unit,
							right_y(r)+y_coord(w)+half_unit,left_y(s)+y_coord(w)+half_unit,
							y_coord(s)+y_coord(w)+half_unit,xy_corr[octant],y_corr[octant]);
				#pragma region <Transfer moves from the |move| array to |env_move|>
				do { 
					m = m + move[n] - 1;
					if (m > env_move[n]) env_move[n] = m;
					n++;
				} while (!(n > move_ptr));				
				#pragma endregion 
				r = s;
			}
			done:  
			#pragma region <Insert the new envelope moves in the pixel data>
#ifndef NO_DEBUG
			if (m != mm1 || move_ptr != n1 - n0) confusion(/*1*/49);
#endif
			move[0] = d0 + env_move[0] - mm0;
			for (n = 1; n <= move_ptr; n++)
				move[n] = env_move[n] - env_move[n - 1] + 1;
			move[move_ptr] = move[move_ptr] - d1;
			if (internal[smoothing] > 0) smooth_moves(smooth_bot,smooth_top);
			move_to_edges(m0,n0,m1,n1);
			if (right_transition(q) == axis)
			{ 
				w = link(h); skew_line_edges(q,knil(w),w);
			}
			#pragma endregion
		}
		else dual_moves(h,p,q);
		right_type(q) = endpoint;
		#pragma endregion
		p = link(q);
	} while (!(p == spec_head));
	if (internal[tracing_edges] > 0) end_edge_tracing();
	toss_knot_list(spec_head);
}



// 510
void skew_line_edges(pointer p, pointer w, pointer ww)
{
	scaled x0,y0,x1,y1; //{from and to}

	if (x_coord(w) != x_coord(ww) || y_coord(w) != y_coord(ww))
	{ 
		x0 = x_coord(p) + x_coord(w); y0 = y_coord(p) + y_coord(w);
		x1 = x_coord(p) + x_coord(ww); y1 = y_coord(p) + y_coord(ww);
		unskew(x0,y0,octant); //{unskew and unrotate the coordinates}
		x0 = cur_x; y0 = cur_y;
		unskew(x1,y1,octant);
		#ifndef NO_STAT 
		if (internal[tracing_edges] > unity)
		{ 
			print_nl(/*@ retrograde line from */507); print_two(x0,y0); print(/* to */504);
			print_two(cur_x,cur_y); print_nl(/**/289);
		}
		#endif
		line_edges(x0,y0,cur_x,cur_y); //{then draw a straight line}
	}
}

// 518
void dual_moves(pointer h, pointer p, pointer q)
{
	pointer r, s; //{for list traversal}
	#pragma region <Other local variables for |fill_envelope|>
	int m, n; //{current lattice position}
	int mm0, mm1; //{skewed equivalents of |m0| and |m1|}
	int k; //{current offset number}
	pointer w, ww; //{pointers to the current offset and its neighbor}
	int smooth_bot=INT_MAX, smooth_top=INT_MAX; //:0..move_size; //{boundaries of smoothing}
	scaled xx, yy, xp, yp, delx, dely, tx, ty;
	  //{registers for coordinate calculations}
	#pragma endregion

	#pragma region <Initialize for dual envelope moves>
	k = info(h) + 1; ww = link(h); w = knil(ww);
	mm0 = floor_unscaled(x_coord(p)+x_coord(w)-xy_corr[octant]);
	mm1 = floor_unscaled(x_coord(q)+x_coord(ww)-xy_corr[octant]);
	for (n = 1; n <= n1 - n0 + 1; n++) env_move[n] = mm1;
	env_move[0] = mm0; move_ptr = 0; m = mm0;
	#pragma endregion

	r = p; //{recall that |right_type(q)=endpoint=0| now}
	while (true) {
		if (r == q) smooth_top = move_ptr;
		while (right_type(r) != k)
			#pragma region <Insert a line segment dually to approach the correct offset>
		{ 
			xx = x_coord(r)+x_coord(w); yy = y_coord(r)+y_coord(w)+half_unit;
	#ifndef NO_STAT
			if (internal[tracing_edges] > unity)
			{ 
				print_nl(/*@ transition line */505); print_int(k); print(/*, from */506);
				print_two_true(xx,yy-half_unit);
			}
	#endif
			if (right_type(r) < k)
			{
				k--; w = knil(w);
				xp = x_coord(r)+x_coord(w); yp = y_coord(r)+y_coord(w)+half_unit;
				if (yp != yy)
					#pragma region <Record a line segment from |(xx,yy)| to |(xp,yp)| dually in |env_move|>
				{ 
					ty = floor_scaled(yy-y_corr[octant]); dely = yp-yy; yy = yy-ty;
					ty = yp-y_corr[octant]-ty;
					if (ty >= unity)
					{
						delx = xp-xx; yy = unity-yy;
						while (true) { 
							if (m < env_move[move_ptr]) env_move[move_ptr] = m;
							tx = take_fraction(delx,make_fraction(yy,dely));
							if (ab_vs_cd(tx,dely,delx,yy)+xy_corr[octant] > 0) tx--;
							m = floor_unscaled(xx+tx);
							ty = ty-unity; move_ptr++;
							if (ty < unity) goto done1;
							yy = yy+unity;
						}
					done1:
						if (m < env_move[move_ptr]) env_move[move_ptr] = m;
					}
				}		
					#pragma endregion
			}
			else { 
				k++; w = link(w);
				xp = x_coord(r)+x_coord(w); yp = y_coord(r)+y_coord(w)+half_unit;
			}
	#ifndef NO_STAT
			if (internal[tracing_edges] > unity)
			{ 
				print(/* to */504);
				print_two_true(xp,yp-half_unit);
				print_nl(/**/289);
			}
	#endif
			m = floor_unscaled(xp-xy_corr[octant]);
			move_ptr = floor_unscaled(yp-y_corr[octant])-n0;
			if (m < env_move[move_ptr]) env_move[move_ptr] = m;
		}
			#pragma endregion
		if (r == p) smooth_bot = move_ptr;
		if (r == q) goto done;
		move[move_ptr] = 1; n = move_ptr; s = link(r);
		make_moves(x_coord(r)+x_coord(w),right_x(r)+x_coord(w), left_x(s)+x_coord(w),x_coord(s)+x_coord(w),y_coord(r)+y_coord(w)+half_unit,right_y(r)+y_coord(w)+half_unit,left_y(s)+y_coord(w)+half_unit,y_coord(s)+y_coord(w)+half_unit,xy_corr[octant],y_corr[octant]);
		#pragma region <Transfer moves dually from the |move| array to |env_move|>
		do { 
			if (m < env_move[n]) env_move[n] = m;
			m = m+move[n]-1;
			n++;
		} while(!(n > move_ptr));
		#pragma endregion
		r = s;
	}
done:
	#pragma region <Insert the new envelope moves dually in the pixel data>
#ifndef NO_DEBUG
	if (m != mm1 || move_ptr != n1-n0) confusion(/*2*/50);
#endif
	move[0] = d0+env_move[1]-mm0;
	for (n = 1; n <= move_ptr; n++)
		move[n] = env_move[n+1]-env_move[n]+1;
	move[move_ptr] = move[move_ptr]-d1;
	if (internal[smoothing] > 0) smooth_moves(smooth_bot,smooth_top);
	move_to_edges(m0,n0,m1,n1);
	if (right_transition(q) == diagonal)
	{
		w = link(h); skew_line_edges(q,w,knil(w));
	}
	#pragma endregion
}


// 527
pointer make_ellipse(scaled major_axis, scaled minor_axis, angle theta)
{
	pointer p,q,r,s; //{for list manipulation}
	pointer h; //{head of the constructed knot list}
	int alpha, beta, gamma, delta; //{special points}
	int c, d; //{class numbers}
	int u, v; //{directions}
	bool symmetric; //{should the result be symmetric about the axes?}

	#pragma region <Initialize the ellipse data structure by beginning with directions $(0,-1)$, $(1,0)$, $(0,1)$>

	#pragma region <Calculate integers $alpha$, $beta$, $gamma$ for the vertex coordinates>
	if (major_axis == minor_axis || theta % ninety_deg == 0)
	{
		symmetric = true; alpha = 0;
		if (myodd(theta / ninety_deg))
		{ 
			beta = major_axis; gamma = minor_axis;
			n_sin = fraction_one; n_cos = 0; //{|n_sin| and |n_cos| are used later}
		}
		else { 
			beta = minor_axis; gamma = major_axis; theta = 0;
		} //{|n_sin| and |n_cos| aren't needed in this case}
	}
	else { 
		symmetric = false;
		n_sin_cos(theta); //{set up $|n_sin|=\sin\theta$ and $|n_cos|=\cos\theta$}
		gamma = take_fraction(major_axis,n_sin);
		delta = take_fraction(minor_axis,n_cos);
		beta = pyth_add(gamma,delta);
		alpha = take_fraction(take_fraction(major_axis,make_fraction(gamma,beta)),n_cos)-take_fraction(take_fraction(minor_axis,make_fraction(delta,beta)),n_sin);
		alpha = (alpha + half_unit) / unity;
		gamma = pyth_add(take_fraction(major_axis,n_cos),take_fraction(minor_axis,n_sin));
	}
	beta = (beta+half_unit) / unity;
	gamma = (gamma+half_unit) / unity;
	#pragma endregion

	p = get_node(knot_node_size); q = get_node(knot_node_size);
	r = get_node(knot_node_size);
	if (symmetric) s = null; else s = get_node(knot_node_size);
	h = p; link(p) = q; link(q) = r; link(r) = s; //{|s=null| or |link(s)=null|}
	#pragma region <Revise the values of $alpha$, $beta$, $gamma$, if necessary, so that degenerate lines of length zero will not be obtained>
	if (beta == 0) beta = 1;
	if (gamma == 0) gamma = 1;
	if (gamma <= myabs(alpha))
	if (alpha > 0) alpha = gamma - 1;
	else alpha = 1 - gamma;

	#pragma endregion
	x_coord(p) = -alpha*half_unit;
	y_coord(p) = -beta*half_unit;
	x_coord(q) = gamma*half_unit;
	y_coord(q) = y_coord(p); x_coord(r) = x_coord(q);
	right_u(p) = 0; left_v(q) = -half_unit;
	right_u(q) = half_unit; left_v(r) = 0;
	right_u(r) = 0;
	right_class(p) = beta; right_class(q) = gamma; right_class(r) = beta;
	left_length(q) = gamma+alpha;
	if (symmetric)
	{ 
		y_coord(r) = 0; left_length(r) = beta;
	}
	else { 
		y_coord(r) = -y_coord(p); left_length(r) = beta+beta;
		x_coord(s) = -x_coord(p); y_coord(s) = y_coord(r);
		left_v(s) = half_unit; left_length(s) = gamma-alpha;
	}
	#pragma endregion
	#pragma region <Interpolate new vertices in the ellipse data structure until improvement is impossible>
	while (true) {
		u = right_u(p)+right_u(q); v = left_v(q)+left_v(r);
		c = right_class(p)+right_class(q);
		#pragma region <Compute the distance |d| from class~0 to the edge of the ellipse in direction |(u,v)|, times $psqrt{u^2+v^2}$, rounded to the nearest integer>
		delta = pyth_add(u,v);
		if (major_axis == minor_axis) d = major_axis; //{circles are easy}
		else {
			if (theta == 0) { 
				alpha = u; beta = v;
			}
			else {
				alpha = take_fraction(u,n_cos)+take_fraction(v,n_sin);
				beta = take_fraction(v,n_cos)-take_fraction(u,n_sin);
			}
			alpha = make_fraction(alpha,delta);
			beta = make_fraction(beta,delta);
			d = pyth_add(take_fraction(major_axis,alpha),
			take_fraction(minor_axis,beta));
		}
		alpha = myabs(u); beta = myabs(v);
		if (alpha < beta)
		{ 
			alpha = myabs(v); beta = myabs(u);
		} //{now $\alpha=\max(\vert u\vert,\vert v\vert)$, $\beta=\min(\vert u\vert,\vert v\vert)$}
		if (internal[fillin] != 0)
			d = d - take_fraction(internal[fillin],make_fraction(beta+beta,delta));
		d = take_fraction((d + 4) / 8,delta); alpha = alpha / half_unit;
		if (d < alpha) d = alpha;
		#pragma endregion
		delta = c-d; //{we want to move |delta| steps back from the intersection vertex~|q|}
		if (delta > 0)
		{
			if (delta > left_length(r)) delta = left_length(r);
			if (delta >= left_length(q))
			#pragma region <Remove the line from |p| to |q|, and adjust vertex~|q| to introduce a new line>
			{ 
				delta = left_length(q);
				right_class(p) = c - delta; right_u(p) = u; left_v(q) = v;
				x_coord(q) = x_coord(q) - delta * left_v(r);
				y_coord(q) = y_coord(q) + delta * right_u(q);
				left_length(r) = left_length(r)-delta;
			}
			#pragma endregion
			else 
				#pragma region <Insert a new line for direction |(u,v)| between |p| and~|q|>
			{ 
				s = get_node(knot_node_size); link(p) = s; link(s) = q;
				x_coord(s) = x_coord(q) + delta * left_v(q);
				y_coord(s) = y_coord(q) - delta * right_u(p);
				x_coord(q) = x_coord(q) - delta * left_v(r);
				y_coord(q) = y_coord(q) + delta * right_u(q);
				left_v(s) = left_v(q); right_u(s) = u; left_v(q) = v;
				right_class(s) = c - delta;
				left_length(s) = left_length(q)-delta; left_length(q) = delta;
				left_length(r) = left_length(r)-delta;
			}
				#pragma endregion
		}
		else p = q;
		#pragma region <Move to the next remaining triple |(p,q,r)|, removing and skipping past zero-length lines that might be present; |goto done| if all triples have been processed>
		while (true) { 
			q = link(p);
			if (q == null) goto done;
			if (left_length(q) == 0)
			{ 
				link(p) = link(q); right_class(p) = right_class(q);
				right_u(p) = right_u(q); free_node(q,knot_node_size);
			}
			else { 
				r = link(q);
				if (r == null) goto done;
				if (left_length(r) == 0)
				{ 
					link(p) = r; free_node(q,knot_node_size); p = r;
				}
				else goto found;
			}
		}
		found:
			;
		#pragma endregion
	}
	done:
	#pragma endregion
	if (symmetric)
		#pragma region <Complete the half ellipse by reflecting the quarter already computed>
	{ 
		s = null; q = h;
		while (true) {
			r = get_node(knot_node_size); link(r) = s; s = r;
			x_coord(s) = x_coord(q); y_coord(s) = -y_coord(q);
			if (q == p) goto done1;
			q = link(q);
			if (y_coord(q) == 0) goto done1;
		}
		done1: 
		if (link(p) != null) free_node(link(p),knot_node_size);
		link(p) = s; beta = -y_coord(h);
		while (y_coord(p) != beta) p = link(p);
		q = link(p);
	}
		#pragma endregion

	#pragma region <Complete the ellipse by copying the negative of the half already computed>
	if (q != null)
	{ 
		if (right_u(h) == 0)
		{ 
			p = h; h = link(h); free_node(p,knot_node_size);
			x_coord(q) = -x_coord(h);
		}
		p = q;
	}
	else q = p;
	r = link(h); //{now |p=q|, |x_coord(p)=-x_coord(h)|, |y_coord(p)=-y_coord(h)|}
	do {
		s = get_node(knot_node_size); link(p) = s; p = s;
		x_coord(p) = -x_coord(r); y_coord(p) = -y_coord(r); r = link(r);
	} while (!(r == q));
	link(p) = h;
	#pragma endregion
	return h;
}


// 539
scaled find_direction_time(scaled x, scaled y, pointer h)
{
	scaled max; //{$\max\bigl(\vert x\vert,\vert y\vert\bigr)$}
	pointer p, q; //{for list traversal}
	scaled n; //{the direction time at knot |p|}
	scaled tt; //{the direction time within a cubic}

	#pragma region <Other local variables for |find_direction_time|>
	scaled x1, x2, x3, y1, y2, y3; //{multiples of rotated derivatives}
	angle theta, phi=INT_MAX; //{angles of exit and entry at a knot}
	fraction t; //{temp storage}	
	#pragma endregion

	#pragma region <Normalize the given direction for better accuracy; but |return| with zero result if its zero>
	if (myabs(x) < myabs(y))
	{ 
		x = make_fraction(x,myabs(y));
		if (y > 0) y = fraction_one; else y = -fraction_one;
	}
	else if (x == 0)
	{ 
		return 0;
	}
	else { 
		y = make_fraction(y,myabs(x));
		if (x > 0) x = fraction_one; else x = -fraction_one;
	}
	#pragma endregion
	n = 0; p = h;
	while (true) { 
		if (right_type(p) == endpoint) goto not_found;
		q = link(p);
		#pragma region <Rotate the cubic between |p| and |q|; then|goto found| if the rotated cubic travels due east at some time |tt|;but |goto not_found| if an entire cyclic path has been traversed>
		tt = 0;
		#pragma region <Set local variables |x1,x2,x3| and |y1,y2,y3| to multiples of the control points of the rotated derivatives>
		x1 = right_x(p)-x_coord(p); x2 = left_x(q)-right_x(p);
		x3 = x_coord(q)-left_x(q);
		y1 = right_y(p)-y_coord(p); y2 = left_y(q)-right_y(p);
		y3 = y_coord(q)-left_y(q);
		max = myabs(x1);
		if (myabs(x2) > max) max = myabs(x2);
		if (myabs(x3) > max) max = myabs(x3);
		if (myabs(y1) > max) max = myabs(y1);
		if (myabs(y2) > max) max = myabs(y2);
		if (myabs(y3) > max) max = myabs(y3);
		if (max == 0) goto found;
		while (max < fraction_half)
		{ 
			_double(max); _double(x1); _double(x2); _double(x3);
			_double(y1); _double(y2); _double(y3);
		}
		t = x1; x1 = take_fraction(x1,x)+take_fraction(y1,y);
		y1 = take_fraction(y1,x)-take_fraction(t,y);
		t = x2; x2 = take_fraction(x2,x)+take_fraction(y2,y);
		y2 = take_fraction(y2,x)-take_fraction(t,y);
		t = x3; x3 = take_fraction(x3,x)+take_fraction(y3,y);
		y3 = take_fraction(y3,x)-take_fraction(t,y);
			
		#pragma endregion
		if (y1 == 0) 
			if (x1 >= 0) goto found;
		if (n > 0)
		{ 
			#pragma region <Exit to |found| if an eastward direction occurs at knot |p|>
			theta = n_arg(x1,y1);
			if (theta >= 0) 
				if (phi <= 0) 
					if (phi >= theta-one_eighty_deg) goto found;
			if (theta <= 0) 
				if (phi >= 0) 
					if (phi <= theta+one_eighty_deg) goto found;
			#pragma endregion
			if (p == h) goto not_found;
		}
		if (x3 != 0 || y3 != 0) phi = n_arg(x3,y3);
		#pragma region <Exit to |found| if the curve whose derivatives are specified by |x1,x2,x3,y1,y2,y3| travels eastward at some time~|tt|>
		if (x1 < 0) 
			if (x2 < 0) 
				if (x3 < 0) goto done;
		if (ab_vs_cd(y1,y3,y2,y2) == 0)
		#pragma region <Handle the test for eastward directions when $y_1y_3=y_2^2$; either |goto found| or |goto done|>
		{ 
			if (ab_vs_cd(y1,y2,0,0) < 0)
			{ 
				t = make_fraction(y1,y1-y2);
				x1 = t_of_the_way(x1,x2);
				x2 = t_of_the_way(x2,x3);
				if (t_of_the_way(x1,x2) >= 0) we_found_it;
			}
			else if (y3 == 0)
				if (y1 == 0)
					#pragma region <Exit to |found| if the derivative $B(x_1,x_2,x_3;t)$ becomes |>=0|>
				{ 
					t = crossing_point(-x1,-x2,-x3);
					if (t <= fraction_one) we_found_it;
					if (ab_vs_cd(x1,x3,x2,x2) <= 0)
					{ 
						t = make_fraction(x1,x1-x2); we_found_it;
					}
				}
					#pragma endregion
				else if (x3 >= 0)
				{ 
					tt = unity; goto found;
				}
			goto done;
		}
		#pragma endregion
		if (y1 <= 0)
			if (y1 < 0)
			{ 
				y1 = -y1; y2 = -y2; y3 = -y3;
			}
			else if (y2 > 0)
			{ 
				y2 = -y2; y3 = -y3;
			}
		#pragma region <Check the places where $B(y_1,y_2,y_3;t)=0$ to see if $B(x_1,x_2,x_3;t)ge0$>
		t = crossing_point(y1,y2,y3);
		if (t > fraction_one) goto done;
		y2 = t_of_the_way(y2,y3);
		x1 = t_of_the_way(x1,x2);
		x2 = t_of_the_way(x2,x3);
		x1 = t_of_the_way(x1,x2);
		if (x1 >= 0) we_found_it;
		if (y2 > 0) y2 = 0;
		tt = t; t = crossing_point(0,-y2,-y3);
		if (t > fraction_one) goto done;
		x1 = t_of_the_way(x1,x2);
		x2 = t_of_the_way(x2,x3);
		if (t_of_the_way(x1,x2) >= 0)
		{ 
			t = t_of_the_way(tt,fraction_one); we_found_it;
		}	
		#pragma endregion
	done:
		#pragma endregion
		
		#pragma endregion
		p = q; n = n+unity;
	}
not_found: 
	return -unity; 
found: 
	return n+tt;
}

// 556
void cubic_intersection(pointer p, pointer pp)
{
	pointer q,qq; //{link(p), link(pp)}

	time_to_go = max_patience; max_t = 2; 
	#pragma region <Initialize for intersections at level zero 558>
	q = link(p); qq = link(pp); bisect_ptr = int_packets;
	u1r = right_x(p) - x_coord(p); u2r = left_x(q) - right_x(p); u3r = x_coord(q) - left_x(q);
	set_min_max(ur_packet);
	v1r = right_y(p) - y_coord(p); v2r = left_y(q) - right_y(p); v3r = y_coord(q) - left_y(q);
	set_min_max(vr_packet);
	x1r = right_x(pp) - x_coord(pp); x2r = left_x(qq) - right_x(pp); x3r = x_coord(qq) - left_x(qq);
	set_min_max(xr_packet);
	y1r = right_y(pp) - y_coord(pp); y2r = left_y(qq) - right_y(pp); y3r = y_coord(qq) - left_y(qq);
	set_min_max(yr_packet);
	delx = x_coord(p) - x_coord(pp); dely = y_coord(p) - y_coord(pp);
	tol = 0; uv = r_packets; xy = r_packets; three_l = 0; cur_t = 1; cur_tt = 1;
	#pragma endregion
	while (true) { 
		mycontinue: if (delx - tol <= stack_max(x_packet(xy)) - stack_min(u_packet(uv)))
		if (delx + tol >= stack_min(x_packet(xy)) - stack_max(u_packet(uv)))
			if (dely - tol <= stack_max(y_packet(xy)) - stack_min(v_packet(uv)))
				if (dely + tol >= stack_min(y_packet(xy)) - stack_max(v_packet(uv)))
				{ 
					if (cur_t >= max_t)
					{ 
						if (max_t == two) // {we've done 17 bisections}
						{ 
							cur_t = half(cur_t + 1); cur_tt = half(cur_tt + 1); return;
						}
						_double(max_t); appr_t = cur_t; appr_tt = cur_tt;
					}
					#pragma region <Subdivide for a new level of intersection 559>

					stack_dx = delx; stack_dy = dely; stack_tol = tol; stack_uv = uv; stack_xy = xy;
					bisect_ptr = bisect_ptr + int_increment;
					_double(cur_t); _double(cur_tt);
					u1l = stack_1(u_packet(uv)); u3r = stack_3(u_packet(uv)); u2l = half(u1l + stack_2(u_packet(uv)));
					u2r = half (u3r + stack_2(u_packet(uv))); u3l = half (u2l + u2r); u1r = u3l; set_min_max(ul_packet);
					set_min_max(ur_packet);
					v1l = stack_1(v_packet(uv)); v3r = stack_3(v_packet(uv)); v2l = half(v1l + stack_2(v_packet(uv)));
					v2r = half (v3r + stack_2(v_packet(uv))); v3l = half (v2l + v2r); v1r = v3l; set_min_max(vl_packet);
					set_min_max(vr_packet);
					x1l = stack_1(x_packet(xy)); x3r = stack_3(x_packet(xy)); x2l = half (x1l + stack_2(x_packet(xy)));
					x2r = half (x3r + stack_2(x_packet(xy))); x3l = half (x2l + x2r); x1r = x3l; set_min_max(xl_packet);
					set_min_max(xr_packet);
					y1l = stack_1(y_packet(xy)); y3r = stack_3(y_packet(xy)); y2l = half (y1l + stack_2(y_packet(xy)));
					y2r = half (y3r + stack_2(y_packet(xy))); y3l = half (y2l + y2r); y1r = y3l; set_min_max(yl_packet);
					set_min_max(yr_packet);
					uv = l_packets; xy = l_packets; _double(delx); _double(dely);
					tol = tol - three_l + tol_step; _double(tol); three_l = three_l + tol_step;

					#pragma endregion
					goto mycontinue;
				}
		if (time_to_go > 0) time_to_go--;
		else { 
			while (appr_t < unity)
			{ 
				_double(appr_t); _double(appr_tt);
			}
			cur_t = appr_t; cur_tt = appr_tt; return;
		}
		#pragma region <Advance to the next pair (cur_t,cur_tt) 560>

		not_found: 
		if (myodd(cur_tt))
			if (myodd(cur_t))
				#pragma region <Descend to the previous level and goto not found 561>
			{
				cur_t = half(cur_t); cur_tt = half(cur_tt);
				if (cur_t == 0) return;
				bisect_ptr = bisect_ptr - int_increment; three_l = three_l - tol_step; delx = stack_dx; dely = stack_dy;
				tol = stack_tol; uv = stack_uv; xy = stack_xy;
				goto not_found;
			}
				#pragma endregion
			else { 
				cur_t++;
				delx = delx + stack_1(u_packet(uv)) + stack_2(u_packet(uv)) + stack_3(u_packet(uv));
				dely = dely + stack_1(v_packet(uv)) + stack_2(v_packet(uv)) + stack_3(v_packet(uv));
				uv = uv + int_packets; // {switch from l_packets to r_packets }
				cur_tt--; xy = xy - int_packets; // {switch from r_packets to l_packets }
				delx = delx + stack_1(x_packet(xy)) + stack_2(x_packet(xy)) + stack_3(x_packet(xy));
				dely = dely + stack_1(y_packet(xy)) + stack_2(y_packet(xy)) + stack_3(y_packet(xy));
			}
		else { 
			cur_tt++; tol = tol + three_l;
			delx = delx - stack_1(x_packet(xy)) - stack_2(x_packet(xy)) - stack_3(x_packet(xy));
			dely = dely - stack_1(y_packet(xy)) - stack_2(y_packet(xy)) - stack_3(y_packet(xy));
			xy = xy + int_packets; // {switch from l_packets to r_packets }
		}


		#pragma endregion
	}
}


// 562
void path_intersection(pointer h, pointer hh)
{

	pointer p,pp; // {link registers that traverse the given paths}
	int n,nn; // {integer parts of intersection times, minus unity }

	#pragma region <Change one-point paths into dead cycles 563>
	if (right_type(h) == endpoint)
	{ 
		right_x(h) = x_coord(h); left_x(h) = x_coord(h); right_y(h) = y_coord(h);
		left_y(h) = y_coord(h); right_type(h) = _explicit;
	}
	if (right_type(hh) == endpoint)
	{ 
		right_x(hh) = x_coord(hh); left_x(hh) = x_coord(hh); right_y(hh) = y_coord(hh);
		left_y(hh) = y_coord(hh); right_type(hh) = _explicit;
	}
	#pragma endregion

	tol_step = 0;
	do { 
		n = -unity; p = h;
		do { 
			if (right_type(p) != endpoint)
			{ 
				nn = -unity; pp = hh;
				do { 
					if (right_type(pp) != endpoint)
					{ 
						cubic_intersection(p,pp);
						if (cur_t > 0)
						{ 
							cur_t = cur_t + n; cur_tt = cur_tt + nn; return;
						}
					}
					nn = nn + unity; pp = link(pp);
				} while (!( pp == hh));
			}
			n = n + unity; p = link(p);
		} while (!( p == h));
		tol_step = tol_step + 3;
	} while (!( tol_step > 3));
	cur_t = -unity; cur_tt = -unity;

}

// 564
bool init_screen()
{
	//return false;

	threadStarted = CreateEvent(NULL, FALSE, FALSE, NULL);
	bool ret = init_win32_window();
	
	WaitForSingleObject(threadStarted, INFINITE);
	return ret;
}

void update_screen() // {will be called only if |init_screen| returns |true|}
{
	/*
#ifndef NO_INIT
	wlog_ln_s("Calling UPDATESCREEN");
#endif
//{for testing only}
	*/

	InvalidateRect(ghwnd, NULL, TRUE);
}



// 567
void blank_rectangle(screen_col left_col,screen_col right_col, screen_row top_row,screen_row bot_row)
{
/*
	screen_row r;
	screen_col c;
	for (r = top_row; r <= bot_row - 1; r++)
	for (c = left_col; c <= right_col - 1; c++)
	screen_pixel[r][c] = white;
#ifndef NO_INIT

	wlog_cr; //{this will be done only after |init_screen=true|}
	fprintf(log_file, "Calling BLANKRECTANGLE(%d,%d,%d,%d)\n", left_col, right_col, top_row, bot_row);
#endif
*/

	WaitForSingleObject(hMutex, INFINITE);
	HBRUSH newBrush = CreateSolidBrush(white_color);
	HBRUSH hOld = (HBRUSH)SelectObject(bitmap_dc, newBrush);
	
	PatBlt(bitmap_dc, left_col, top_row, right_col-left_col, bot_row-top_row, PATCOPY);
	SelectObject(bitmap_dc, hOld);
	ReleaseMutex(hMutex);
}



// 568
// 568
void paint_row(screen_row r, pixel_color b, trans_spec& a, screen_col n)
{
	
	screen_col k; //{an index into |a|}
	screen_col c; //{an index into |screen_pixel|}
/*
	k = 0; c = a[0];
	do { 
		k++;
		do { 
			screen_pixel[r][c] = b; c++;
		} while (!(c == a[k]));
		b = black - b; //{$|black|\swap|white|$}
	} while (!(k == n));
*/	
#ifndef NO_INIT
	fprintf(log_file, "Calling PAINTROW(%d,%d;", r, b);
	//{this is done only after |init_screen=true|}
	for (k = 0; k <= n; k++)
	{ 
		fprintf(log_file, "%d", a[k]); if (k != n) fprintf(log_file, ",");
	}
	fprintf(log_file, ")\n");
#endif

	int col;
	int *aa = &a[0];
	SelectObject(bitmap_dc, GetStockObject(DC_PEN));
	WaitForSingleObject(hMutex, INFINITE);
	do {
		col = *aa++;
		MoveToEx(bitmap_dc, col, r, NULL);
		SetDCPenColor(bitmap_dc, b==0 ? white_color : black_color);

		LineTo(bitmap_dc, *aa, r);
		b=!b;
	} while (--n > 0);
	SelectObject(bitmap_dc, GetStockObject(NULL_PEN));
	ReleaseMutex(hMutex);
}


// 574
void open_a_window(window_number k,scaled r0, scaled c0,scaled r1,scaled c1,scaled x,scaled y)
{
	int m, n; //{pixel coordinates}
	#pragma region <Adjust the coordinates |(r0,c0)| and |(r1,c1)| so that they lie in the proper range>
	if (r0 < 0) r0 = 0; else r0 = round_unscaled(r0);
	r1 = round_unscaled(r1);
	if (r1 > screen_depth) r1 = screen_depth;
	if (r1 < r0)
		if (r0 > screen_depth) r0 = r1; else r1 = r0;
	if (c0 < 0) c0 = 0; else c0 = round_unscaled(c0);
	c1 = round_unscaled(c1);
	if (c1 > screen_width) c1 = screen_width;
	if (c1 < c0)
		if (c0 > screen_width) c0 = c1; else c1 = c0;	
	#pragma endregion
	window_open[k] = true; window_time[k]++;
	left_col[k] = c0; right_col[k] = c1; top_row[k] = r0; bot_row[k] = r1;
	#pragma region <Compute the offsets between screen coordinates and actual coordinates>
	m = round_unscaled(x); n = round_unscaled(y) - 1;
	m_window[k] = c0 - m; n_window[k] = r0 + n;	
	#pragma endregion
	start_screen;
	if (screen_OK)
	{ 
		blank_rectangle(c0,c1,r0,r1); update_screen();
	}
}





// 577
void disp_edges(window_number k)
{
	pointer p, q; //{for list manipulation}
	bool already_there; //{is a previous incarnation in the window?}
	int r; //{row number}

	#pragma region <Other local variables for |disp_edges|>
	screen_col n; //{the highest active index in |row_transition|}
	int w, ww; //{old and new accumulated weights}
	pixel_color b = INT_MAX; //{status of first pixel in the row transitions}
	int m,mm; //{old and new screen column positions}
	int d; //{edge-and-weight without |min_halfword| compensation}
	int m_adjustment; //{conversion between edge and screen coordinates}
	int right_edge; //{largest edge-and-weight that could affect the window}
	screen_col min_col; //{the smallest screen column number in the window}
		
	#pragma endregion

	if (screen_OK)
		if (left_col[k] < right_col[k]) 
			if (top_row[k] < bot_row[k])
			{
				already_there = false;
				if (last_window(cur_edges) == k)
					if (last_window_time(cur_edges) == window_time[k])
						already_there = true;
				if (!already_there)
					blank_rectangle(left_col[k],right_col[k],top_row[k],bot_row[k]);
				#pragma region <Initialize for the display computations>
				m_adjustment = m_window[k] - m_offset(cur_edges);
				right_edge = 8 * (right_col[k] - m_adjustment);
				min_col = left_col[k];		
				#pragma endregion
				p = link(cur_edges); r = n_window[k] - (n_min(cur_edges) - zero_field);
				while (p != cur_edges && r >= top_row[k])
				{
					if (r < bot_row[k])
					#pragma region <Display the pixels of edge row |p| in screen row |r|>
					{ 
						if (unsorted(p) > _void) sort_edges(p);
						else if (unsorted(p) == _void) if (already_there) goto done;
						unsorted(p) = _void; //{this time we'll paint, but maybe not next time}
						#pragma region <Set up the parameters needed for |paint_row|; but |goto done| if no painting is needed after all>
						n = 0; ww = 0; m = -1; w = 0;
						q = sorted(p); row_transition[0] = min_col;
						while (true) {
							if (q == sentinel) d = right_edge;
							else d = ho(info(q));
							mm = (d / 8) + m_adjustment;
							if (mm != m)
							{ 
								#pragma region <Record a possible transition in column |m|>
								if (w <= 0)
								{
									if (ww > 0) if (m > min_col)
									{ 
										if (n == 0)
											if (already_there)
											{ 
												b = white; n++;
											}
											else b = black;
										else n++;
										row_transition[n] = m;
									}
								}
								else if (ww <= 0)
									if (m > min_col)
									{
										if (n == 0) b = black;
										n++; row_transition[n] = m;
									}
								#pragma endregion
								m = mm; w = ww;
							}
							if (d >= right_edge) goto found;
							ww = ww + (d % 8) - zero_w;
							q = link(q);
						}
						found:
						#pragma region <Wind up the |paint_row| parameter calculation by inserting the final transition; |goto done| if no painting is needed>
						if (already_there || ww > 0)
						{
							if (n == 0)
								if (ww > 0) b = black;
								else b = white;
							n++; row_transition[n] = right_col[k];
						}
						else if (n == 0) goto done;
						#pragma endregion

						#pragma endregion
						paint_row(r,b,row_transition,n);
					done:
							;
					}
					#pragma endregion
					p = link(p); r--;
				}
				update_screen();
				window_time[k]++;
				last_window(cur_edges) = k; last_window_time(cur_edges) = window_time[k];
			}
}


// 589
void print_dependency(pointer p, small_number t)
{
	int v; // a coefficient
	pointer pp, q; // for list manipulations

	pp = p;
	while (true) {
		v = myabs(value(p)); q = info(p);
		if (q == null) {// the constant term
			if (v != 0 || p == pp) {
				if (value(p) > 0)
					if (p != pp) print_char(/*+*/43);
				print_scaled(value(p));
			}
			return;
		}
		#pragma region <Print the coefficient, unless its +-1.0 590>
		if (value(p) < 0) print_char(/*-*/45);
		else if (p != pp) print_char(/*+*/43);
		if (t == dependent) v = round_fraction(v);
		if (v != unity) print_scaled(v);
		#pragma endregion
		if (type(q) != independent) confusion(/*dep*/508);
		print_variable_name(q); v = value(q) % s_scale;
		while (v > 0) {
			print(/**4*/509); v = v - 2;
		}
		p = link(p);
	}
}

// 591
fraction max_coef(pointer p)
{
	fraction x; // the maximum so far

	x = 0;
	while (info(p) != null) {
		if (myabs(value(p)) > x) x = myabs(value(p));
		p = link(p);
	}
	return x;
}


// 594
pointer p_plus_fq(pointer p, int f, pointer q, small_number t, small_number tt)
{
	pointer pp,qq; //{|info(p)| and |info(q)|, respectively}
	pointer r,s; //{for list manipulation}
	int threshold; //{defines a neighborhood of zero}
	int v; //{temporary register}
	if (t == dependent) threshold = fraction_threshold;
	else threshold = scaled_threshold;
	r = temp_head; pp = info(p); qq = info(q);
	while (true) 
		if (pp == qq)
			if (pp == null) goto done;
			else 
				#pragma region <Contribute a term from |p|, plus |f| times the corresponding term from |q|>
			{
				if (tt == dependent) v = value(p) + take_fraction(f,value(q));
				else v = value(p) + take_scaled(f,value(q));
				value(p) = v; s = p; p = link(p);
				if (myabs(v) < threshold) free_node(s,dep_node_size);
				else  { 
					if (myabs(v) >= coef_bound)
						if (watch_coefs)
						{ 
							type(qq) = independent_needing_fix; fix_needed = true;
						}
					link(r) = s; r = s;
				}
				pp = info(p); q = link(q); qq = info(q);
			}	
				#pragma endregion
		else if (value(pp) < value(qq))
			#pragma region <Contribute a term from |q|, multiplied by~|f|>
		{ 
			if (tt == dependent) v = take_fraction(f,value(q));
			else v = take_scaled(f,value(q));
			if (myabs(v) > half(threshold))
			{ 
				s = get_node(dep_node_size); info(s) = qq; value(s) = v;
				if (myabs(v) >= coef_bound) 
					if (watch_coefs)
					{ 
						type(qq) = independent_needing_fix; fix_needed = true;
					}
				link(r) = s; r = s;
			}
			q = link(q); qq = info(q);
		}	
			#pragma endregion 
		else  {
			link(r) = p; r = p; p = link(p); pp = info(p);
		}
done: 
	if (t == dependent)
		value(p) = slow_add(value(p),take_fraction(value(q),f));
	else value(p) = slow_add(value(p),take_scaled(value(q),f));
	
	link(r) = p; dep_final = p; return link(temp_head);
}


// 597
pointer p_plus_q(pointer p, pointer q, small_number t)
{
	pointer pp,qq; //{|info(p)| and |info(q)|, respectively}
	pointer r,s; //{for list manipulation}
	int threshold; //{defines a neighborhood of zero}
	int v; //{temporary register}
	if (t == dependent) threshold = fraction_threshold;
	else threshold = scaled_threshold;
	r = temp_head; pp = info(p); qq = info(q);
	while (true) 
		if (pp == qq)
			if (pp == null) goto done;
			else 
				#pragma region <Contribute a term from |p|, plus the corresponding term from |q|>
			{ 
				v = value(p) + value(q);
				value(p) = v; s = p; p = link(p); pp = info(p);
				if (myabs(v) < threshold) free_node(s,dep_node_size);
				else  {
					if (myabs(v) >= coef_bound) 
						if (watch_coefs)
						{ 
							type(qq) = independent_needing_fix; fix_needed = true;
						}
					link(r) = s; r = s;
				}
				q = link(q); qq = info(q);
			}
				#pragma endregion
		else if (value(pp) < value(qq))
		{ 
			s = get_node(dep_node_size); info(s) = qq; value(s) = value(q);
			q = link(q); qq = info(q); link(r) = s; r = s;
		}
		else  { 
			link(r) = p; r = p; p = link(p); pp = info(p);
		}
	done: value(p) = slow_add(value(p),value(q));
	link(r) = p; dep_final = p; return link(temp_head);
}


// 599
pointer p_times_v(pointer p, int v, small_number t0, small_number t1, bool v_is_scaled)
{
	pointer r, s; //{for list manipulation}
	int w; //{tentative coefficient}
	int threshold;
	bool scaling_down;
	if (t0 != t1) scaling_down = true; else scaling_down = !v_is_scaled;
	if (t1 == dependent) threshold = half_fraction_threshold;
	else threshold = half_scaled_threshold;
	r = temp_head;
	while (info(p) != null)
	{ 
		if (scaling_down) w = take_fraction(v,value(p));
		else w = take_scaled(v,value(p));
		if (myabs(w) <= threshold)
		{ 
			s = link(p); free_node(p,dep_node_size); p = s;
		}
		else { 
			if (myabs(w) >= coef_bound)
			{ 
				fix_needed = true; type(info(p)) = independent_needing_fix;
			}
			link(r) = p; r = p; value(p) = w; p = link(p);
		}
	}
	link(r) = p;
	if (v_is_scaled) value(p) = take_scaled(value(p),v);
	else value(p) = take_fraction(value(p),v);
	return link(temp_head);
}


// 600
pointer p_over_v(pointer p, scaled v, small_number t0, small_number t1)
{
	pointer r,s; //{for list manipulation}
	int w; //{tentative coefficient}
	int threshold;
	bool scaling_down;
	if (t0 != t1) scaling_down = true; else scaling_down = false;
	if (t1 == dependent) threshold = half_fraction_threshold;
	else threshold = half_scaled_threshold;
	r = temp_head;
	while (info(p) != null)
	{ 
		if (scaling_down)
			if (myabs(v) < 02000000) w = make_scaled(value(p),v*010000);
			else w = make_scaled(round_fraction(value(p)),v);
		else w = make_scaled(value(p),v);
		if (myabs(w) <= threshold)
		{ 
			s = link(p); free_node(p,dep_node_size); p = s;
		}
		else { 
			if (myabs(w) >= coef_bound)
			{ 
				fix_needed = true; type(info(p)) = independent_needing_fix;
			}
			link(r) = p; r = p; value(p) = w; p = link(p);
		}
	}
	link(r) = p; value(p) = make_scaled(value(p),v);
	return link(temp_head);
}

// 601
pointer p_with_x_becoming_q(pointer p, pointer x, pointer q, small_number t)
{
	pointer r,s; //{for list manipulation}
	int v; //{coefficient of |x|}
	int sx; //{serial number of |x|}
	s = p; r = temp_head; sx = value(x);
	while (value(info(s)) > sx)
	{ 
		r = s; s = link(s);
	}
	if (info(s) != x) return p;
	else { 
		link(temp_head) = p; link(r) = link(s); v = value(s);
		free_node(s,dep_node_size);
		return p_plus_fq(link(temp_head),v,q,t,dependent);
	}
}


// 602
void val_too_big(scaled x)
{
	if (internal[warning_check] > 0) {
		print_err(/*Value is too large (*/510); print_scaled(x); print_char(/*)*/41);
		help4(/*The equation I just processed has given some variable*/511,
		      /*a value of 4096 or more. Continue and I'll try to cope*/512,
			  /*with that big value; but it might be dangerous.*/513,
			  /*(Set warningcheck:=0 to suppress this message.)*/514); error();
	}
}



// 603
void make_known(pointer p, pointer q)
{
	int t; //:dependent..proto_dependent; {the previous type}
	prev_dep(link(q)) = prev_dep(p);
	link(prev_dep(p)) = link(q); t = type(p);
	type(p) = known; value(p) = value(q); free_node(q,dep_node_size);
	if (myabs(value(p)) >= fraction_one) val_too_big(value(p));
	if (internal[tracing_equations] > 0)
		if (interesting(p))
		{
			begin_diagnostic(); print_nl(/*#### */515);
			print_variable_name(p); print_char(/*=*/61); print_scaled(value(p));
			end_diagnostic(false);
		}
	if (cur_exp == p)
		if (cur_type == t)
		{ 
			cur_type = known; cur_exp = value(p);
			free_node(p,value_node_size);
		}
}

// 604
void fix_dependencies()
{
	pointer p,q,r,s,t; //{list manipulation registers}
	pointer x; //{an independent variable}
	r = link(dep_head); s = null;
	while (r != dep_head)
	{ 
		t = r;
		#pragma region <Run through the dependency list for variable |t|, fixing all nodes, and ending with final link~|q|>
		r = value_loc(t); //{|link(r)=dep_list(t)|}
		while (true) {
			q = link(r); x = info(q);
			if (x == null) goto done;
			if (type(x) <= independent_being_fixed)
			{ 
				if (type(x) < independent_being_fixed)
				{
					p = get_avail(); link(p) = s; s = p;
					info(s) = x; type(x) = independent_being_fixed;
				}
				value(q) = value(q) / 4;
				if (value(q) == 0)
				{ 
					link(r) = link(q); free_node(q,dep_node_size); q = r;
				}
			}
			r = q;
		}
	done:		
		#pragma endregion
		r = link(q);
		if (q == dep_list(t)) make_known(t,q);
	}
	while (s != null)
	{ 
		p = link(s); x = info(s); free_avail(s); s = p;
		type(x) = independent; value(x) = value(x) + 2;
	}
	fix_needed = false;
}


// 606
void new_dep(pointer q, pointer p)
{
	pointer r; //{what used to be the first dependency}
	dep_list(q) = p; prev_dep(q) = dep_head;
	r = link(dep_head); link(dep_final) = r; prev_dep(r) = dep_final;
	link(dep_head) = q;
}

pointer const_dependency(scaled v)
{
	dep_final = get_node(dep_node_size);
	value(dep_final) = v; info(dep_final) = null;
	return dep_final;
}

pointer single_dependency(pointer p)
{
	pointer q; //{the new dependency list}
	int m; //{the number of doublings}
	m = value(p) % s_scale;
	if (m > 28) return const_dependency(0);
	else  {
		q = get_node(dep_node_size);
		value(q) = two_to_the[28 - m]; info(q) = p;
		link(q) = const_dependency(0); return q;
	}
}

pointer copy_dep_list(pointer p)
{
	pointer q; //{the new dependency list}
	q = get_node(dep_node_size); dep_final = q;
	while (true) {
		info(dep_final) = info(p); value(dep_final) = value(p);
		if (info(dep_final) == null) goto done;
		link(dep_final) = get_node(dep_node_size);
		dep_final = link(dep_final); p = link(p);
	}
	done: return q;
}


// 610
void linear_eq(pointer p, small_number t)
{
	pointer q,r,s; //{for link manipulation}
	pointer x; //{the variable that loses its independence}
	int n; //{the number of times |x| had been halved}
	int v; //{the coefficient of |x| in list |p|}
	pointer prev_r; //{lags one step behind |r|}
	pointer final_node; //{the constant term of the new dependency list}
	int w; //{a tentative coefficient}


	#pragma region <Find a node |q| in list |p| whose coefficient |v| is largest>
	q = p; r = link(p); v = value(q);
	while (info(r) != null)
	{
		if (myabs(value(r)) > myabs(v))
		{
			q = r; v = value(r);
		}
		r = link(r);
	}
	#pragma endregion
	x = info(q); n = value(x) % s_scale;
	#pragma region <Divide list |p| by |-v|, removing node |q|>
	s = temp_head; link(s) = p; r = p;
	do {
		if (r == q)
		{ 
			link(s) = link(r); free_node(r,dep_node_size);
		}
		else  {
			w = make_fraction(value(r),v);
			if (myabs(w) <= half_fraction_threshold)
			{
				link(s) = link(r); free_node(r,dep_node_size);
			}
			else {
				value(r) = -w; s = r;
			}
		}
		r = link(s);
	} while (!( info(r) == null));
	
	if (t == proto_dependent) value(r) = -make_scaled(value(r),v);
	else if (v != -fraction_one) value(r) = -make_fraction(value(r),v);
	final_node = r; p = link(temp_head);
	#pragma endregion
	if (internal[tracing_equations] > 0)
		#pragma region <Display the new dependency>
		if (interesting(x))
		{
			begin_diagnostic(); print_nl(/*## */516); print_variable_name(x);
			w = n;
			while (w > 0)
			{
				print(/**4*/509); w = w - 2;
			}
			print_char(/*=*/61); print_dependency(p,dependent); end_diagnostic(false);
		}	
		#pragma endregion
		
	#pragma region <Simplify all existing dependencies by substituting for |x|>
	prev_r = dep_head; r = link(dep_head);
	while (r != dep_head)
	{
		s = dep_list(r); q = p_with_x_becoming_q(s,x,p,type(r));
		if (info(q) == null) make_known(r,q);
		else {
			dep_list(r) = q;
			do {
				q = link(q);
			} while (!(info(q) == null));
			prev_r = q;
		}
		r = link(prev_r);
	}
	#pragma endregion

	#pragma region <Change variable |x| from |independent| to |dependent| or |known|>
	if (n > 0)
		#pragma region <Divide list |p| by $2^n$>
	{
		s = temp_head; link(temp_head) = p; r = p;
		do {
			if (n > 30) w = 0;
			else w = value(r) / two_to_the[n];
			if (myabs(w) <= half_fraction_threshold && info(r) != null)
			{
				link(s) = link(r);
				free_node(r,dep_node_size);
			}
			else {
				value(r) = w; s = r;
			}
			r = link(s);
		} while (!(info(s) == null));
		p = link(temp_head);
	}
		#pragma endregion
	if (info(p) == null)
	{
		type(x) = known;
		value(x) = value(p);
		if (myabs(value(x)) >= fraction_one) val_too_big(value(x));
		free_node(p,dep_node_size);
		if (cur_exp == x)
			if (cur_type == independent)
			{
				cur_exp = value(x); cur_type = known;
				free_node(x,value_node_size);
			}
	}
	else {
		type(x) = dependent; dep_final = final_node; new_dep(x,p);
		if (cur_exp == x)
			if (cur_type == independent) cur_type = dependent;
	}
	#pragma endregion
	if (fix_needed) fix_dependencies();

}

// 619
pointer new_ring_entry(pointer p)
{
	pointer q; // the new capsule node

	q = get_node(value_node_size); name_type(q) = capsule; type(q) = type(p);
	if(value(p) == null) value(q) = p; else value(q) = value(p);
	value(p) = q; return q;
}

// 620
void ring_delete(pointer p)
{
	pointer q;

	q = value(p);
	if (q != null)
		if (q != p) {
			while (value(q) != p) q = value(q);
			value(q) = value(p);
		}
}

// 621
void nonlinear_eq(int v, pointer p, bool flush_p)
{
	small_number t; // the type of ring p
	pointer q, r; // link manipulation registers
	t = type(p) - unknown_tag; q = value(p);
	if (flush_p) type(p) = vacuous; else p = q;
	do {
		r = value(q); type(q) = t;
		switch(t) {
			case boolean_type:
				value(q) = v;
				break;
			case string_type:
				value(q) = v; add_str_ref(v);
				break;
			case pen_type:
				value(q) = v; add_pen_ref(v);
				break;
			case path_type:
				value(q) = copy_path(v);
				break;
			case picture_type:
				value(q) = copy_edges(v);
				break;
		}
		q = r;
	} while(!(q == p));
}

// 622
void ring_merge(pointer p, pointer q)
{
	pointer r; // traverses one list
	r = value(p);
	while (r != p) {
		if (r == q) {
			#pragma region <Exclaim about a redundant equation 623>
			print_err(/*Redundant equation*/517);
			help2(/*I already knew that this equation was true.*/518,
				/*But perhaps no harm has been done; let's continue.*/519);
			put_get_error();
			#pragma endregion

			return;
		}
		r = value(r);
	}
	r = value(p); value(p) = value(q); value(q) = r;
}

// 625
void print_cmd_mod(int c, int m)
{
	switch (c) {
	#pragma region <Cases of print_cmd_mod for symbolic printing of primitives 212>
	case add_to_command: print(/*addto*/520); break;
	case assignment: print(/*:=*/521); break;
	case at_least: print(/*atleast*/453); break;
	case at_token: print(/*at*/522); break;
	case bchar_label: print(/*||:*/523); break;
	case begin_group: print(/*begingroup*/524); break;
	case colon: print(/*:*/58); break;
	case comma: print(/*,*/44); break;
	case controls: print(/*controls*/525); break;
	case cull_command: print(/*cull*/526); break;
	case curl_command: print(/*curl*/527); break;
	case delimiters: print(/*delimiters*/528); break;
	case display_command: print(/*display*/529); break;
	case double_colon: print(/*::*/530); break;
	case end_group: print(/*endgroup*/531); break;
	case every_job_command: print(/*everyjob*/532); break;
	case exit_test: print(/*exitif*/533); break;
	case expand_after: print(/*expandafter*/534); break;
	case from_token: print(/*from*/535); break;
	case in_window: print(/*inwindow*/536); break;
	case interim_command: print(/*interim*/537); break;
	case left_brace: print(/*{*/123); break;
	case left_bracket: print(/*[*/91); break;
	case let_command: print(/*let*/538); break;
	case new_internal: print(/*newinternal*/539); break;
	case of_token: print(/*of*/540); break;
	case open_window: print(/*openwindow*/541); break;
	case path_join: print(/*..*/416); break;
	case random_seed: print(/*randomseed*/542); break;
	case relax: print_char(/*\*/92); break;
	case right_brace: print(/*}*/125); break;
	case right_bracket: print(/*]*/93); break;
	case save_command: print(/*save*/543); break;
	case scan_tokens: print(/*scantokens*/544); break;
	case semicolon: print(/*;*/59); break;
	case ship_out_command: print(/*shipout*/545); break;
	case skip_to: print(/*skipto*/546); break;
	case step_token: print(/*step*/547); break;
	case str_op: print(/*str*/548); break;
	case tension: print(/*tension*/549); break;
	case to_token: print(/*to*/550); break;
	case until_token: print(/*until*/551); break;

	// 684
	case macro_def: 
		if (m <= var_def)
			if (m == start_def) print(/*def*/552);
			else if (m < start_def) print(/*enddef*/553);
			else print(/*vardef*/554);
		else if (m == secondary_primary_macro) print(/*primarydef*/555);
		else if (m == tertiary_secondary_macro) print(/*secondarydef*/556);
		else print(/*tertiarydef*/557);
		break;
	case iteration:
		if (m <= start_forever)
			if (m == start_forever) print(/*forever*/558); else print(/*endfor*/559);
		else if (m == expr_base) print(/*for*/560); else print(/*forsuffixes*/561);
		break;

	// 689
	case macro_special:
		switch(m) {
			case macro_prefix: print(/*#@*/562); break;
			case macro_at: print_char(/*@*/64); break;
			case macro_suffix: print(/*@#*/563); break;
			default: print(/*quote*/564); break;
		}
		break;

	// 696
	case param_type:
		if (m >= expr_base)
			if (m == expr_base) print(/*expr*/565);
			else if (m == suffix_base) print(/*suffix*/566);
			else print(/*text*/567);
		else if (m < secondary_macro) print(/*primary*/568);
		else if (m == secondary_macro) print(/*secondary*/569);
		else print(/*tertiary*/570);
		break;

	// 710
	case input: if (m == 0) print(/*input*/571); else print(/*endinput*/572); break;

	// 741
	case if_test:
	case fi_or_else:
		switch(m) {
			case if_code: print(/*if*/573); break;
			case fi_code: print(/*fi*/574); break;
			case else_code: print(/*else*/575); break;
			default: print(/*elseif*/576); break;
		}
		break;
	
	// 894
	case nullary: case unary: case primary_binary: case secondary_binary: case tertiary_binary:
	case expression_binary: case cycle: case plus_or_minus: case slash: case ampersand:
	case equals: case and_command:
		print_op(m);
		break;

	// 1014
	case type_name: print_type(m); break;

	// 1019
	case stop:
		if (m == 0) print(/*end*/577); else print(/*dump*/578);
		break;

	// 1025
	case mode_command:
		switch(m) {
			case batch_mode: print(/*batchmode*/278); break;
			case nonstop_mode: print(/*nonstopmode*/279); break;
			case scroll_mode: print(/*scrollmode*/280); break;
			default: print(/*errorstopmode*/579); break;
		}
		break;

	// 1028
	case protection_command: if (m == 0) print(/*inner*/580); else print(/*outer*/581); break;


	// 1038
	case show_command:
		switch(m) {
			case show_token_code: print(/*showtoken*/582); break;
			case show_stats_code: print(/*showstats*/583); break;
			case show_code: print(/*show*/584); break;
			case show_var_code: print(/*showvariable*/585); break;
			default: print(/*showdependencies*/586); break;
		}
		break;

	// 1043
	case left_delimiter:
	case right_delimiter:
		if (c == left_delimiter) print(/*lef*/587);
		else print(/*righ*/588);
		print(/*t delimiter that matches */589); slow_print(text(m));
		break;
	case tag_token: if (m == null) print(/*tag*/590); else print(/*variable*/591); break;

	case defined_macro: print(/*macro:*/592); break;

	case secondary_primary_macro: case tertiary_secondary_macro: case expression_tertiary_macro:
		print_cmd_mod(macro_def, c); print(/*'d macro:*/593); print_ln();
		show_token_list(link(link(m)), null, 1000, 0);
		break;

	case repeat_loop: print(/*[repeat the loop]*/594); break;

	case internal_quantity: slow_print(int_name[m]); break;

	// 1053
	case thing_to_add:
		if (m == contour_code) print(/*contour*/595);
		else if (m == double_path_code) print(/*doublepath*/596);
		else print(/*also*/597);
		break;
	case with_option:
		if (m == pen_type) print(/*withpen*/598);
		else print(/*withweight*/599);
		break;
	case cull_op:
		if (m == drop_code) print(/*dropping*/600);
		else print(/*keeping*/601);
		break;

	// 1080
	case message_command:
		if (m < err_message_code) print(/*message*/602);
		else if (m == err_message_code) print(/*errmessage*/603);
		else print(/*errhelp*/604);
		break;

	// 1102
	case tfm_command:
		switch(m) {
			case char_list_code: print(/*charlist*/605); break;
			case lig_table_code: print(/*ligtable*/606); break;
			case extensible_code: print(/*extensible*/607); break;
			case header_byte_code: print(/*headerbyte*/608); break;
			default: print(/*fontdimen*/609); break;
		}
		break;

	// 1109
	case lig_kern_token:
		switch(m) {
			case 0: print(/*=:*/610); break;
			case 1: print(/*=:|*/611); break;
			case 2: print(/*|=:*/612); break;
			case 3: print(/*|=:|*/613); break;
			case 5: print(/*=:|>*/614); break;
			case 6: print(/*|=:>*/615); break;
			case 7: print(/*|=:|>*/616); break;
			case 11: print(/*|=:|>>*/617); break;
			default: print(/*kern*/618); break;
		}
		break;

	// 1180
	case special_command:
		if (m == known) print(/*numspecial*/619);
		else print(/*special*/620);
		break;






	#pragma endregion
	default: print(/*[unknown command code!]*/621); break;
	}
}

// 626
void show_cmd_mod(int c, int m)
{
	begin_diagnostic(); print_nl(/*{*/123); print_cmd_mod(c, m); print_char(/*}*/125); end_diagnostic(false);
}




// 635
void show_context()
{
	int old_setting; // 0..max_selector, saved selector setting
	#pragma region <Local variables for formatting calculations 641>
	int i; // 0..buf_size, index into buffer
	int l; // length of descriptive information on line 1
	int m; // context information gathered for line 2
	int n; // 0..error_line, lenght of line 1
	int p; // starting or ending place in trick_buf
	int q; // temporary index
	#pragma endregion

	file_ptr = input_ptr; input_stack[file_ptr] = cur_input; // store current state
	while (true) {
		cur_input = input_stack[file_ptr]; // enter into the context
		#pragma region <Display the current context 636>
		if (file_ptr == input_ptr || file_state || token_type != backed_up || loc != null) {
			// we omit backed-up token lists that have already been read
			tally = 0; // get ready to count characters
			old_setting = selector;
			if (file_state) {

				#pragma region <Print location of current line 637>
				if (name <= 1)
					if (terminal_input && file_ptr == 0) print_nl(/*<*>*/622);
					else print_nl(/*<insert>*/623);
				else if (name == 2) print_nl(/*<scantokens>*/624);
				else {
					print_nl(/*l.*/625); print_int(line);
				}
				print_char(/* */32);
				#pragma endregion

				#pragma region <Pseudoprint the line 644>
				begin_pseudoprint;
				if (limit > 0)
					for (i = start; i <= limit - 1; i++) {
						if (i == loc) set_trick_count;
						print(buffer[i]);
					}
				#pragma endregion

			}
			else {
				#pragma region <Print type of token list 638>
				switch (token_type) {
				case forever_text: print_nl(/*<forever> */626); break;
				case loop_text:
					#pragma region <Print the current loop value 639>
					print_nl(/*<for(*/627); p = param_stack[param_start];
					if (p != null)
						if (link(p) == _void) print_exp(p, 0); // we're in a for loop
						else show_token_list(p, null, 20, tally);
					print(/*)> */628);
					#pragma endregion
					break;
				case parameter: print_nl(/*<argument> */629); break;
				case backed_up:
					if (loc == null) print_nl(/*<recently read> */630);
					else print_nl(/*<to be read again> */631);
					break;
				case inserted: print_nl(/*<inserted text> */632); break;
				case macro:
					print_ln();
					if (name != null) slow_print(text(name));
					else
						#pragma region <Print the name of a vardefd macro 640>
					{
						p = param_stack[param_start];
						if (p == null) show_token_list(param_stack[param_start + 1], null, 20, tally);
						else {
							q = p;
							while (link(q) != null) q = link(q);
							link(q) = param_stack[param_start + 1]; show_token_list(p, null, 20, tally); link(q) = null;
						}
					}
						#pragma endregion
					print(/*->*/429);
					break;
				default:
					print_nl(/*?*/63); // this should never happen
					break;


				}

				#pragma endregion

				#pragma region <Pseudoprint the token list 645>
				begin_pseudoprint;
				if (token_type != macro) show_token_list(start, loc, 100000, 0);
				else show_macro(start, loc, 100000);
				#pragma endregion
			}
			selector = old_setting; // stop pseudoprinting
			#pragma region <Print two lines using the tricky pseudoprinted information 643>

			if (trick_count == 1000000) set_trick_count; // set_trick_count must be performed
			if (tally < trick_count) m = tally - first_count;
			else m = trick_count - first_count; // context on line 2
			if (l + first_count <= half_error_line) {
				p = 0; n = l + first_count;
			}
			else {
				print(/*...*/281); p = l + first_count - half_error_line + 3; n = half_error_line;
			}
			for (q = p; q <= first_count - 1; q++) print_char(trick_buf[q % error_line]);
			print_ln();
			for (q = 1; q <= n; q++) print_char(/* */32); // print n spaces to begin line 2
			if (m + n <= error_line) p = first_count + m;
			else p = first_count + (error_line - n - 3);
			for (q = first_count; q <= p - 1; q++) print_char(trick_buf[q % error_line]);
			if (m + n > error_line) print(/*...*/281);

			#pragma endregion

		}
		#pragma endregion
		if (file_state)
			if (name > 2 || file_ptr == 0) goto done;
		file_ptr--;
	}
done:
	cur_input = input_stack[input_ptr]; // restore original state
}

// 649
void begin_token_list(pointer p, quarterword t)
{
	push_input;
	start = p; token_type = t; param_start = param_ptr; loc = p;
}

// 650
void end_token_list() //{leave a token-list input level}
{
	pointer p; //{temporary register}
	if (token_type >= backed_up) //{token list to be deleted}
		if (token_type <= inserted)
		{ 
			flush_token_list(start); goto done;
		}
	else delete_mac_ref(start); //{update reference count}
	while (param_ptr > param_start) //{parameters must be flushed}
	{ 
		param_ptr--;
		p = param_stack[param_ptr];
		if (p != null)
			if (link(p) == _void) //{it's an \&{expr} parameter}
			{ 
					recycle_value(p); free_node(p,value_node_size);
			}
			else flush_token_list(p); //{it's a \&{suffix} or \&{text} parameter}
	}
done: 
	pop_input; check_interrupt;
}


// 651
pointer cur_tok()
{
	pointer p; //{a new token node}
	small_number save_type; //{|cur_type| to be restored}
	int save_exp; //{|cur_exp| to be restored}
	if (cur_sym == 0)
		if (cur_cmd == capsule_token)
		{ 
			save_type = cur_type; save_exp = cur_exp;
			make_exp_copy(cur_mod); p = stash_cur_exp(); link(p) = null;
			cur_type = save_type; cur_exp = save_exp;
		}
		else { 
			p = get_node(token_node_size);
			value(p) = cur_mod; name_type(p) = token;
			if (cur_cmd == numeric_token) type(p) = known;
			else type(p) = string_type;
		}
	else { 
		fast_get_avail(p); info(p) = cur_sym;
	}
	return p;
}

// 652
void back_input()
{
	pointer p; // a token list of length one
	p = cur_tok();
	while (token_state && loc == null) end_token_list(); // conserve stack space
	back_list(p);
}

// 653
void back_error() // back up one token and call error
{
	OK_to_interrupt = false; back_input(); OK_to_interrupt = true; error();
}

void ins_error() // back up one inserted token and call error
{
	OK_to_interrupt = false; back_input(); token_type = inserted; OK_to_interrupt = true; error();
}





// 654
void begin_file_reading() // enter a new syntactic level for terminal input
{
	if (in_open == max_in_open) overflow(/*text input levels*/633, max_in_open);
	if (first == buf_size) overflow(/*buffer size*/264, buf_size);
	in_open++; push_input; index = in_open; line_stack[index] = line; start = first; name = 0;
	// terminal_input is now true
}




// 655
void end_file_reading()
{
	first = start; line = line_stack[index];
	if (index != in_open) confusion(/*endinput*/572);
	if (name > 2) a_close(cur_file); // forget it
	pop_input; in_open--;
}


// 656
void clear_for_error_prompt()
{
	while (file_state && terminal_input && input_ptr > 0 && loc == limit) end_file_reading();
	print_ln(); clear_terminal();
}

// 661
bool check_outer_validity()
{
	pointer p; //{points to inserted token list}
	if (scanner_status == normal) return true;
	else { 
		deletions_allowed = false;
		#pragma region <Back up an outer symbolic token so that it can be reread>
		if (cur_sym != 0)
		{ 
			p = get_avail(); info(p) = cur_sym;
			back_list(p); //{prepare to read the symbolic token again}
		}
		#pragma endregion
		if (scanner_status > skipping)
			#pragma region <Tell the user what has run away and try to recover>
		{ 
			runaway(); //{print the definition-so-far}
			if (cur_sym == 0) print_err(/*File ended*/634);
			else { 
				print_err(/*Forbidden token found*/635);
			}
			print(/* while scanning */636);
			help4(/*I suspect you have forgotten an `enddef',*/637,
				/*causing me to read past where you wanted me to stop.*/638,
				/*I'll try to recover; but if the error is serious,*/639,
				/*you'd better type `E' or `X' now and fix your file.*/640);
			switch(scanner_status) {
				#pragma region <Complete the error message,and set |cur_sym| to a token that might help recover from the error>
				case flushing: 
					print(/*to the end of the statement*/641);
					help_line[3] = /*A previous error seems to have propagated,*/642;
					cur_sym = frozen_semicolon;
					break;
				case absorbing: 
					print(/*a text argument*/643);
					help_line[3] = /*It seems that a right delimiter was left out,*/644;
					if (warning_info == 0) cur_sym = frozen_end_group;
					else { 
						cur_sym = frozen_right_delimiter;
						equiv(frozen_right_delimiter) = warning_info;
					}
					break;
				case var_defining: case op_defining: 
					print(/*the definition of */645);
					if (scanner_status == op_defining) slow_print(text(warning_info));
					else print_variable_name(warning_info);
					cur_sym = frozen_end_def;
					break;
				case loop_defining: 
					print(/*the text of a */646); slow_print(text(warning_info));
					print(/* loop*/647);
					help_line[3] = /*I suspect you have forgotten an `endfor',*/648;
					cur_sym = frozen_end_for;
					break;
				#pragma endregion
			} //{there are no other cases}
			ins_error();
		}
			#pragma endregion
		else { 
			print_err(/*Incomplete if; all text was ignored after line */649);
			print_int(warning_info);
			help3(/*A forbidden `outer' token occurred in skipped text.*/650,
				/*This kind of error happens when you say `if...' and forget*/651,
				/*the matching `fi'. I've inserted a `fi'; this might work.*/652);
			if (cur_sym == 0) help_line[2] = /*The file ended while I was skipping conditional text.*/653;
			cur_sym = frozen_fi; ins_error();
		}
		deletions_allowed = true; return false;
	}
}
// 665
void runaway()
{
	if (scanner_status > flushing) {
		print_nl(/*Runaway */654);
		switch (scanner_status) {
		case absorbing:
			print(/*text?*/655);
			break;
		case var_defining: case op_defining:
			print(/*definition?*/656);
			break;
		case loop_defining:
			print(/*loop?*/657);
			break;
		}
		print_ln(); show_token_list(link(hold_head), null, error_line - 10, 0);
	}
}

// 667

void get_next() //{sets |cur_cmd|, |cur_mod|, |cur_sym| to next token}
{
	// label restart, {go here to get the next input token}
	//  exit, {go here when the next input token has been got}
	//  found, {go here when the end of a symbolic token has been found}

	//  switch, {go here to branch on the class of an input character}
	//  start_numeric_token,start_decimal_token,fin_numeric_token,done;
	//    {go here at crucial stages when scanning a number}

	int k; //:0..buf_size; {an index into |buffer|}
	ASCII_code c; //{the current character in the buffer}
	ASCII_code _class; //{its class number}
	int n, f; //{registers for decimal-to-binary conversion}

restart:
	cur_sym = 0;
	if (file_state)
		#pragma region <Input from external file; |goto restart| if no input found, or |return| if a non-symbolic token is found>

	{
_switch: 
		c = buffer[loc]; loc++; _class = char_class[c];
		switch (_class) {
		case digit_class: goto start_numeric_token;
		case period_class:
			_class = char_class[buffer[loc]];
			if (_class > period_class) goto _switch;
			else if (_class < period_class) //{|class=digit_class|}
			{
				n = 0; goto start_decimal_token;
			}
			break;
		case space_class: goto _switch;
		case percent_class:
			#pragma region <Move to next line of file, or |goto restart| if there is no next line>

			if (name > 2)
				#pragma region <Read next line of file into |buffer|, or |goto restart| if the file has ended>
			{
				line++; first = start;
				if (!force_eof)
				{
					if (input_ln(cur_file, true)) //{not end of file}
						firm_up_the_line(); //{this sets |limit|}
					else force_eof = true;
				}
				if (force_eof)
				{
					print_char(/*)*/41); open_parens--;
					update_terminal(); //{show user that file has been read}
					force_eof = false;
					end_file_reading(); //{resume previous level}
					if (check_outer_validity()) goto restart; else goto restart;
				}
				buffer[limit] = /*%*/37; first = limit + 1; loc = start; //{ready to read}
			}
				#pragma endregion
			else {
				if (input_ptr > 0)
					//{text was inserted during error recovery or by \&{scantokens}}
				{
					end_file_reading(); goto restart; //{resume previous level}
				}
				if (selector < log_only) open_log_file();
				if (interaction > nonstop_mode)
				{
					if (limit == start) //{previous line was empty}
						print_nl(/*(Please type a command or say `end')*/658);
					print_ln(); first = start;
					prompt_input(/***/42); //{input on-line into |buffer|}
					limit = last; buffer[limit] = /*%*/37;
					first = limit + 1; loc = start;
				}
				else fatal_error(/**** (job aborted, no legal end found)*/659);
				//{nonstop mode, which is intended for overnight batch processing,
				//never waits for on-line input}
			}

			#pragma endregion
			check_interrupt;
			goto _switch;
			break;
		case string_class:
			#pragma region <Get a string token and |return|>
			if (buffer[loc] == /*"*/34) cur_mod = /**/289;
			else {
				k = loc; buffer[limit + 1] = /*"*/34;
				do {
					loc++;
				} while (!(buffer[loc] == /*"*/34));
				if (loc > limit)
					#pragma region <Decry the missing string delimiter and |goto restart|>
				{
					loc = limit; //{the next character to be read on this line will be |"%"|}
					print_err(/*Incomplete string token has been flushed*/660);
					help3(/*Strings should finish on the same line as they began.*/661,
						/*I've deleted the partial string; you might want to*/662,
						/*insert another by typing, e.g., `I"new string"'.*/663);
					deletions_allowed = false; error(); deletions_allowed = true; goto restart;
				}
					#pragma endregion
				if (loc == k + 1 && length(buffer[k]) == 1) cur_mod = buffer[k];
				else {
					str_room(loc - k);
					do {
						append_char(buffer[k]); k++;
					} while (!(k == loc));
					cur_mod = make_string();
				}
			}
			loc++; cur_cmd = string_token; return;
			#pragma endregion
			break;
		case isolated_classes:
			k = loc - 1; goto found;
			break;
		case invalid_class:
			#pragma region <Decry the invalid character and |goto restart|>
			print_err(/*Text line contains an invalid character*/664);
			help2(/*A funny symbol that I can't read has just been input.*/665,
				/*Continue, and I'll forget that it ever happened.*/666);
			deletions_allowed = false; error(); deletions_allowed = true;
			goto restart;
			#pragma endregion
			break;
		default: //do_nothing //{letters, etc.}
			break;
		}
		k = loc - 1;
		while (char_class[buffer[loc]] == _class) loc++;
		goto found;
	start_numeric_token:
		#pragma region <Get the integer part |n| of a numeric token; set |f = 0| and |goto fin_numeric_token| if there is no decimal point>
		n = c - /*0*/48;
		while (char_class[buffer[loc]] == digit_class)
		{
			if (n < 4096) n = 10 * n + buffer[loc] - /*0*/48;
			loc++;
		}
		if (buffer[loc] == /*.*/46)
			if (char_class[buffer[loc + 1]] == digit_class) goto done;
		f = 0; goto fin_numeric_token;
	done: loc++;
		#pragma endregion
start_decimal_token:
		#pragma region <Get the fraction part |f| of a numeric token>
		k = 0;
		do {
			if (k < 17) //{digits for |k>=17| cannot affect the result}
			{
				dig[k] = buffer[loc] - /*0*/48; k++;
			}
			loc++;
		} while (!(char_class[buffer[loc]] != digit_class));
		f = round_decimals(k);
		if (f == unity)
		{
			n++; f = 0;
		}
		#pragma endregion
fin_numeric_token:
	#pragma region <Pack the numeric and fraction parts of a numeric token and |return|>
	if (n < 4096) cur_mod = n * unity + f;
	else {
		print_err(/*Enormous number has been reduced*/667);
		help2(/*I can't handle numbers bigger than about 4095.99998;*/668,
			/*so I've changed your constant to that maximum amount.*/669);
		deletions_allowed = false; error(); deletions_allowed = true;
		cur_mod = 01777777777;
	}
	cur_cmd = numeric_token; return;
	#pragma endregion
found: cur_sym = id_lookup(k, loc - k);
	}
		#pragma endregion
	else
		#pragma region <Input from token list; |goto restart| if end of list or if a parameter needs to be expanded, or |return| if a non-symbolic token is found>
		if (loc >= hi_mem_min) //{one-word token}
		{
			cur_sym = info(loc); loc = link(loc); //{move to next}
			if (cur_sym >= expr_base)
				if (cur_sym >= suffix_base)
				#pragma region <Insert a suffix or text parameter and |goto restart|>
				{
					if (cur_sym >= text_base) cur_sym = cur_sym - param_size;
					//{|param_size=text_base-suffix_base|}
					begin_token_list(param_stack[param_start + cur_sym - (suffix_base)], parameter);
					goto restart;
				}
				#pragma endregion
				else {
					cur_cmd = capsule_token;
					cur_mod = param_stack[param_start + cur_sym - (expr_base)];
					cur_sym = 0; return;
				}
		}
		else if (loc > null)
			#pragma region <Get a stored numeric or string or capsule token and |return|>
		{
			if (name_type(loc) == token)
			{
				cur_mod = value(loc);
				if (type(loc) == known) cur_cmd = numeric_token;
				else {
					cur_cmd = string_token; add_str_ref(cur_mod);
				}
			}
			else {
				cur_mod = loc; cur_cmd = capsule_token;
			}
			loc = link(loc); return;
		}
			#pragma endregion
		else { //{we are done with this token list}
			end_token_list(); goto restart; //{resume previous level}
		}
	#pragma endregion

	#pragma region <Finish getting the symbolic token in |cur_sym|; |goto restart| if it is illegal>
	cur_cmd = (eight_bits)eq_type(cur_sym); cur_mod = equiv(cur_sym);
	if (cur_cmd >= outer_tag)
		if (check_outer_validity()) cur_cmd = cur_cmd - outer_tag;
		else goto restart;
	#pragma endregion
}



// 682
void firm_up_the_line()
{
	int k; // 0..buf_size, an index into buffer

	limit = last;
	if (internal[pausing] > 0)
		if (interaction > nonstop_mode) {
			wake_up_terminal(); print_ln();
			if (start < limit)
				for (k = start; k <= limit - 1; k++) print(buffer[k]);
			first = limit; prompt_input(/*=>*/670); // wait for user response
			if (last > first) {
				for (k = first; k <= last - 1; k++) // move line dwon in buffer
					buffer[k + start - first] = buffer[k];
				limit = start + last - first;
			}
		}
}

// 685
pointer scan_toks(command_code terminator, pointer subst_list, pointer tail_end, small_number suffix_count)
{
	pointer p; //{tail of the token list being built}
	pointer q; //{temporary for link management}
	int balance; //{left delimiters minus right delimiters}
	p = hold_head; balance = 1; link(hold_head) = null;
	while (true) {
		get_next();
		if (cur_sym > 0)
		{
			#pragma region <Substitute for | cur_sym | , if its on the |subst_list|>
			q = subst_list;
			while (q != null)
			{
				if (info(q) == cur_sym)
				{ 
					cur_sym = value(q); cur_cmd = relax; goto found;
				}
				q = link(q);
			}
		found:
			;
			#pragma endregion
			if (cur_cmd == terminator)
				#pragma region <Adjust the balance; | goto done | if its zero>
				if (cur_mod > 0) balance++;
				else {
					balance--;
					if (balance == 0) goto done;
				}
				#pragma endregion
			else if (cur_cmd == macro_special)
				#pragma region <Handle quoted symbols, ...>
			{
				if (cur_mod == quote) get_next();
				else if (cur_mod <= suffix_count) cur_sym = suffix_base - 1 + cur_mod;
			}
				#pragma endregion
		}
		link(p) = cur_tok(); p = link(p);
	}
done: 
	link(p) = tail_end; flush_node_list(subst_list);
	return link(hold_head);
}


// 691
void get_symbol() // sets cur_sym to a safe symbol
{
restart:
	get_next();

	if (cur_sym == 0 || cur_sym > frozen_inaccessible) {
		print_err(/*Missing symbolic token inserted*/671);

		help3(/*Sorry: You can't redefine a number, string, or expr.*/672,
			/*I've inserted an inaccessible symbol so that your*/673,
			/*definition will be completed without mixing me up too badly.*/674);

		if (cur_sym > 0) help_line[2] = /*Sorry: You can't redefine my error-recovery tokens.*/675;
		else if (cur_cmd == string_token) delete_str_ref(cur_mod);
		cur_sym = frozen_inaccessible; ins_error(); goto restart;
	}
}

// 692
void get_clear_symbol()
{
	get_symbol(); clear_symbol(cur_sym, false);
}

// 693
void check_equals()
{
	if (cur_cmd != equals)
		if (cur_cmd != assignment)
		{ 
			missing_err(/*=*/61);
			help5(/*The next thing in this `def' should have been `=',*/676,
			/*because I've already looked at the definition heading.*/677,
			/*But don't worry; I'll pretend that an equals sign*/678,
			/*was present. Everything from here to `enddef'*/679,
			/*will be the replacement text of this macro.*/680); back_error();
		}
}



// 694
void make_op_def()
{
	command_code m; // {the type of definition}
	pointer p, q, r; //{for list manipulation}
	m = cur_mod;
	get_symbol(); q = get_node(token_node_size); info(q) = cur_sym; value(q) = expr_base;
	get_clear_symbol(); warning_info = cur_sym;
	get_symbol(); p = get_node(token_node_size); info(p) = cur_sym; value(p) = expr_base + 1; link(p) = q;
	get_next(); check_equals();
	scanner_status = op_defining; q = get_avail(); ref_count(q) = null; r = get_avail(); link(q) = r;
	info(r) = general_macro; link(r) = scan_toks(macro_def, p, null, 0); scanner_status = normal;
	eq_type(warning_info) = m; equiv(warning_info) = q; get_x_next();
}

// 697
void scan_def()
{
	int m; // start_def .. var_def, {the type of definition}
	int n; // 0 .. 3,  {the number of special suffix parameters}
	int k; // 0 .. param size, {the total number of parameters}
	int c; // general_macro .. text_macro, {the kind of macro we�re defining}
	pointer r; // {parameter-substitution list}
	pointer q; // {tail of the macro token list}
	pointer p; // {temporary storage}
	halfword base; // {expr base, suffix base, or text base }
	pointer l_delim,r_delim; // {matching delimiters}

	m = cur_mod; c = general_macro; link(hold_head) = null;
	q = get_avail(); ref_count(q) = null; r = null;
	#pragma region <Scan the token or variable to be defined; set n, scanner status, and warning info 700>
	if (m == start_def)
	{ 
		get_clear_symbol(); warning_info = cur_sym; get_next(); scanner_status = op_defining; n = 0;
		eq_type(warning_info) = defined_macro; equiv(warning_info) = q;
	}
	else { 
		p = scan_declared_variable(); flush_variable(equiv(info(p)),link(p),true);
		warning_info = find_variable(p); flush_list(p);
		if (warning_info == null)
			#pragma region <Change to a bad variable 701>
		{
			print_err(/*This variable already starts with a macro*/681);
			help2(/*After `vardef a' you can't say `vardef a.b'.*/682,
			/*So I'll have to discard this definition.*/683); error(); warning_info = bad_vardef;
		}
			#pragma endregion
		scanner_status = var_defining; n = 2;
		if (cur_cmd == macro_special)
			if (cur_mod == macro_suffix) // {@#}
			{ 
				n = 3; get_next();
			}
		type(warning_info) = unsuffixed_macro - 2 + n; value(warning_info) = q;
	} // {suffixed_macro = unsuffixed_macro + 1}

	#pragma endregion
	k = n;
	if (cur_cmd == left_delimiter)
		#pragma region	<Absorb delimited parameters, putting them into lists q and r 703>
	{

		do { 
			l_delim = cur_sym; r_delim = cur_mod; get_next();
			if (cur_cmd == param_type && cur_mod >= expr_base) base = cur_mod;
			else { 
				print_err(/*Missing parameter type; `expr' will be assumed*/684);
				help1(/*You should've had `expr' or `suffix' or `text' here.*/685); back_error();
				base = expr_base;
			}
			#pragma region <Absorb parameter tokens for type base 704>
			do { 
				link(q) = get_avail(); q = link(q); info(q) = base + k;
				get_symbol(); p = get_node(token_node_size); value(p) = base + k; info(p) = cur_sym;
				if (k == param_size) overflow(/*parameter stack size*/686,param_size);
				k++; link(p) = r; r = p; get_next();
			} while (!( cur_cmd != comma));

			#pragma endregion
			check_delimiter(l_delim,r_delim); get_next();
		} while (!( cur_cmd != left_delimiter));


	}
		#pragma endregion
	if (cur_cmd == param_type)
		#pragma region <Absorb undelimited parameters, putting them into list r 705>
	{

		p = get_node(token_node_size);
		if (cur_mod < expr_base)
		{ 
			c = cur_mod; value(p) = expr_base + k;
		}
		else {
			value(p) = cur_mod + k;
			if (cur_mod == expr_base) c = expr_macro;
			else if (cur_mod == suffix_base) c = suffix_macro;
			else c = text_macro;
		}
		if (k == param_size) overflow(/*parameter stack size*/686,param_size);
		k++; get_symbol(); info(p) = cur_sym; link(p) = r; r = p; get_next();
		if (c == expr_macro)
			if (cur_cmd == of_token)
			{ 
				c = of_macro; p = get_node(token_node_size);
				if (k == param_size) overflow(/*parameter stack size*/686,param_size);
				value(p) = expr_base + k; get_symbol(); info(p) = cur_sym; link(p) = r; r = p; get_next();
			}
	}
		#pragma endregion
	check_equals(); p = get_avail(); info(p) = c; link(q) = p;
	#pragma region <Attach the replacement text to the tail of node p 698>
	if (m == start_def) link(p) = scan_toks(macro_def,r,null,n);
	else { 
		q = get_avail(); info(q) = bg_loc; link(p) = q; p = get_avail(); info(p) = eg_loc;
		link(q) = scan_toks(macro_def, r, p, n);
	}
	if (warning_info == bad_vardef) flush_token_list(value(bad_vardef));
	#pragma endregion
	scanner_status = normal; get_x_next();
}



// 707
void expand()
{
	pointer p; //{for list manipulation}
	int k; //{something that we hope is |<=buf_size|}
	pool_pointer j; //{index into |str_pool|}

	if (internal[tracing_commands] > unity)
		if (cur_cmd != defined_macro)
			show_cur_cmd_mod;
	switch (cur_cmd) {
		case if_test:
			conditional(); //{this procedure is discussed in Part 36 below}
			break;
		case fi_or_else:
			#pragma region <Terminate the current conditional and skip to {fi}>
			if (cur_mod > if_limit)
				if (if_limit == if_code) //{condition not yet evaluated}
				{
					missing_err(/*:*/58);
					back_input(); cur_sym = frozen_colon; ins_error();
				}
				else
				{
					print_err(/*Extra */687); print_cmd_mod(fi_or_else,cur_mod);
					help1(/*I'm ignoring this; it doesn't match any if.*/688);
					error();
				}
			else  { 
				while (cur_mod != fi_code) pass_text(); //{skip to \&{fi}}
				#pragma region <Pop the condition stack>
				{ 
					p = cond_ptr; if_line = if_line_field(p);
					cur_if = name_type(p); if_limit = type(p); cond_ptr = link(p);
					free_node(p,if_node_size);
				}	
				#pragma endregion
			}
			#pragma endregion
			break;
		case input:
			#pragma region <Initiate or terminate input from a file>
			if (cur_mod > 0) force_eof = true;
			else start_input();
			#pragma endregion
			break;
		case iteration:
			if (cur_mod == end_for)
				#pragma region <Scold the user for having an extra {endfor}>
			{
				print_err(/*Extra `endfor'*/689);
				help2(/*I'm not currently working on a for loop,*/690,
					/*so I had better not try to end anything.*/691);
				error();
			}			
				#pragma endregion
			else begin_iteration(); //{this procedure is discussed in Part 37 below}
			break;
		case repeat_loop:
			#pragma region <Repeat a loop>
			{
				while (token_state && loc == null) end_token_list(); //{conserve stack space}
				if (loop_ptr == null)
				{
					print_err(/*Lost loop*/692);
					help2(/*I'm confused; after exiting from a loop, I still seem*/693,
						/*to want to repeat it. I'll try to forget the problem.*/694);
					error();
				}
				else resume_iteration(); //{this procedure is in Part 37 below}
			}		
			#pragma endregion
			break;
		case exit_test:
			#pragma region <Exit a loop if the proper time has come>
			{ 
				get_boolean();
				if (internal[tracing_commands] > unity) show_cmd_mod(nullary,cur_exp);
				if (cur_exp == true_code)
					if (loop_ptr == null)
					{
						print_err(/*No loop is in progress*/695);
						help1(/*Why say `exitif' when there's nothing to exit from?*/696);
						if (cur_cmd == semicolon) error(); else back_error();
					}
					else
						#pragma region <Exit prematurely from an iteration>
					{
						p = null;
						do {
							if (file_state) end_file_reading();
							else  {
								if (token_type <= loop_text) p = start;
								end_token_list();
							}
						} while (!( p!=null));
					if (p != info(loop_ptr)) fatal_error(/**** (loop confusion)*/697);
					stop_iteration(); //{this procedure is in Part 37 below}
					}		
					#pragma endregion
				else if (cur_cmd != semicolon)
				{
					missing_err(/*;*/59);
					help2(/*After `exitif <boolean exp>' I expect to see a semicolon.*/698,
					/*I shall pretend that one was there.*/699); back_error();
				}
			}	
			#pragma endregion
			break;
		case relax: //do_nothing;
			break;
		case expand_after:
			#pragma region <Expand the token after the next token>
			get_next();
			p = cur_tok(); get_next();
			if (cur_cmd < min_command) expand(); else back_input();
			back_list(p);
			#pragma endregion
			break;
		case scan_tokens:
			#pragma region <Put a string into the input buffer>
			get_x_next(); scan_primary();
			if (cur_type != string_type)
			{
				disp_err(null,/*Not a string*/700);
				help2(/*I'm going to flush this expression, since*/701,
				/*scantokens should be followed by a known string.*/702);
				put_get_flush_error(0);
			}
			else {
				back_input();
				if (length(cur_exp) > 0)
				#pragma region <Pretend were reading a new one-line file>
				{
					begin_file_reading(); name = 2;
					k = first + length(cur_exp);
					if (k >= max_buf_stack)
					{
						if (k >= buf_size)
						{
							max_buf_stack = buf_size;
							overflow(/*buffer size*/264, buf_size);
						}
						max_buf_stack = k + 1;
					}
					j = str_start[cur_exp]; limit = k;
					while (first < limit)
					{
						buffer[first] = so(str_pool[j]); j++; first++;
					}
					buffer[limit] = /*%*/37; first = limit + 1; loc = start; flush_cur_exp(0);
				}			
				#pragma endregion
			}
			#pragma endregion
			break;
		case defined_macro:
			macro_call(cur_mod,null,cur_sym);
			break;
	} //{there are no other cases}
}



// 718
void get_x_next()
{
	pointer save_exp; // a capsule to save cur_type and cur_exp

	get_next();
	if (cur_cmd < min_command) {
		save_exp = stash_cur_exp();
		do {
			if (cur_cmd == defined_macro) macro_call(cur_mod, null, cur_sym);
			else expand();
			get_next();
		} while (!(cur_cmd >= min_command));
		unstash_cur_exp(save_exp); // that restores cur_type and cur_exp
	}
}

// 720
void macro_call(pointer def_ref,pointer arg_list,pointer macro_name)
{
	//{invokes a user-defined control sequence}
	pointer r; //{current node in the macro's token list}
	pointer p,q; //{for list manipulation}
	int n; //{the number of arguments}
	pointer l_delim=max_halfword, r_delim=max_halfword; //{a delimiter pair}
	pointer tail = max_halfword; //{tail of the argument list}

	r = link(def_ref); add_mac_ref(def_ref);
	if (arg_list == null) n = 0;
	else
		#pragma region <Determine the number |n| of arguments already supplied, and set |tail| to the tail of |arg_list|>
	{
		n = 1; tail = arg_list;
		while (link(tail) != null)
		{ 
			n++; tail = link(tail);
		}
	}
		#pragma endregion
	if (internal[tracing_macros] > 0)
		#pragma region <Show the text of the macro being expanded, and the existing arguments>
	{ 
		begin_diagnostic(); print_ln(); print_macro_name(arg_list,macro_name);
		if (n == 3) print(/*@#*/563); //{indicate a suffixed macro}
		show_macro(def_ref,null,100000);
		if (arg_list != null)
		{ 
			n = 0; p = arg_list;
			do { 
				q = info(p);
				print_arg(q,n,0);
				n++; p = link(p);
			} while (!(p == null));
		}
		end_diagnostic(false);
	}
		#pragma endregion

	#pragma region <Scan the remaining arguments, if any; set |r| to the first token of the replacement text>
	cur_cmd = comma + 1; //{anything |<>comma| will do}
	while (info(r) >= expr_base)
	{
		#pragma region <Scan the delimited argument represented by |info(r)|>
		
		if (cur_cmd != comma)
		{ 
			get_x_next();
			if (cur_cmd != left_delimiter)
			{ 
				print_err(/*Missing argument to */703);
				print_macro_name(arg_list,macro_name);
				help3(/*That macro has more parameters than you thought.*/704,
					/*I'll continue by pretending that each missing argument*/705,
					/*is either zero or null.*/706);
				if (info(r) >= suffix_base)
				{ 
				cur_exp = null; cur_type = token_list;
				}
				else {
					cur_exp = 0; cur_type = known;
				}
				back_error(); cur_cmd = right_delimiter; goto found;
			}
			l_delim = cur_sym; r_delim = cur_mod;
		}
		#pragma region <Scan the argument represented by |info(r)|>
		if (info(r) >= text_base) scan_text_arg(l_delim,r_delim);
		else { get_x_next();
		if (info(r) >= suffix_base) scan_suffix();
		else scan_expression();
		}
		#pragma endregion
		if (cur_cmd != comma) 
			#pragma region <Check that the proper right delimiter was present>
			if (cur_cmd != right_delimiter || cur_mod != l_delim)
				if (info(link(r)) >= expr_base)
				{ 
					missing_err(/*,*/44);
					help3(/*I've finished reading a macro argument and am about to*/707,
						/*read another; the arguments weren't delimited correctly.*/708,
						/*You might want to delete some tokens before continuing.*/709);
					back_error(); cur_cmd = comma;
				}
				else  { 
					missing_err(text(r_delim));
					help2(/*I've gotten to the end of the macro parameter list.*/710,
						/*You might want to delete some tokens before continuing.*/709);
					back_error();
				}
			#pragma endregion
	found:
		#pragma region <Append the current expression to |arg_list|>
		p = get_avail();
		if (cur_type == token_list) info(p) = cur_exp;
		else info(p) = stash_cur_exp();
		if (internal[tracing_macros] > 0)
		{ begin_diagnostic(); print_arg(info(p),n,info(r)); end_diagnostic(false);
		}
		if (arg_list == null) arg_list = p;
		else link(tail) = p;
		tail = p; n++;
		#pragma endregion
		
		
		#pragma endregion
		r = link(r);
	}
	if (cur_cmd == comma)
	{ 
		print_err(/*Too many arguments to */711);
		print_macro_name(arg_list,macro_name); print_char(/*;*/59);
		print_nl(/*  Missing `*/712); slow_print(text(r_delim));
		print(/*' has been inserted*/304);
		help3(/*I'm going to assume that the comma I just read was a*/713,
			/*right delimiter, and then I'll begin expanding the macro.*/714,
			/*You might want to delete some tokens before continuing.*/709);
		error();
	}
	if (info(r) != general_macro) 
		#pragma region <Scan undelimited argument(s)>
	{ 
		if (info(r) < text_macro)
		{ 
			get_x_next();
			if (info(r) != suffix_macro)
			if (cur_cmd == equals || cur_cmd == assignment) get_x_next();
		}
		switch(info(r)) {
			case primary_macro:
				scan_primary();
				break;
			case secondary_macro:
				scan_secondary();
				break;
			case tertiary_macro:
				scan_tertiary();
				break;
			case expr_macro:
				scan_expression();
				break;
			case of_macro:
				#pragma region <Scan an expression followed by ...>
				scan_expression(); p = get_avail(); info(p) = stash_cur_exp();
				if (internal[tracing_macros] > 0)
				{ 
					begin_diagnostic(); print_arg(info(p),n,0); end_diagnostic(false);
				}
				if (arg_list == null) arg_list = p; else link(tail) = p;
				tail = p; n++;
				if (cur_cmd != of_token)
				{ 
					missing_err(/*of*/540); print(/* for */715);
					print_macro_name(arg_list,macro_name);
					help1(/*I've got the first argument; will look now for the other.*/716);
					back_error();
				}
				get_x_next(); scan_primary();
				#pragma endregion
				break;
			case suffix_macro:
				#pragma region <Scan a suffix with optional delimiters>
				if (cur_cmd != left_delimiter) l_delim = null;
				else { 
					l_delim = cur_sym; r_delim = cur_mod; get_x_next();
				}
				scan_suffix();
				if (l_delim != null)
				{ 
					if(cur_cmd != right_delimiter || cur_mod != l_delim)
					{ 
						missing_err(text(r_delim));
						help2(/*I've gotten to the end of the macro parameter list.*/710,
						/*You might want to delete some tokens before continuing.*/709);
						back_error();
					}
					get_x_next();
				}
				#pragma endregion
				break;
			case text_macro:
				scan_text_arg(0,0);
				break;
		} //{there are no other cases}
		back_input(); 
		#pragma region <Append the current expression to |arg_list|>
		p = get_avail();
		if (cur_type == token_list) info(p) = cur_exp;
		else info(p) = stash_cur_exp();
		if (internal[tracing_macros] > 0)
		{ 
			begin_diagnostic(); print_arg(info(p),n,info(r)); end_diagnostic(false);
		}
		if (arg_list == null) arg_list = p;
		else link(tail) = p;
		tail = p; n++;
		#pragma endregion
	}

	
		#pragma endregion
	r = link(r);

	#pragma endregion

	#pragma region <Feed the arguments and replacement text to the scanner>
	while (token_state && loc == null) end_token_list(); //{conserve stack space}
	if (param_ptr + n > max_param_stack)
	{
		max_param_stack = param_ptr + n;
		if (max_param_stack > param_size)
			overflow(/*parameter stack size*/686,param_size);
	}
	begin_token_list(def_ref,macro); name = macro_name; loc = r;
	if (n > 0)
	{ 
		p = arg_list;
		do { 
			param_stack[param_ptr] = info(p); param_ptr++; p = link(p);
		} while (!(p == null));
		flush_list(arg_list);
	}
	#pragma endregion
}


// 722
void print_macro_name(pointer a, pointer n)
{
	pointer p, q; // they traverse the first part of a
	if (n != null) slow_print(text(n));
	else {
		p = info(a);
		if (p == null) slow_print(text(info(info(link(a)))));
		else {
			q = p;
			while (link(q) != null) q = link(q);
			link(q) = info(link(a)); show_token_list(p, null, 1000, 0); link(q) = null;
		}
	}
}

// 723
void print_arg(pointer q, int n, pointer b)
{
	if (link(q) == _void) print_nl(/*(EXPR*/422);
	else if (b < text_base && b != text_macro) print_nl(/*(SUFFIX*/423);
	else print_nl(/*(TEXT*/424);
	print_int(n); print(/*)<-*/717);
	if (link(q) == _void) print_exp(q, 1);
	else show_token_list(q, null, 1000, 0);
}


// 730
void scan_text_arg(pointer l_delim, pointer r_delim)
{
	int balance; //{excess of l_delim over r_delim }
	pointer p; //{list tail}
	warning_info = l_delim; scanner_status = absorbing; p = hold_head; balance = 1;
	link(hold_head) = null;
	while (true) { 
		get_next();
		if (l_delim == 0)
			#pragma region <Adjust the balance for an undelimited argument; goto done if done 732>
		{ 
			if (end_of_statement) //{cur_cmd = semicolon, end_group, or stop }
			{ 
				if (balance == 1) goto done;
				else if (cur_cmd == end_group) balance--;
			}
			else if (cur_cmd == begin_group) balance++;
		}
			#pragma endregion
		else
			#pragma region <Adjust the balance for a_delimited argument; goto done if done 731>
		{ 
			if (cur_cmd == right_delimiter)
			{ 
				if (cur_mod == l_delim)
				{ 
					balance--;
					if (balance == 0) goto done;
				}
			}
			else if (cur_cmd == left_delimiter)
				if (cur_mod == r_delim) balance++;
		}
			#pragma endregion
		link(p) = cur_tok(); p = link(p);
	}
	done: cur_exp = link(hold_head); cur_type = token_list; scanner_status = normal;
}

// 737
void stack_argument(pointer p)
{
	if (param_ptr == max_param_stack) {
		max_param_stack++;
		if (max_param_stack > param_size) overflow(/*parameter stack size*/686, param_size);
	}
	param_stack[param_ptr] = p; param_ptr++;
}


// 742
void pass_text()
{
	int l;

	scanner_status = skipping; l = 0; warning_info = line;
	while (true) {
		get_next();
		if (cur_cmd <= fi_or_else)
			if (cur_cmd < fi_or_else) l++;
			else {
				if (l == 0) goto done;
				if (cur_mod == fi_code) l--;
			}
		else
			#pragma region <Decrease the string reference count, if the current token is a string 743>
		{
			if (cur_cmd == string_token) delete_str_ref(cur_mod);
		}
			#pragma endregion
	}
done:
	scanner_status = normal;
}



// 746
void change_if_limit(small_number l, pointer p)
{
	pointer q;
	if (p == cond_ptr) if_limit = l; // that's the easy case
	else {
		q = cond_ptr;
		while (true) {
			if (q == null) confusion(/*if*/573);
			if (link(q) == p) {
				type(q) = l; return;
			}
			q = link(q);
		}
	}
}


// 747
void check_colon()
{
	if (cur_cmd != colon) {
		missing_err(/*:*/58);
		help2(/*There should've been a colon after the condition.*/718,
		      /*I shall pretend that one was there.*/699); back_error();
	}
}

// 748
void conditional()
{
	pointer save_cond_ptr; //{|cond_ptr| corresponding to this conditional}
	int new_if_limit;//:fi_code..else_if_code; //{future value of |if_limit|}
	pointer p; //{temporary register}
	
	#pragma region<Push the condition stack>
	p = get_node(if_node_size); link(p) = cond_ptr; type(p) = if_limit;
	name_type(p) = cur_if; if_line_field(p) = if_line;
	cond_ptr = p; if_limit = if_code; if_line = line; cur_if = if_code;
	#pragma endregion
	save_cond_ptr = cond_ptr;
reswitch: 
	get_boolean(); new_if_limit = else_if_code;
	if (internal[tracing_commands] > unity)
		#pragma region <Display the boolean value of |cur_exp|>
	{ 
		begin_diagnostic();
		if (cur_exp == true_code) print(/*{true}*/719); else print(/*{false}*/720);
		end_diagnostic(false);
	}	
		#pragma endregion
found: 
	check_colon();
	if (cur_exp == true_code)
	{ 
		change_if_limit(new_if_limit,save_cond_ptr);
		return; //{wait for \&{elseif}, \&{else}, or \&{fi}}
	}
	#pragma region <Skip to {elseif} or {else} or {fi}, then |goto done|>
	while (true) { 
		pass_text();
		if (cond_ptr == save_cond_ptr) goto done;
		else if (cur_mod == fi_code) 
			#pragma region <Pop the condition stack>
		{ 
			p = cond_ptr; if_line = if_line_field(p);
			cur_if = name_type(p); if_limit = type(p); cond_ptr = link(p);
			free_node(p,if_node_size);
		}		
			#pragma endregion
	}
	#pragma endregion
done: 
	cur_if = cur_mod; if_line = line;
	if (cur_mod == fi_code) 
		#pragma region <Pop the condition stack>
		{ 
			p = cond_ptr; if_line = if_line_field(p);
			cur_if = name_type(p); if_limit = type(p); cond_ptr = link(p);
			free_node(p,if_node_size);
		}	
		#pragma endregion
	else if (cur_mod == else_if_code) goto reswitch;
	else { 
		cur_exp = true_code; new_if_limit = fi_code; get_x_next(); goto found;
	}
}


// 754
void bad_for(str_number s)
{
	disp_err(null , /*Improper */721); // show the bad expression above the message
	print(s); print(/* has been replaced by 0*/311);
	help4(/*When you say `for x=a step b until c',*/722,
		/*the initial value `a' and the step size `b'*/723,
		/*and the final value `c' must have known numeric values.*/724,
		/*I'm zeroing this one. Proceed, with fingers crossed.*/313); put_get_flush_error(0);
}

// 755
void begin_iteration()
{
	halfword m; //{|expr_base| (\&{for}) or |suffix_base| (\&{forsuffixes})}
	halfword n; //{hash address of the current symbol}
	pointer p,q,s,pp; //{link manipulation registers}
	m = cur_mod; n = cur_sym; s = get_node(loop_node_size);
	if (m == start_forever)
	{ loop_type(s) = _void; p = null; get_x_next(); goto found;
	}
	get_symbol(); p = get_node(token_node_size); info(p) = cur_sym; value(p) = m;
	get_x_next();
	if (cur_cmd != equals && cur_cmd != assignment)
	{ 
		missing_err(/*=*/61);
		help3(/*The next thing in this loop should have been `=' or `:='.*/725,
			/*But don't worry; I'll pretend that an equals sign*/678,
			/*was present, and I'll look for the values next.*/726);
		back_error();
	}
	#pragma region <Scan the values to be used in the loop>
	loop_type(s) = null; q = loop_list_loc(s); link(q) = null; //{|link(q)=loop_list(s)|}
	do { 
		get_x_next();
		if (m != expr_base) scan_suffix();
		else {
			if (cur_cmd >= colon) if (cur_cmd <= comma) continue;
			scan_expression();
			if (cur_cmd == step_token) 
				if (q == loop_list_loc(s))
					#pragma region <Prepare for step-until construction and |goto done|>
				{ 
					if (cur_type != known) bad_for(/*initial value*/727);
					pp = get_node(progression_node_size); value(pp) = cur_exp;
					get_x_next(); scan_expression();
					if (cur_type != known) bad_for(/*step size*/728);
					step_size(pp) = cur_exp;
					if (cur_cmd != until_token)
					{ 
						missing_err(/*until*/551);
						help2(/*I assume you meant to say `until' after `step'.*/729,
							/*So I'll look for the final value and colon next.*/730);
						back_error();
					}
					get_x_next(); scan_expression();
					if (cur_type != known) bad_for(/*final value*/731);
					final_value(pp) = cur_exp; loop_type(s) = pp; goto done;
				}
					#pragma endregion
			cur_exp = stash_cur_exp();
		}
		link(q) = get_avail(); q = link(q); info(q) = cur_exp; cur_type = vacuous;
	} while (!(cur_cmd != comma));
done:
	#pragma endregion
found:
	#pragma region <Check for the presence of a colon>
	if (cur_cmd != colon)
	{ 
		missing_err(/*:*/58);
		help3(/*The next thing in this loop should have been a `:'.*/732,
			/*So I'll pretend that a colon was present;*/733,
			/*everything from here to `endfor' will be iterated.*/734);
		back_error();
	}	
	#pragma endregion


	#pragma region <Scan the loop text and put it on the loop control stack>
	q = get_avail(); info(q) = frozen_repeat_loop;
	scanner_status = loop_defining; warning_info = n;
	info(s) = scan_toks(iteration,p,q,0); scanner_status = normal;
	link(s) = loop_ptr; loop_ptr = s;
	#pragma endregion
	resume_iteration();
}





// 760
void resume_iteration()
{
	pointer p,q; //{link registers}
	p = loop_type(loop_ptr);
	if (p > _void) //{|p| points to a progression node}
	{ 
		cur_exp = value(p);
		if (/*<The arithmetic progression has ended>*/ ( (step_size(p) > 0) && (cur_exp > final_value(p) ) ) || ( (step_size(p) < 0) && (cur_exp < final_value(p) ) )) goto not_found;
		cur_type = known; q = stash_cur_exp(); //{make |q| an \&{expr} argument}
		value(p) = cur_exp + step_size(p); //{set |value(p)| for the next iteration}
	}
	else if (p < _void)
	{ 
		p = loop_list(loop_ptr);
		if (p == null) goto not_found;
		loop_list(loop_ptr) = link(p); q = info(p); free_avail(p);
	}
	else { 
		begin_token_list(info(loop_ptr),forever_text); return;
	}
	begin_token_list(info(loop_ptr),loop_text);
	stack_argument(q);
	if (internal[tracing_commands] > unity) 
		#pragma region <Trace the start of a loop>
	{ 
		begin_diagnostic(); print_nl(/*{loop value=*/735);
		if (q != null && link(q) == _void) print_exp(q,1);
		else show_token_list(q,null,50,0);
		print_char(/*}*/125); end_diagnostic(false);
	}
		#pragma endregion
	return;
not_found:
	stop_iteration();
}



// 763
void stop_iteration()
{
	pointer p,q; //{the usual}
	p = loop_type(loop_ptr);
	if (p > _void) free_node(p,progression_node_size);
	else if (p < _void)
	{
		q = loop_list(loop_ptr);
		while (q != null)
		{ 
			p = info(q);
			if (p != null)
				if (link(p) == _void) //{it's an \&{expr} parameter}
				{ 
					recycle_value(p); free_node(p,value_node_size);
				}
				else flush_token_list(p); //{it's a \&{suffix} or \&{text} parameter}
			p = q; q = link(q); free_avail(p);
		}
	}
	p = loop_ptr; loop_ptr = link(p); flush_token_list(info(p));
	free_node(p,loop_node_size);
}




// 770
void begin_name()
{
	area_delimiter = 0;
	ext_delimiter = 0;
}

// 771
bool more_name(ASCII_code c)
{
	if (c == /* */32 || c == tab) return false;
	else {
		if (c == /*\*/92) {
			area_delimiter = pool_ptr; ext_delimiter = 0;
		}
		else if (c == /*.*/46 && ext_delimiter == 0) ext_delimiter = pool_ptr;
		str_room(1); append_char(c); // contribute |c| to the current string
		return true;
	}
}

// 772
void end_name()
{
	if (str_ptr+3 > max_str_ptr)
	{
		if (str_ptr+3 > max_strings)
			overflow(/*number of strings*/265,max_strings-init_str_ptr);
		max_str_ptr = str_ptr+3;
	}
	if (area_delimiter == 0) cur_area = /**/289;
	else { 
		cur_area =str_ptr; str_ptr++;
		str_start[str_ptr] = area_delimiter+1;
	}
	if (ext_delimiter == 0)
	{
		cur_ext = /**/289; 
		cur_name = make_string();
	}
	else { 
		cur_name=str_ptr; str_ptr++;
		str_start[str_ptr] = ext_delimiter; cur_ext = make_string();
	}
}





// 773
void print_file_name(int n, int a, int e)
{
	slow_print(a);
	slow_print(n);
	slow_print(e);
}



// 774
void pack_file_name(str_number n, str_number a, str_number e)
{
	int k;
	ASCII_code c;
	pool_pointer j;

	k = 0;
	for (j = str_start[a]; j <= str_start[a + 1] - 1; j++)
		append_to_name(str_pool[j]);
	for (j = str_start[n]; j <= str_start[n + 1] - 1; j++)
		append_to_name(str_pool[j]);
	for (j = str_start[e]; j <= str_start[e + 1] - 1; j++)
		append_to_name(str_pool[j]);

	name_of_file[k + 1] = 0;
}

// 778
void pack_buffered_name(small_number n, int a, int b)
{
	int k; // number of positions filled in name_of_file
	ASCII_code c; // number of positions filled in name_of_file
	int j; // index into buffer or MF_base_default
	int base_default_length = (int)strlen(MF_base_default);
	
	if (n + b - a + 1 + base_ext_length > file_name_size)
		b = a + file_name_size - n - 1 - base_ext_length;
	k = 0;
	for (j = 1; j <= n; j++) append_to_name(xord[MF_base_default[j-1]]);
	for (j = a; j <= b; j++) append_to_name(buffer[j]);
	for (j = base_default_length - base_ext_length + 1; j <= base_default_length; j++)
		append_to_name(xord[MF_base_default[j-1]]);

	// addition for C, add zero terminator
	name_of_file[k + 1] = 0;

}


// 779
bool open_base_file()
{
	int j; // 0..buf_size, the first space after the file name
	int base_default_length = (int)strlen(MF_base_default);
	j = loc;
	if (buffer[loc] == /*&*/38) {
		loc++; j = loc; buffer[last] = /* */32;
		while (buffer[j] != /* */32) j++;
		pack_buffered_name(0, loc, j - 1); // try first without the system file area
		if (w_open_in(&base_file)) goto found;
		pack_buffered_name(base_area_length, loc, j - 1); // now try the system base file area
		if (w_open_in(&base_file)) goto found;
		wake_up_terminal(); wterm_ln_s("Sorry, I can't find that base; will try PLAIN.");
		update_terminal();

	}
	pack_buffered_name(base_default_length - base_ext_length, 1, 0);
	if (!w_open_in(&base_file)) {
		wake_up_terminal(); wterm_ln_s("I can't find the PLAIN base file!");
		return false;
	}
found:
	loc = j;
	return true;
}

// 780
str_number make_name_string()
{
	int k;
	if (pool_ptr + strlen(name_of_file.get_c_str()) > pool_size || str_ptr == max_strings)
		return /*?*/63;
	else {
		for (k = 1; k <= (int)strlen(name_of_file.get_c_str()); k++) append_char(xord[name_of_file[k]]);
		return make_string();
	}
}

str_number a_make_name_string(alpha_file)
{
	return make_name_string();
}
str_number b_make_name_string(byte_file)
{
	return make_name_string();
}
str_number w_make_name_string(word_file)
{
	return make_name_string();
}


// 781
void scan_file_name()
{
	begin_name();
	while (buffer[loc] == /* */32 || buffer[loc] == tab) loc++;
	while (true) {
		if (buffer[loc] == /*;*/59 || buffer[loc] == /*%*/37) goto done;
		if (!more_name(buffer[loc])) goto done;
		loc++;
	}
done:
	end_name();
}


// 784
void pack_job_name(str_number s)
{
	cur_area = /**/289;
	cur_ext = s;
	cur_name = job_name;
	pack_cur_name;
}

// 786
void prompt_file_name(str_number s, str_number e)
{
	int k; // 0..buf_size, index into buffer
	if (interaction == scroll_mode)
		wake_up_terminal();

	if (s == /*input file name*/736)
		print_err(/*I can't find file `*/737);
	else print_err(/*I can't write on file `*/738);
	print_file_name(cur_name, cur_area, cur_ext); print(/*'.*/739);
	if (e == /*.mf*/740) show_context();
	print_nl(/*Please type another */741); print(s);
	if (interaction < scroll_mode)
		fatal_error(/**** (job aborted, file error in nonstop mode)*/742);
	clear_terminal();
	prompt_input(/*: */743);
	#pragma region <Scan file name in the buffer 787>
	{
		begin_name();
		k = first;
		while ((buffer[k] == /* */32 || buffer[k]==tab) && k < last) k++;
		while (1) {
			if (k == last)
				goto done;
			if (!more_name(buffer[k])) goto done;
			k++;
		}
	done:
		end_name();
	}
	#pragma endregion
	if (cur_ext == /**/289) cur_ext = e;
	pack_cur_name;
}




// 788
void open_log_file()
{
	int old_setting; // 0..max_selector, previous selector setting
	int k; // 0..buf_size, index into months and buffer
	int l; // 0..buf_size, end of first input line
	int m; // the current month
	Array<char, 1, 37> months;
	old_setting = selector;
	if (job_name == 0) job_name = /*mfput*/744;
	pack_job_name(/*.log*/745);
	while (!a_open_out(&log_file))
		#pragma region <Try to get a different log file name 789>
	{
		selector = term_only; prompt_file_name(/*transcript file name*/746, 
			/*.log*/745);
	}
		#pragma endregion

	log_name = a_make_name_string(log_file); selector = log_only; log_opened = true;


	#pragma region <Print the banner line, including the date and time 790>
	wlog_s(banner); slow_print(base_ident); print(/*  */747); print_int(round_unscaled(internal[day]));
	print_char(/* */32); strcpy(months.get_c_str(), "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC");

	m = round_unscaled(internal[month]);
	for (k = 3 * m - 2; k <= 3 * m; k++) wlog_c(months[k]);
	print_char(/* */32);print_int(round_unscaled(internal[year])); print_char(/* */32);
	m = round_unscaled(internal[_time]);print_dd(m / 60); print_char(/*:*/58); print_dd(m % 60);
	#pragma endregion

	input_stack[input_ptr] = cur_input; // make sure bottom level is in memory
	print_nl(/****/748); l = input_stack[0].limit_field - 1; // last position of first line

	for (k = 1; k <= l; k++)
		print(buffer[k]);
	print_ln(); // now the transcript file contains the first line of input
	selector = old_setting + 2; // log_only or term_and_log
}

// 793
 

void start_input() // METAFONT will input something
{
	#pragma region <Put the desired file name in (cur_name, cur_ext, cur_area) 795>
	while (token_state && loc == null) end_token_list();
	if (token_state) {
		print_err(/*File names can't appear within macros*/749);
		help3(/*Sorry...I've converted what follows to tokens,*/750,
		      /*possibly garbaging the name you gave.*/751,
			  /*Please delete the tokens and insert the name again.*/752);
		error();
	}
	if (file_state) scan_file_name();
	else {
		cur_name = /**/289; cur_ext = /**/289; cur_area = /**/289;
	}

	#pragma endregion
	if (cur_ext == /**/289)
		cur_ext = /*.mf*/740;
	pack_cur_name;
	while (1) {
		begin_file_reading();
		if (a_open_in(&cur_file, input_file_path)) goto done;
		if (cur_ext == /*.mf*/740) {
			pack_file_name(cur_name, cur_area, /**/289);
			if (a_open_in(&cur_file, input_file_path)) goto done;
		}
		end_file_reading();
		prompt_file_name(/*input file name*/736,
			/*.mf*/740);
	}
done:
	name = a_make_name_string(cur_file); str_ref[cur_name] = max_str_ref;
	if (job_name == 0) {
		job_name = cur_name; open_log_file();
	} // open_log_file doesn't show_context, so limit and loc neednt be set to meaningful values yet
	if (term_offset + length(name) > max_print_line - 2) print_ln();
	else if (term_offset > 0 || file_offset > 0) print_char(/* */32);
	print_char(/*(*/40); open_parens++; slow_print(name); update_terminal();
	
	#pragma region <Read the first line of the new file 794>
	line = 1;
	input_ln(cur_file, false);
	firm_up_the_line(); buffer[limit] = /*%*/37;
	first = limit + 1; loc = start;
	#pragma endregion
}




// 799
pointer stash_cur_exp()
{
	pointer p; // the capsule that will be returned

	switch (cur_type) {
	case unknown_types: case transform_type: case pair_type: case dependent: case proto_dependent: case independent:
		p = cur_exp;
		break;
	default:
		p = get_node(value_node_size); name_type(p) = capsule; type(p) = cur_type;
		value(p) = cur_exp;
		break;
	}
	cur_type = vacuous; link(p) = _void; return p;
}

// 800
void unstash_cur_exp(pointer p)
{
	cur_type = type(p);
	switch (cur_type) {
	case unknown_types: case transform_type: case pair_type: case dependent: case proto_dependent: case independent:
		cur_exp = p;
		break;
	default:
		cur_exp = value(p); free_node(p, value_node_size);
		break;
	}
}


// 801
void print_exp(pointer p, small_number verbosity)
{
	bool restore_cur_exp; // should cur_exp be restored?
	small_number t; // the type of the expression
	int v = INT_MAX; // the value of the expression
	pointer q; // a big node being displayed

	if (p != null) restore_cur_exp = false;
	else {
		p = stash_cur_exp(); restore_cur_exp = true;
	}
	t = type(p);
	if (t < dependent) v = value(p); else if (t < independent) v = dep_list(p);

	#pragma region <Print an abbreviated value of v with format depending on t 802>
	switch (t) {
	case vacuous:
		print(/*vacuous*/331);
		break;
	case boolean_type:
		if (v == true_code) print(/*true*/355); else print(/*false*/356);
		break;
	case unknown_types: case numeric_type:
		#pragma region <Display a variable thats been declared but not defined 806>
		print_type(t);
		if (v != null) {
			print_char(/* */32);
			while (name_type(v) == capsule && v != p) v = value(v);
			print_variable_name(v);
		}
		#pragma endregion
		break;
	case string_type:
		print_char(/*"*/34); slow_print(v); print_char(/*"*/34);
		break;
	case pen_type:case future_pen:case path_type:case picture_type:
		#pragma region <Display a complex type 804>
		if (verbosity <= 1) print_type(t);
		else {
			if (selector == term_and_log)
				if (internal[tracing_online] <= 0) {
					selector = term_only; print_type(t); print(/* (see the transcript file)*/753);
					selector = term_and_log;
				}
			switch(t) {
				case pen_type:
					print_pen(v, /**/289, false);
					break;

				case future_pen:
					print_path(v, /* (future pen)*/754, false);
					break;

				case path_type:
					print_path(v, /**/289, false);
					break;

				case picture_type:
					cur_edges = v; print_edges(/**/289, false, 0, 0);
					break;
			}
		}
		#pragma endregion
		break;
	case transform_type:case pair_type:
		if (v == null) print_type(t);
		else
			#pragma region <Display a big node 803>
		{
			print_char(/*(*/40); q = v + big_node_size[t];
			do {
				if (type(v) == known) print_scaled(value(v));
				else if (type(v) == independent) print_variable_name(v);
				else print_dp(type(v), dep_list(v), verbosity);
				v = v + 2;
				if (v != q) print_char(/*,*/44);
			} while (!(v == q));
			print_char(/*)*/41);
		}
			#pragma endregion
		break;
	case known:
		print_scaled(v);
		break;
	case dependent: case proto_dependent:
		print_dp(t, v, verbosity);
		break;
	case independent:
		print_variable_name(p);
		break;

	default:
		confusion(/*exp*/755);
		break;
	}
	#pragma endregion

	if (restore_cur_exp) unstash_cur_exp(p);
}

// 805
void print_dp(small_number t, pointer p, small_number verbosity)
{
	pointer q; // the node following p
	q = link(p);
	if (info(q) == null || verbosity > 0) print_dependency(p, t);
	else print(/*linearform*/756);
}

// 807
void disp_err(pointer p, str_number s)
{
	if (interaction == error_stop_mode) wake_up_terminal();
	print_nl(/*>> */757); print_exp(p,1); //{``medium verbose'' printing of the expression}
	if (s != /**/289)
	{ 
		print_nl(/*! */758); print(s);
	}
}




// 808
void flush_cur_exp(scaled v)
{
	switch(cur_type) {
		case unknown_types: case transform_type: case pair_type: case dependent:
		case proto_dependent: case independent:
			recycle_value(cur_exp);
			free_node(cur_exp, value_node_size);
			break;
		case pen_type:
			delete_pen_ref(cur_exp);
			break;
		case string_type:
			delete_str_ref(cur_exp);
			break;
		case future_pen: case path_type:
			toss_knot_list(cur_exp);
			break;
		case picture_type:
			toss_edges(cur_exp);
			break;
		default:
			// do_nothing
			break;
	}
	cur_type = known; cur_exp = v;

}

// 809
void recycle_value(pointer p)
{
	small_number t; //{a type code}
	int v = INT_MAX; //{a value}
	int vv; //{another value}
	pointer q, r, s, pp; //{link manipulation registers}
	
	t = type(p);
	if (t < dependent) v = value(p);
	switch(t) {
		case undefined:case vacuous:case boolean_type:case known:case numeric_type:
			//do_nothing;
			break;
		case unknown_types:
			ring_delete(p);
			break;
		case string_type:
			delete_str_ref(v);
			break;
		case pen_type:
			delete_pen_ref(v);
			break;
		case path_type: case future_pen:
			toss_knot_list(v);
			break;
		case picture_type:
			toss_edges(v);
			break;
		case pair_type: case transform_type:
			#pragma region <Recycle a big node>
			if (v != null)
			{ 
				q = v + big_node_size[t];
				do { 
					q = q - 2; recycle_value(q);
				} while (!(q==v));
				free_node(v,big_node_size[t]);
			}		
			#pragma endregion
			break;
		case dependent: case proto_dependent:
			#pragma region <Recycle a dependency list>
			q = dep_list(p);
			while (info(q) != null) q = link(q);
			link(prev_dep(p)) = link(q);
			prev_dep(link(q)) = prev_dep(p);
			link(q) = null; flush_node_list(dep_list(p));
			#pragma endregion
			break;
		case independent:
			#pragma region <Recycle an independent variable>
			max_c[dependent] = 0; max_c[proto_dependent] = 0;
			max_link[dependent] = null; max_link[proto_dependent] = null;
			q = link(dep_head);
			while (q != dep_head)
			{ 
				s = value_loc(q); //{now |link(s)=dep_list(q)|}
				while (true) {
					r = link(s);
					if (info(r) == null) goto done;
					if (info(r) != p) s = r;
					else  { 
						t = type(q); link(s) = link(r); info(r) = q;
						if (myabs(value(r)) > max_c[t])
						#pragma region <Record a new maximum coefficient of type |t|>
						{ 
							if (max_c[t] > 0)
							{
								link(max_ptr[t]) = max_link[t]; max_link[t] = max_ptr[t];
							}
							max_c[t] = myabs(value(r)); max_ptr[t] = r;
						}		
						#pragma endregion
						else  { 
							link(r) = max_link[t]; max_link[t] = r;
						}
					}
				}
				done:  q = link(r);
			}
			if (max_c[dependent] > 0 || max_c[proto_dependent] > 0)
			#pragma region <Choose a dependent variable to take the place of the disappearing independent variable, and change all remaining dependencies accordingly>
			{
				if (max_c[dependent] / 010000 >= max_c[proto_dependent])
					t = dependent;
				else t = proto_dependent;
				#pragma region <Determine the dependency list |s| to substitute for the independent variable~|p|>
				s = max_ptr[t]; pp = info(s); v = value(s);
				if (t == dependent) value(s) = -fraction_one; else value(s) = -unity;
				r = dep_list(pp); link(s) = r;
				while (info(r) != null) r = link(r);
				q = link(r); link(r) = null;
				prev_dep(q) = prev_dep(pp); link(prev_dep(pp)) = q;
				new_indep(pp);
				if (cur_exp == pp)
					if (cur_type == t) cur_type = independent;
				if (internal[tracing_equations] > 0)
					#pragma region <Show the transformed dependency>
					if (interesting(p))
					{
						begin_diagnostic(); print_nl(/*### */759);
						if (v > 0) print_char(/*-*/45);
						if (t == dependent) vv = round_fraction(max_c[dependent]);
						else vv = max_c[proto_dependent];
						if (vv != unity) print_scaled(vv);
						print_variable_name(p);
						while (value(p) % s_scale > 0)
						{ 
							print(/**4*/509); value(p) = value(p) - 2;
						}
						if (t == dependent) print_char(/*=*/61); else print(/* = */760);
						print_dependency(s,t);
						end_diagnostic(false);
					}
					#pragma endregion
								
				#pragma endregion
				t = dependent + proto_dependent - t; //{complement |t|}
				if (max_c[t] > 0) //{we need to pick up an unchosen dependency}
				{
					link(max_ptr[t]) = max_link[t]; max_link[t] = max_ptr[t];
				}
				if (t != dependent) 
					#pragma region <Substitute new dependencies in place of |p|>
					for (t = dependent; t <= proto_dependent; t++)
					{ 
						r = max_link[t];
						while (r!=null)
						{ 
							q = info(r);
							dep_list(q) = p_plus_fq(dep_list(q),make_fraction(value(r),-v),s,t,dependent);
							if (dep_list(q) == dep_final) make_known(q,dep_final);
							q = r; r = link(r); free_node(q,dep_node_size);
						}
					}			
					#pragma endregion
				else 
					#pragma region <Substitute new proto-dependencies in place of |p|>
					for (t = dependent; t <= proto_dependent; t++)
					{ 
						r = max_link[t];
						while (r != null)
						{ 
							q = info(r);
							if (t == dependent) //{for safety's sake, we change |q| to |proto_dependent|}
							{ 
								if (cur_exp == q)
									if (cur_type == dependent)
										cur_type = proto_dependent;
								dep_list(q) = p_over_v(dep_list(q),unity,dependent,proto_dependent);
								type(q) = proto_dependent; value(r) = round_fraction(value(r));
							}
							dep_list(q) = p_plus_fq(dep_list(q),make_scaled(value(r),-v),s,proto_dependent,proto_dependent);
							if (dep_list(q) == dep_final) make_known(q,dep_final);
							q = r; r = link(r); free_node(q,dep_node_size);
						}
					}
					#pragma endregion
				flush_node_list(s);
				if (fix_needed) fix_dependencies();
				check_arith;
			}			
			#pragma endregion
			#pragma endregion
			break;
		case token_list: case structured:
			confusion(/*recycle*/761);
			break;
		case unsuffixed_macro: case suffixed_macro:
			delete_mac_ref(value(p));
			break;
	} //{there are no other cases}
	type(p) = undefined;
}


// 820
void flush_error(scaled v)
{
	error(); flush_cur_exp(v);
}

void put_get_error()
{
	back_error(); get_x_next();
}

void put_get_flush_error(scaled v)
{
	put_get_error(); flush_cur_exp(v);
}


// 823
void scan_primary()
{
	pointer p,q,r; //{for list manipulation}
	quarterword c; //{a primitive operation code}
	int my_var_flag; // 0..max_command_code; //{initial value of |my_var_flag|}
	pointer l_delim, r_delim; //{hash addresses of a delimiter pair}
	#pragma region <Other local variables for |scan_primary|>
	// 831
	int group_line; //{where a group began}
	// 836
	scaled num, denom; //{for primaries that are fractions, like `1/2'}
	// 843
	pointer pre_head,post_head,tail;
	  //{prefix and suffix list variables}
	small_number tt; //{approximation to the type of the variable-so-far}
	pointer t; //{a token}
	pointer macro_ref = max_halfword; //{reference count for a suffixed macro}
	#pragma endregion

	my_var_flag = var_flag; var_flag = 0;
	restart:
	check_arith;
	#pragma region <Supply diagnostic information, if requested 825>
	#ifndef NO_DEBUG 
	if (panicking) check_mem(false);
	#endif
	if (interrupt != 0)
		if (OK_to_interrupt)
		{
			back_input(); check_interrupt; get_x_next();
		}
	#pragma endregion

	switch(cur_cmd) {
	case left_delimiter:
		#pragma region <Scan a delimited primary>
		l_delim = cur_sym; r_delim = cur_mod; get_x_next(); scan_expression();
		if (cur_cmd == comma && cur_type >= known)
			#pragma region <Scan the second of a pair of numerics>
		{ 
			p = get_node(value_node_size); type(p) = pair_type; name_type(p) = capsule;
			init_big_node(p); q = value(p); stash_in(x_part_loc(q));
			get_x_next(); scan_expression();
			if (cur_type < known)
			{ 
				exp_err(/*Nonnumeric ypart has been replaced by 0*/762);
				help4(/*I thought you were giving me a pair `(x,y)'; but*/763,
					/*after finding a nice xpart `x' I found a ypart `y'*/764,
					/*that isn't of numeric type. So I've changed y to zero.*/765,
					/*(The y that I didn't like appears above the error message.)*/766);
				put_get_flush_error(0);
			}
			stash_in(y_part_loc(q));
			check_delimiter(l_delim,r_delim);
			cur_type = pair_type; cur_exp = p;
		}		
			#pragma endregion
		else check_delimiter(l_delim,r_delim);
		#pragma endregion
		break;
	case begin_group:
		#pragma region <Scan a grouped primary>
		group_line = line;
		if (internal[tracing_commands] > 0) show_cur_cmd_mod;
		save_boundary_item(p);
		do { 
			do_statement(); //{ends with |cur_cmd>=semicolon|}
		} while (!( cur_cmd != semicolon));
		if (cur_cmd != end_group)
		{
			print_err(/*A group begun on line */767);
			print_int(group_line);
			print(/* never ended*/768);
			help2(/*I saw a `begingroup' back there that hasn't been matched*/769,
			/*by `endgroup'. So I've inserted `endgroup' now.*/770);
			back_error(); cur_cmd = end_group;
		}
		unsave(); //{this might change |cur_type|, if independent variables are recycled}
		if (internal[tracing_commands] > 0) show_cur_cmd_mod;
		#pragma endregion
		break;
	case string_token:
		#pragma region <Scan a string constant>
		cur_type = string_type; cur_exp = cur_mod;
		#pragma endregion
		break;
	case numeric_token:
		#pragma region <Scan a primary that starts with a numeric token>
		cur_exp = cur_mod; cur_type = known; get_x_next();
		if (cur_cmd != slash)
		{
			num = 0; denom = 0;
		}
		else  {
			get_x_next();
			if (cur_cmd != numeric_token)
			{
				back_input();
				cur_cmd = slash; cur_mod = over; cur_sym = frozen_slash;
				goto done;
			}
			num = cur_exp; denom = cur_mod;
			if (denom == 0)
			#pragma region <Protest division by zero>
			{
				print_err(/*Division by zero*/771);
				help1(/*I'll pretend that you meant to divide by 1.*/772); error();
			}	
			#pragma endregion
			else cur_exp = make_scaled(num,denom);
			check_arith; get_x_next();
		}
		if (cur_cmd >= min_primary_command)
			if (cur_cmd < numeric_token) //{in particular, |cur_cmd != plus_or_minus|}
			{
				p = stash_cur_exp(); scan_primary();
				if (myabs(num) >= myabs(denom) || cur_type < pair_type)  do_binary(p,times);
				else {
					frac_mult(num,denom);
					free_node(p,value_node_size);
				}
			}
		goto done;
		#pragma endregion
		break;
	case nullary:
		#pragma region <Scan a nullary operation>
		do_nullary(cur_mod);
		#pragma endregion
		break;
	case unary: case type_name: case cycle: case plus_or_minus:
		#pragma region <Scan a unary operation>
		c = cur_mod; get_x_next(); scan_primary(); do_unary(c); goto done;
		#pragma endregion
		break;
	case primary_binary:
		#pragma region <Scan a binary operation with &{of} between its operands>
		c = cur_mod; get_x_next(); scan_expression();
		if (cur_cmd != of_token)
		{
			missing_err(/*of*/540); print(/* for */715); print_cmd_mod(primary_binary,c);
			help1(/*I've got the first argument; will look now for the other.*/716);
			back_error();
		}
		p = stash_cur_exp(); get_x_next(); scan_primary(); do_binary(p,c); goto done;
		#pragma endregion
		break;
	case str_op:
		#pragma region <Convert a suffix to a string>
		get_x_next(); scan_suffix(); old_setting = selector; selector = new_string;
		show_token_list(cur_exp, null, 100000,0); flush_token_list(cur_exp);
		cur_exp = make_string(); selector = old_setting; cur_type = string_type;
		goto done;
		#pragma endregion
		break;
	case internal_quantity:
		#pragma region <Scan an internal numeric quantity>
		q = cur_mod;
		if (my_var_flag == assignment)
		{
			get_x_next();
			if (cur_cmd == assignment)
			{
				cur_exp = get_avail();
				info(cur_exp) = q + hash_end; cur_type = token_list; goto done;
			}
			back_input();
		}
		cur_type = known; cur_exp = internal[q];
		#pragma endregion
		break;
	case capsule_token:
		make_exp_copy(cur_mod);
		break;
	case tag_token:
		#pragma region <Scan a variable primary |goto restart| if it turns out to be a macro>
		fast_get_avail(pre_head); tail = pre_head; post_head = null; tt = vacuous;
		while (true) {
			t = cur_tok(); link(tail) = t;
			if (tt != undefined)
			{
				#pragma region <Find the approximate type |tt| and corresponding~|q|>
				{
					p = link(pre_head); q = info(p); tt = undefined;
					if (eq_type(q) % outer_tag == tag_token)
					{
						q = equiv(q);
						if (q == null) goto done2;
						while (true)  {
							p = link(p);
							if (p == null)
							{
								tt = type(q); goto done2;
							}
							if (type(q) != structured) goto done2;
							q = link(attr_head(q)); //{the |collective_subscript| attribute}
							if (p >= hi_mem_min) //{it's not a subscript}
							{
								do {
									q = link(q);
								} while (!( attr_loc(q) >= info(p)));
								if (attr_loc(q) > info(p)) goto done2;
							}
						}
					}
				done2:
					;
				}	
				#pragma endregion
				if (tt >= unsuffixed_macro)
				#pragma region <Either {an unsuffixed macro call or prepare for a suffixed one>
				{
					link(tail) = null;
					if (tt > unsuffixed_macro) //{|tt=suffixed_macro|}
					{
						post_head = get_avail(); tail = post_head; link(tail) = t;
						tt = undefined; macro_ref = value(q); add_mac_ref(macro_ref);
					}
					else
						#pragma region <Set up unsuffixed macro call and |goto restart|>
					{ 
						p = get_avail(); info(pre_head) = link(pre_head); link(pre_head) = p;
						info(p) = t; macro_call(value(q),pre_head,null); get_x_next(); goto restart;
					}
						#pragma endregion
				}
				#pragma endregion
			}
			get_x_next(); tail = t;
			if (cur_cmd == left_bracket)
			#pragma region <Scan for a subscript; replace |cur_cmd| by |numeric_token| if found>
			{
				get_x_next(); scan_expression();
				if (cur_cmd != right_bracket)
					#pragma region <Put the left bracket and the expression back to be rescanned>
				{ 
					back_input(); //{that was the token following the current expression}
					back_expr(); cur_cmd = left_bracket; cur_mod = 0; cur_sym = frozen_left_bracket;
				}
					#pragma endregion
				else  {
					if (cur_type != known) bad_subscript();
					cur_cmd = numeric_token; cur_mod = cur_exp; cur_sym = 0;
				}
			}	
			#pragma endregion
			if (cur_cmd > max_suffix_token) goto done1;
			if (cur_cmd < min_suffix_token) goto done1;
		} //{now |cur_cmd| is |internal_quantity|, |tag_token|, or |numeric_token|}
		done1:
		#pragma region <Handle unusual cases that masquerade as variables, and |goto restart| or |goto done| if appropriate; otherwise make a copy of the variable and |goto done|>
		if (post_head != null)
			#pragma region <Set up suffixed macro call and |goto restart|>
		{ 
			back_input(); p = get_avail(); q = link(post_head);
			info(pre_head) = link(pre_head); link(pre_head) = post_head;
			info(post_head) = q; link(post_head) = p; info(p) = link(q); link(q) = null;
			macro_call(macro_ref,pre_head,null); ref_count(macro_ref)--;
			get_x_next(); goto restart;
		}
			#pragma endregion
		q = link(pre_head); free_avail(pre_head);
		if (cur_cmd == my_var_flag)
		{
			cur_type = token_list; cur_exp = q; goto done;
		}
		p = find_variable(q);
		if (p != null) make_exp_copy(p);
		else  {
			obliterated(q);
			help_line[2] = /*While I was evaluating the suffix of this variable,*/773;
			help_line[1] = /*something was redefined, and it's no longer a variable!*/774;
			help_line[0] = /*In order to get back on my feet, I've inserted `0' instead.*/775;
			put_get_flush_error(0);
		}
		flush_node_list(q); goto done;
		#pragma endregion
		#pragma endregion
		break;
	default:
		bad_exp(/*A primary*/776); goto restart;
		break;
	}
	get_x_next(); //{the routines |goto done| if they don't want this}
	done: 
	if (cur_cmd == left_bracket)
		if (cur_type >= known)
			#pragma region <Scan a mediation construction>
			{
				p = stash_cur_exp(); get_x_next(); scan_expression();
				if (cur_cmd != comma)
				{
					#pragma region <Put the left bracket and the expression back...>
					{
						back_input(); //{that was the token following the current expression}
						back_expr(); cur_cmd = left_bracket; cur_mod = 0; cur_sym = frozen_left_bracket;
					}
					#pragma endregion 
					unstash_cur_exp(p);
				}
				else  {
					q = stash_cur_exp(); get_x_next(); scan_expression();
					if (cur_cmd != right_bracket)
					{
						missing_err(/*]*/93);
						help3(/*I've scanned an expression of the form `a[b,c',*/777,
							/*so a right bracket should have come next.*/778,
							/*I shall pretend that one was there.*/699);
						back_error();
					}
					r = stash_cur_exp(); make_exp_copy(q);
					do_binary(r,minus); do_binary(p,times); do_binary(q,plus); get_x_next();
				}
			}	
			#pragma endregion
}



// 824
void bad_exp(str_number s)
{
	int save_flag; //:0..max_command_code;
	print_err(s); print(/* expression can't begin with `*/779);
	print_cmd_mod(cur_cmd,cur_mod); print_char(/*'*/39);
	help4(/*I'm afraid I need some sort of value in order to continue,*/780,
		/*so I've tentatively inserted `0'. You may want to*/781,
		/*delete this zero and insert something else;*/782,
		/*see Chapter 27 of The METAFONTbook for an example.*/783);
	back_input(); cur_sym = 0; cur_cmd = numeric_token; cur_mod = 0; ins_error();
	save_flag = var_flag; var_flag = 0; get_x_next();
	var_flag = save_flag;
}


// 827
void stash_in(pointer p)
{
	pointer q; //{temporary register}
	type(p) = cur_type;
	if (cur_type == known) value(p) = cur_exp;
	else {
		if (cur_type == independent)
			#pragma region <Stash an independent |cur_exp| into a big node>
		{
			q = single_dependency(cur_exp);
			if (q == dep_final)
			{ 
				type(p) = known; value(p) = 0; free_node(q,dep_node_size);
			}
			else  {
				type(p) = dependent; new_dep(p,q);
			}
			recycle_value(cur_exp);
		}
			#pragma endregion
		else {
			mem[value_loc(p)] = mem[value_loc(cur_exp)];
			//{|dep_list(p):=dep_list(cur_exp)| and |prev_dep(p):=prev_dep(cur_exp)|}
			link(prev_dep(p)) = p;
		}
		free_node(cur_exp,value_node_size);
	}
	cur_type = vacuous;
}


// 848
void back_expr()
{
	pointer p; // capsule token

	p = stash_cur_exp(); link(p) = null; back_list(p);
}

// 849
void bad_subscript()
{
	exp_err(/*Improper subscript has been replaced by zero*/784);
	help3(/*A bracketed subscript must have a known numeric value;*/785,
		/*unfortunately, what I found was the value that appears just*/786,
		/*above this error message. So I'll try a zero subscript.*/787); flush_error(0);
}

// 851
void obliterated(pointer q)
{
	print_err(/*Variable */788); show_token_list(q, null, 1000, 0); print(/* has been obliterated*/789);
	help5(/*It seems you did a nasty thing---probably by accident,*/790,
		/*but nevertheless you nearly hornswoggled me...*/791,
		/*While I was evaluating the right-hand side of this*/792,
		/*command, something happened, and the left-hand side*/793,
		/*is no longer a variable! So I won't change anything.*/794);
}


// 855
void make_exp_copy(pointer p)
{
	pointer q,r,t; //{registers for list manipulation}
restart:
	cur_type = type(p);
	switch (cur_type) {
		case vacuous: case boolean_type: case known:
			cur_exp = value(p);
			break;
		case unknown_types:
			cur_exp = new_ring_entry(p);
			break;
		case string_type:
			cur_exp = value(p); add_str_ref(cur_exp);
			break;
		case pen_type:
			cur_exp = value(p); add_pen_ref(cur_exp);
			break;
		case picture_type:
			cur_exp = copy_edges(value(p));
			break;
		case path_type: case future_pen:
			cur_exp = copy_path(value(p));
			break;
		case transform_type: case pair_type:
			#pragma region <Copy the big node |p|>
			if (value(p) == null) init_big_node(p);
			t = get_node(value_node_size); name_type(t) = capsule; type(t) = cur_type; init_big_node(t);
			q = value(p) + big_node_size[cur_type]; r = value(t) + big_node_size[cur_type];
			do { 
				q = q - 2; r = r - 2; install(r,q);
			} while (!(q == value(p)));
			cur_exp = t;
			#pragma endregion
			break;
		case dependent: case proto_dependent:
			encapsulate(copy_dep_list(dep_list(p)));
			break;
		case numeric_type:
			new_indep(p); goto restart;
			break;
		case independent:
			q = single_dependency(p);
			if (q == dep_final)
			{
				cur_type = known; cur_exp = 0; free_node(q,value_node_size);
			}
			else {
				cur_type = dependent; encapsulate(q);
			}
			break;
		default: confusion(/*copy*/795); break;
	}
}

// 856
void encapsulate(pointer p)
{
	cur_exp = get_node(value_node_size); type(cur_exp) = cur_type;
	name_type(cur_exp) = capsule; new_dep(cur_exp,p);
}


// 858
void install(pointer r, pointer q)
{
	pointer p; //{temporary register}
	if (type(q) == known)
	{
		value(r) = value(q); type(r) = known;
	}
	else if (type(q) == independent)
	{ 
		p = single_dependency(q);
		if (p == dep_final)
		{ 
			type(r) = known; value(r) = 0; free_node(p,value_node_size);
		}
		else { 
			type(r) = dependent; new_dep(r,p);
		}
	}
	else { 
		type(r) = type(q); new_dep(r,copy_dep_list(dep_list(q)));
	}
}

// 860
void scan_suffix()
{
	pointer h, t; // head and tail of the list being built
	pointer p; // temporary register

	h = get_avail(); t = h;

	while (true) {
		if (cur_cmd == left_bracket)
			#pragma region <Scan a bracketed subscript and set cur_cmd to numeric_token 861
		{
			get_x_next(); scan_expression();
			if (cur_type != known) bad_subscript();
			if (cur_cmd != right_bracket) {
				missing_err(/*]*/93);
				help3(/*I've seen a `[' and a subscript value, in a suffix,*/796,
					/*so a right bracket should have come next.*/778,
					/*I shall pretend that one was there.*/699);
				back_error();
			}
			cur_cmd = numeric_token; cur_mod = cur_exp;
		}
			#pragma endregion
		if (cur_cmd == numeric_token) p = new_num_tok(cur_mod);
		else if (cur_cmd == tag_token || cur_cmd == internal_quantity) {
			p = get_avail(); info(p) = cur_sym;
		}
		else goto done;
		link(t) = p; t = p; get_x_next();
	}
done:
	cur_exp = link(h); free_avail(h); cur_type = token_list;
}

// 862
void scan_secondary()
{
	pointer p; //{for list manipulation}
	halfword c, d; //{operation codes or modifiers}
	pointer mac_name = max_halfword; //{token defined with \&{primarydef}}
	restart:
	if(cur_cmd < min_primary_command || cur_cmd > max_primary_command)
		bad_exp(/*A secondary*/797);
	scan_primary();
mycontinue:
	if (cur_cmd <= max_secondary_command)
		if (cur_cmd >= min_secondary_command)
		{ 
			p = stash_cur_exp(); c = cur_mod; d = cur_cmd;
			if (d == secondary_primary_macro)
			{
				mac_name = cur_sym; add_mac_ref(c);
			}
			get_x_next(); scan_primary();
			if (d != secondary_primary_macro) do_binary(p,(quarterword)c);
			else  {
				back_input(); binary_mac(p,c,mac_name);
				ref_count(c)--; get_x_next(); goto restart;
			}
			goto mycontinue;
		}
}


// 863
void binary_mac(pointer p, pointer c, pointer n)
{
	pointer q, r; // nodes in the parameter list

	q = get_avail(); r = get_avail(); link(q) = r;
	info(q) = p; info(r) = stash_cur_exp();
	macro_call(c, q, n);
}

// 864
void scan_tertiary()
{
	pointer p; //{for list manipulation}
	halfword c,d; //{operation codes or modifiers}
	pointer mac_name=max_halfword; //{token defined with \&{secondarydef}}

	restart:
	if(cur_cmd < min_primary_command || cur_cmd > max_primary_command)
		bad_exp(/*A tertiary*/798);
	scan_secondary();
	if (cur_type == future_pen) materialize_pen();
mycontinue: 
	if (cur_cmd <= max_tertiary_command)
		if (cur_cmd >= min_tertiary_command)
	{ 
		p = stash_cur_exp(); c = cur_mod; d = cur_cmd;
		if (d == tertiary_secondary_macro)
		{ 
			mac_name = cur_sym; add_mac_ref(c);
		}
		get_x_next(); scan_secondary();
		if (d != tertiary_secondary_macro) do_binary(p,(quarterword)c);
		else  { 
			back_input(); binary_mac(p,c,mac_name);
			ref_count(c)--; get_x_next(); goto restart;
		}
		goto mycontinue;
	}
}


// 865
void materialize_pen()
{
	scaled a_minus_b,a_plus_b,major_axis,minor_axis; //{ellipse variables}
	angle theta; //{amount by which the ellipse has been rotated}
	pointer p; //{path traverser}
	pointer q; //{the knot list to be made into a pen}

	q = cur_exp;
	if (left_type(q) == endpoint)
	{ 
		print_err(/*Pen path must be a cycle*/799);
		help2(/*I can't make a pen from the given path.*/800,
			/*So I've replaced it by the trivial path `(0,0)..cycle'.*/496); put_get_error();
		cur_exp = null_pen; goto common_ending;
	}
	else if (left_type(q) == _open_)
		#pragma region <Change node q to a path for an elliptical pen 866>
	{ 
			tx = x_coord(q); ty = y_coord(q); txx = left_x(q) - tx; tyx = left_y(q) - ty;
			txy = right_x(q) - tx; tyy = right_y(q) - ty; a_minus_b = pyth_add(txx - tyy,tyx + txy);
			a_plus_b = pyth_add(txx + tyy,tyx - txy); major_axis = half(a_minus_b + a_plus_b);
			minor_axis = half (myabs(a_plus_b - a_minus_b));
			if (major_axis == minor_axis) theta = 0; //{circle}
			else theta = half(n_arg(txx - tyy,tyx + txy) + n_arg(txx + tyy,tyx - txy));
			free_node(q,knot_node_size); q = make_ellipse(major_axis,minor_axis,theta);
			if (tx != 0 || ty != 0) 
				#pragma region <Shift the coordinates of path q 867>
			{
					p = q;
					do {
						x_coord(p) = x_coord(p) + tx; y_coord(p) = y_coord(p) + ty; p = link(p);
					} while (!(p == q));
			}
				#pragma endregion
	}


		#pragma endregion

	cur_exp = make_pen(q);
common_ending: toss_knot_list(q); cur_type = pen_type;
}




// 868
void scan_expression()
{
	pointer p, q, r, pp, qq; //{for list manipulation}
	halfword c, d; //{operation codes or modifiers}
	int my_var_flag;//:0..max_command_code; //{initial value of |var_flag|}
	pointer mac_name = max_halfword; //{token defined with \&{tertiarydef}}
	bool cycle_hit; //{did a path expression just end with `\&{cycle}'?}
	scaled x=INT_MAX, y=INT_MAX; //{explicit coordinates or tension at a path join}
	int t=INT_MAX;//:endpoint..open; //{knot type following a path join}

	my_var_flag = var_flag;
restart:
	if(cur_cmd < min_primary_command || cur_cmd > max_primary_command)
		bad_exp(/*An*/801);
	scan_tertiary();
mycontinue:
	if (cur_cmd <= max_expression_command)
		if (cur_cmd >= min_expression_command)
			if (cur_cmd != equals || my_var_flag != assignment)
			{ 
				p = stash_cur_exp(); c = cur_mod; d = cur_cmd;
				if (d == expression_tertiary_macro)
				{ 
					mac_name = cur_sym; add_mac_ref(c);
				}
				if (d < ampersand || (d == ampersand && ((type(p) == pair_type || type(p) == path_type))))
					#pragma region <Scan a path construction operation; but |return| if |p| has the wrong type>
				{ 
					cycle_hit = false;
					#pragma region <Convert the left operand, |p|, into a partial path ending at|q|;but |return| if |p| doesnt have a suitable type>
					{ 
						unstash_cur_exp(p);
						if (cur_type == pair_type) p = new_knot();
						else if (cur_type == path_type) p = cur_exp;
						else return;
						q = p;
						while (link(q) != p) q = link(q);
						if (left_type(p) != endpoint) //{open up a cycle}
						{ 
							r = copy_knot(p); link(q) = r; q = r;
						}
						left_type(p) = _open_; right_type(q) = _open_;
					}				
					#pragma endregion
				continue_path: 
					#pragma region <Determine the path join parameters;but |goto finish_path| if theres only a direction specifier>
					if (cur_cmd == left_brace)
						#pragma region <Put the pre-join direction information into node |q|>
					{ 
						t = scan_direction();
						if (t != _open_)
						{ 
							right_type(q) = t; right_given(q) = cur_exp;
							if (left_type(q) == _open_)
							{ 
								left_type(q) = t; left_given(q) = cur_exp;
							} //{note that |left_given(q)=left_curl(q)|}
						}
					}			
						#pragma endregion
					d = cur_cmd;
					if (d == path_join) 
						#pragma region <Determine the tension and/or control points>
					{ 
						get_x_next();
						if (cur_cmd == tension) 
							#pragma region <Set explicit tensions>
						{ 
							get_x_next(); y = cur_cmd;
							if (cur_cmd == at_least) get_x_next();
							scan_primary();
							#pragma region <Make sure that the current expression is a valid tension setting>
							if (cur_type != known || cur_exp < min_tension)
							{ 
								exp_err(/*Improper tension has been set to 1*/802);
								help1(/*The expression above should have been a number >=3/4.*/803);
								put_get_flush_error(unity);
							}						
							#pragma endregion
							if (y == at_least) negate(cur_exp);
							right_tension(q) = cur_exp;
							if (cur_cmd == and_command)
							{ 
								get_x_next(); y = cur_cmd;
								if (cur_cmd == at_least) get_x_next();
								scan_primary();
								#pragma region <Make sure that the current expression is a valid tension setting>
								if (cur_type != known || cur_exp < min_tension)
								{ 
									exp_err(/*Improper tension has been set to 1*/802);
									help1(/*The expression above should have been a number >=3/4.*/803);
									put_get_flush_error(unity);
								}						
								#pragma endregion
								if (y == at_least) negate(cur_exp);
							}
							y = cur_exp;
						}						
							#pragma endregion
						else if (cur_cmd == controls) 
							#pragma region <Set explicit control points>
						{ 
							right_type(q) = _explicit; t = _explicit; get_x_next(); scan_primary();
							known_pair(); right_x(q) = cur_x; right_y(q) = cur_y;
							if (cur_cmd != and_command)
							{ 
								x = right_x(q); y = right_y(q);
							}
							else { 
								get_x_next(); scan_primary();
								known_pair(); x = cur_x; y = cur_y;
							}
						}
							#pragma endregion
						else { 
							right_tension(q) = unity; y = unity; back_input(); //{default tension}
							goto done;
						}
						if (cur_cmd != path_join)
						{ 
							missing_err(/*..*/416);
							help1(/*A path join command should end with two dots.*/804);
							back_error();
						}
					done: ;
					}
						#pragma endregion
					else if (d != ampersand) goto finish_path;
					get_x_next();
					if (cur_cmd == left_brace)
						#pragma region <Put the post-join direction information into |x| and |t|>
					{ 
						t = scan_direction();
						if (right_type(q) != _explicit) x = cur_exp;
						else t = _explicit; //{the direction information is superfluous}
					}
						#pragma endregion
					else if (right_type(q) != _explicit)
					{ 
						t = _open_; x = 0;
					}
					#pragma endregion
					if (cur_cmd == cycle) 
						#pragma region <Get ready to close a cycle>
					{ 
						cycle_hit = true; get_x_next(); pp = p; qq = p;
						if (d == ampersand) 
							if (p == q)
							{ 
								d = path_join; right_tension(q) = unity; y = unity;
							}
					}
						#pragma endregion
					else { 
						scan_tertiary();
						#pragma region <Convert the right operand, |cur_exp|,into a partial path from |pp| to~|qq|>
						{ 
							if (cur_type != path_type) pp = new_knot();
							else pp = cur_exp;
							qq = pp;
							while (link(qq) != pp) qq = link(qq);
							if (left_type(pp) != endpoint) //{open up a cycle}
							{ 
								r = copy_knot(pp); link(qq) = r; qq = r;
							}
							left_type(pp) = _open_; right_type(qq) = _open_;
						}						
						#pragma endregion
					}
					#pragma region <Join the partial paths and reset |p| and |q| to the head and tail of the result>
					{ 
						if (d == ampersand)
							if (x_coord(q) != x_coord(pp) || y_coord(q) != y_coord(pp))
							{ 
								print_err(/*Paths don't touch; `&' will be changed to `..'*/805);
								help3(/*When you join paths `p&q', the ending point of p*/806,
									/*must be exactly equal to the starting point of q.*/807,
									/*So I'm going to pretend that you said `p..q' instead.*/808);
								put_get_error(); d = path_join; right_tension(q) = unity; y = unity;
							}
						#pragma region <Plug an opening in |right_type(pp)|, if possible>
						if (right_type(pp) == _open_)
							if (t == curl || t == given)
							{ 
								right_type(pp) = t; right_given(pp) = x;
							}					
						#pragma endregion
						
						if (d == ampersand) 
							#pragma region <Splice independent paths together>
						{ 
							if (left_type(q) == _open_) 
								if (right_type(q) == _open_)
								{ 
									left_type(q) = curl; left_curl(q) = unity;
								}
							if (right_type(pp) == _open_) 
								if (t == _open_)
								{ 
									right_type(pp) = curl; right_curl(pp) = unity;
								}
							right_type(q) = right_type(pp); link(q) = link(pp);
							right_x(q) = right_x(pp); right_y(q) = right_y(pp);
							free_node(pp,knot_node_size);
							if (qq == pp) qq = q;
						}	
							#pragma endregion
						else  { 
							#pragma region <Plug an opening in |right_type(q)|, if possible>
							if (right_type(q) == _open_)
								if (left_type(q) == curl || left_type(q) == given)
								{ 
									right_type(q) = left_type(q); right_given(q) = left_given(q);
								}						
							#pragma endregion
							link(q) = pp; left_y(pp) = y;
							if (t != _open_)
							{ 
								left_x(pp) = x; left_type(pp) = t;
							}
						}
						q = qq;
					}
					#pragma endregion
					if (cur_cmd >= min_expression_command)
						if (cur_cmd <= ampersand)
							if (!cycle_hit) goto continue_path;
				finish_path:
					#pragma region <Choose control points for the path and put the result into |cur_exp|>
					if (cycle_hit)
					{ 
						if (d == ampersand) p = q;
					}
					else { 
						left_type(p) = endpoint;
						if (right_type(p) == _open_)
						{ 
							right_type(p) = curl; right_curl(p) = unity;
						}
						right_type(q) = endpoint;
						if (left_type(q) == _open_)
						{ 
							left_type(q) = curl; left_curl(q) = unity;
						}
						link(q) = p;
					}
					make_choices(p);
					cur_type = path_type; cur_exp = p;
					#pragma endregion
				}
					#pragma endregion
				else { 
					get_x_next(); scan_tertiary();
					if (d != expression_tertiary_macro) do_binary(p,(quarterword)c);
					else {
						back_input(); binary_mac(p,c,mac_name);
						ref_count(c)--; get_x_next(); goto restart;
					}
				}
				goto mycontinue;
			}
}



// 871
pointer new_knot() // convert a pair to a knot with two endpoints
{
	pointer q; // the new node
	q = get_node(knot_node_size); left_type(q) = endpoint; right_type(q) = endpoint; link(q) = q;
	known_pair(); x_coord(q) = cur_x; y_coord(q) = cur_y; return q;
}

// 872
void known_pair()
{
	pointer p; //{the pair node}
	if (cur_type != pair_type)
	{ 
		exp_err(/*Undefined coordinates have been replaced by (0,0)*/809);
		help5(/*I need x and y numbers for this part of the path.*/810,
		/*The value I found (see above) was no good;*/811,
		/*so I'll try to keep going by using zero instead.*/812,
		/*(Chapter 27 of The METAFONTbook explains that*/813,
		/*you might want to type `I ???' now.)*/814); put_get_flush_error(0); cur_x = 0; cur_y = 0;
	}
	else { 
		p = value(cur_exp);
		#pragma region <Make sure that both x and y_parts of p are known; copy them into cur_x and cur_y 873>
		
		if (type(x_part_loc(p)) == known) cur_x = value(x_part_loc(p));
		else { 
			disp_err(x_part_loc(p),/*Undefined x coordinate has been replaced by 0*/815);
			help5(/*I need a `known' x value for this part of the path.*/816,
			/*The value I found (see above) was no good;*/811,
			/*so I'll try to keep going by using zero instead.*/812,
			/*(Chapter 27 of The METAFONTbook explains that*/813,
			/*you might want to type `I ???' now.)*/814); put_get_error(); recycle_value(x_part_loc(p));
			cur_x = 0;
		}
		if (type(y_part_loc(p)) == known) cur_y = value(y_part_loc(p));
		else { 
			disp_err(y_part_loc(p),/*Undefined y coordinate has been replaced by 0*/817);
			help5(/*I need a `known' y value for this part of the path.*/818,
			/*The value I found (see above) was no good;*/811,
			/*so I'll try to keep going by using zero instead.*/812,
			/*(Chapter 27 of The METAFONTbook explains that*/813,
			/*you might want to type `I ???' now.)*/814); put_get_error(); recycle_value(y_part_loc(p));
			cur_y = 0;
		}		
		
		
		#pragma endregion
		flush_cur_exp(0);
	}
}

// 875
small_number scan_direction()
{
	int t; // given..open, the type of information found
	scaled x; // an x coordinate

	get_x_next();
	if (cur_cmd == curl_command)
		#pragma region <Scan a curl specification 876>
	{
		get_x_next(); scan_expression();
		if (cur_type != known || cur_exp < 0) {
			exp_err(/*Improper curl has been replaced by 1*/819);
			help1(/*A curl must be a known, nonnegative number.*/820); put_get_flush_error(unity);
		}
		t = curl;
	}
		#pragma endregion
	else
		#pragma region <Scan a given direction 877>
	{
		scan_expression();
		if (cur_type > pair_type)
			#pragma region <Get given directions separated by commas 878>
		{
			if (cur_type != known) {
				exp_err(/*Undefined x coordinate has been replaced by 0*/815);
				help5(/*I need a `known' x value for this part of the path.*/816,
					/*The value I found (see above) was no good;*/811,
					/*so I'll try to keep going by using zero instead.*/812,
					/*(Chapter 27 of The METAFONTbook explains that*/813,
					/*you might want to type `I ???' now.)*/814); put_get_flush_error(0);
			}
			x = cur_exp;
			if (cur_cmd != comma) {
				missing_err(/*,*/44);
				help2(/*I've got the x coordinate of a path direction;*/821,
					/*will look for the y coordinate next.*/822); back_error();
			}
			get_x_next(); scan_expression();
			if (cur_type != known) {
				exp_err(/*Undefined y coordinate has been replaced by 0*/817);
				help5(/*I need a `known' y value for this part of the path.*/818,
					/*The value I found (see above) was no good;*/811,
					/*so I'll try to keep going by using zero instead.*/812,
					/*(Chapter 27 of The METAFONTbook explains that*/813,
					/*you might want to type `I ???' now.)*/814); put_get_flush_error(0);
			}
			cur_y = cur_exp; cur_x = x;
		}
			#pragma endregion
		else known_pair();
		if (cur_x == 0 && cur_y == 0) t = _open_;
		else {
			t = given; cur_exp = n_arg(cur_x, cur_y);
		}
	}
		#pragma endregion
	if (cur_cmd != right_brace) {
		missing_err(/*}*/125);
		help3(/*I've scanned a direction spec for part of a path,*/823,
			/*so a right brace should have come next.*/824,
			/*I shall pretend that one was there.*/699);
		back_error();
	}
	get_x_next(); return t;
}

// 892
void get_boolean()
{
	get_x_next(); scan_expression();
	if (cur_type != boolean_type)
	{ 
		exp_err(/*Undefined condition will be treated as `false'*/825);
		help2(/*The expression shown above should have had a definite*/826,
			/*true-or-false value. I'm changing it to `false'.*/827);
		put_get_flush_error(false_code); cur_type = boolean_type;
	}
}


// 895
void do_nullary(quarterword c)
{
	int k; // {all - purpose loop index}
	check_arith;
	if (internal[tracing_commands] > two) show_cmd_mod(nullary, c);
	switch( c ) {
	case true_code:
	case false_code: 
		cur_type = boolean_type; cur_exp = c;
		break;
	case null_picture_code: 
		cur_type = picture_type; cur_exp = get_node(edge_header_size);
		init_edges(cur_exp);
		break;
	case null_pen_code: 
		cur_type = pen_type; cur_exp = null_pen;
		break;
	case normal_deviate: 
		cur_type = known; cur_exp = norm_rand();
		break;
	case pen_circle: 
		#pragma region <Make a special knot node for pencircle 896>
		cur_type = future_pen; cur_exp = get_node(knot_node_size); left_type(cur_exp) = _open_;
		right_type(cur_exp) = _open_; link(cur_exp) = cur_exp;
		x_coord(cur_exp) = 0; y_coord(cur_exp) = 0;
		left_x(cur_exp) = unity; left_y(cur_exp) = 0;
		right_x(cur_exp) = 0; right_y(cur_exp) = unity;
		#pragma endregion
		break;
	case job_name_op: 
		if (job_name == 0) open_log_file();
		cur_type = string_type; cur_exp = job_name;
		break;
	case read_string_op: 
		#pragma region <Read a string from the terminal 897>
		if (interaction <= nonstop_mode)
			fatal_error(/**** (cannot readstring in nonstop modes)*/828);
		begin_file_reading(); name = 1; prompt_input(/**/289); str_room(last - start);
		for (k = start; k <= last - 1; k++) append_char(buffer[k]);
		end_file_reading(); cur_type = string_type; cur_exp = make_string();
		#pragma endregion
		break;
	}	// {there are no other cases}
	check_arith;
}

// 898
void do_unary(quarterword c)
{
	pointer p, q; //{for list manipulation}
	int x; //{a temporary register}
	check_arith;
	if (internal[tracing_commands] > two)
	#pragma region <Trace the current unary operation>
	{
		begin_diagnostic(); print_nl(/*{*/123); print_op(c); print_char(/*(*/40);
		print_exp(null,0); //{show the operand, but not verbosely}
		print(/*)}*/829); end_diagnostic(false);
	}
	#pragma endregion
	switch(c) {
		case plus:
			if (cur_type < pair_type)
				if (cur_type != picture_type) bad_unary(plus);
			break;
		case minus:
			#pragma region <Negate the current expression>
			switch(cur_type) {
			case pair_type: case independent:
				q = cur_exp; make_exp_copy(q);
				if (cur_type == dependent) negate_dep_list(dep_list(cur_exp));
				else if (cur_type == pair_type)
				{
					p = value(cur_exp);
					if (type(x_part_loc(p)) == known) negate(value(x_part_loc(p)));
					else negate_dep_list(dep_list(x_part_loc(p)));
					if (type(y_part_loc(p)) == known) negate(value(y_part_loc(p)));
					else negate_dep_list(dep_list(y_part_loc(p)));
				} //{if |cur_type=known| then |cur_exp=0|}
				recycle_value(q); free_node(q,value_node_size);
				break;
			case dependent: case proto_dependent:
				negate_dep_list(dep_list(cur_exp));
				break;
			case known:
				negate(cur_exp);
				break;
			case picture_type:
				negate_edges(cur_exp);
				break;
			default: 
				bad_unary(minus);
				break;
			}
			#pragma endregion
			break;
		#pragma region <Additional cases of unary operators>
		// 905
		case not_op: 
			if (cur_type != boolean_type) bad_unary(not_op);
			else cur_exp = true_code + false_code - cur_exp;
			break;
		// 906
		case sqrt_op: case m_exp_op: case m_log_op: case sin_d_op: case cos_d_op: case floor_op:
		case uniform_deviate: case odd_op: case char_exists_op:
			if (cur_type != known) bad_unary(c);
			else switch(c) {
				case sqrt_op:
					cur_exp = square_rt(cur_exp);
					break;
				case m_exp_op:
					cur_exp = m_exp(cur_exp);
					break;
				case m_log_op:
					cur_exp = m_log(cur_exp);
					break;
				case sin_d_op: case cos_d_op:
					n_sin_cos((cur_exp % three_sixty_units) * 16);
					if (c == sin_d_op) cur_exp = round_fraction(n_sin);
					else cur_exp = round_fraction(n_cos);
					break;
				case floor_op:
					cur_exp = floor_scaled(cur_exp);
					break;
				case uniform_deviate:
					cur_exp = unif_rand(cur_exp);
					break;
				case odd_op:
					boolean_reset(myodd(round_unscaled(cur_exp)));
					cur_type = boolean_type;
					break;
				case char_exists_op:
					#pragma region <Determine if a character has been shipped out>
					cur_exp = round_unscaled(cur_exp) % 256;
					if (cur_exp < 0) cur_exp = cur_exp + 256;
					boolean_reset(char_exists[cur_exp]); cur_type = boolean_type;
					#pragma endregion
					break;
			} //{there are no other cases}	
			break;
		// 907
		case angle_op:
			if (nice_pair(cur_exp,cur_type))
			{
				p = value(cur_exp);
				x = n_arg(value(x_part_loc(p)),value(y_part_loc(p)));
				if (x >= 0) flush_cur_exp((x + 8) / 16);
				else flush_cur_exp(-((-x + 8) / 16));
			}
			else bad_unary(angle_op);
			break;
		// 909
		case x_part: case y_part:
			if (cur_type <= pair_type && cur_type >= transform_type)
			take_part(c);
			else bad_unary(c);
			break;
		case xx_part: case xy_part: case yx_part: case yy_part: 
			if (cur_type == transform_type) take_part(c);
			else bad_unary(c);
			break;
		
		// 912
		case char_op:
			if (cur_type != known) bad_unary(char_op);
			else {
				cur_exp = round_unscaled(cur_exp) % 256; cur_type = string_type;
				if (cur_exp < 0) cur_exp = cur_exp + 256;
				if (length(cur_exp) != 1)
				{
					str_room(1); append_char(cur_exp); cur_exp = make_string();
				}
			}
			break;
		case decimal:
			if (cur_type != known) bad_unary(decimal);
			else {
				old_setting = selector; selector = new_string;
				print_scaled(cur_exp); cur_exp = make_string();
				selector = old_setting; cur_type = string_type;
			}
			break;
		case oct_op: case hex_op: case ASCII_op:
			if (cur_type != string_type) bad_unary(c);
			else str_to_num(c);
			break;
		
		// 915
		case length_op:
			if (cur_type == string_type) flush_cur_exp(length(cur_exp)*unity);
			else if (cur_type == path_type) flush_cur_exp(path_length());
			else if (cur_type == known) cur_exp = myabs(cur_exp);
			else if (nice_pair(cur_exp,cur_type))
				flush_cur_exp(pyth_add(value(x_part_loc(value(cur_exp))),value(y_part_loc(value(cur_exp)))));
			else bad_unary(c);
			break;
			
		// 917
		case turning_op:
			if (cur_type == pair_type) flush_cur_exp(0);
			else if (cur_type != path_type) bad_unary(turning_op);
			else if (left_type(cur_exp) == endpoint)
			flush_cur_exp(0); //{not a cyclic path}
			else {
				cur_pen = null_pen; cur_path_type = contour_code;
				cur_exp = make_spec(cur_exp, fraction_one - half_unit - 1 - el_gordo, 0);
				flush_cur_exp(turning_number*unity); //{convert to |scaled|}
			}
			break;
			
		
		// 918
		case boolean_type: 
			type_range(boolean_type,unknown_boolean);
			break;
		case string_type: 
			type_range(string_type,unknown_string);
			break;
		case pen_type: 
			type_range(pen_type,future_pen);
			break;
		case path_type:
			type_range(path_type,unknown_path);
			break;
		case picture_type:
			type_range(picture_type,unknown_picture);
			break;
		case transform_type: case pair_type: 
			type_test(c);
			break;
		case numeric_type: 
			type_range(known,independent);
			break;
		case known_op: case unknown_op: 
			test_known(c);
			break;
		
		// 920
		case cycle_op: 
			if (cur_type != path_type) flush_cur_exp(false_code);
			else if (left_type(cur_exp) != endpoint) flush_cur_exp(true_code);
			else flush_cur_exp(false_code);
			cur_type = boolean_type;
			break;
			
		// 921
		case make_pen_op: 
			if (cur_type == pair_type) pair_to_path();
			if (cur_type == path_type) cur_type = future_pen;
			else bad_unary(make_pen_op);
			break;
		case make_path_op:
			if (cur_type == future_pen) materialize_pen();
			if (cur_type != pen_type) bad_unary(make_path_op);
			else {
				flush_cur_exp(make_path(cur_exp)); cur_type = path_type;
			}
			break;
		case total_weight_op: 
			if (cur_type != picture_type) bad_unary(total_weight_op);
			else flush_cur_exp(total_weight(cur_exp));
			break;
		case reverse: 
			if (cur_type == path_type)				
			{
				p = htap_ypoc(cur_exp);
				if (right_type(p) == endpoint) p = link(p);
				toss_knot_list(cur_exp); cur_exp = p;
			}
			else if (cur_type == pair_type) pair_to_path();
			else bad_unary(reverse);
			break;
			
			
	#pragma endregion
	} //{there are no other cases}
	check_arith;
}


// 899
bool nice_pair(int p, quarterword t)
{
	if (t == pair_type) {
		p = value(p);
		if (type(x_part_loc(p)) == known)
			if (type(y_part_loc(p)) == known) {
				return true;
			}
	}
	return false;
}

// 900
void print_known_or_unknown_type(small_number t, int v)
{
	print_char(/*(*/40);
	if (t < dependent)
		if (t != pair_type) print_type(t);
		else if (nice_pair(v, pair_type)) print(/*pair*/344);
		else print(/*unknown pair*/830);
	else print(/*unknown numeric*/831);
	print_char(/*)*/41);
}

// 901
void bad_unary(quarterword c)
{
	exp_err(/*Not implemented: */832); print_op(c); print_known_or_unknown_type(cur_type, cur_exp);
	help3(/*I'm afraid I don't know how to apply that operation to that*/833,
		/*particular type. Continue, and I'll simply return the*/834,
		/*argument (shown above) as the result of the operation.*/835); put_get_error();
}

// 904
void negate_dep_list(pointer p)
{
	while (true) {
		negate(value(p));
		if (info(p) == null) return;
		p = link(p);
	}
}

// 908
void pair_to_path()
{
	cur_exp = new_knot(); cur_type = path_type;
}

// 910
void take_part(quarterword c)
{
	pointer p; // the big node
	p = value(cur_exp); value(temp_val) = p; type(temp_val) = cur_type; link(p) = temp_val;
	free_node(cur_exp, value_node_size); make_exp_copy(p + 2 * (c - x_part)); recycle_value(temp_val);
}


// 913
void str_to_num(quarterword c) // {converts a string to a number}
{
	int n; // {accumulator}
	ASCII_code m ; // {current character}
	pool_pointer k; // {index into str pool }
	int b; // 8 .. 16, {radix of conversion}
	bool bad_char; // {did the string contain an invalid digit=}
	if (c == ASCII_op)
		if (length(cur_exp) == 0) n = -1;
		else n = str_pool[str_start[cur_exp]];
	else { 
		if (c == oct_op) b = 8; else b = 16;
		n = 0; bad_char = false;
		for (k = str_start[cur_exp]; k <= str_start[cur_exp + 1] - 1; k++)
		{ 
			m = str_pool[k];
			if (m >= /*0*/48 && m <= /*9*/57) m = m - /*0*/48;
			else if (m >= /*A*/65 && m <= /*F*/70) m = m - /*A*/65 + 10;
			else if (m >= /*a*/97 && m <= /*f*/102) m = m - /*a*/97 + 10;
			else { 
				bad_char = true; m = 0;
			}
			if (m == b)
			{ 
				bad_char = true; m = 0;
			}
			if (n < 32768 / b) n = n * b + m; else n = 32767;
		}
		#pragma region <Give error messages if bad char or n = 4096 914>
		if (bad_char) {
			exp_err(/*String contains illegal digits*/836);
			if (c == oct_op) help1(/*I zeroed out characters that weren't in the range 0..7.*/837);
			else help1(/*I zeroed out characters that weren't hex digits.*/838);
			put_get_error();
		}
		if (n > 4095) {
			print_err(/*Number too large (*/839); print_int(n); print_char(/*)*/41);
			help1(/*I have trouble with numbers greater than 4095; watch out.*/840); put_get_error();
		}
		#pragma endregion
	}
	flush_cur_exp(n * unity);
}





// 916
scaled path_length()
{
	scaled n; // the path length so far
	pointer p; // traverser

	p = cur_exp;
	if (left_type(p) == endpoint) n = -unity; else n = 0;
	do {
		p = link(p); n = n + unity;
	} while (!(p == cur_exp));
	return n;
}

// 919
void test_known(quarterword c)
{
	int b; // true_code..false_code, is the current expression known?
	pointer p, q; // locations in a big node

	b = false_code;
	switch(cur_type) {
		case vacuous:
		case boolean_type:
		case string_type:
		case pen_type:
		case future_pen:
		case path_type:
		case picture_type:
		case known:
			b = true_code;
			break;
		case transform_type:
		case pair_type:
			p = value(cur_exp); q = p + big_node_size[cur_type];
			do {
				q = q - 2;
				if (type(q) != known) goto done;
			} while (!(q == p));
			b = true_code;
		done:
			break;
		default:
			// do_nothing
			break;
	}
	if (c == known_op) flush_cur_exp(b);
	else flush_cur_exp(true_code + false_code - b);
	cur_type = boolean_type;

}

// 922
void do_binary(pointer p, quarterword c)
{
	pointer q, r, rr; //{for list manipulation}
	pointer old_p, old_exp; //{capsules to recycle}
	int v; //{for numeric manipulation}

	check_arith;
	if (internal[tracing_commands] > two)
		#pragma region <Trace the current binary operation>
	{ 
		begin_diagnostic(); print_nl(/*{(*/841);
		print_exp(p,0); //{show the operand, but not verbosely}
		print_char(/*)*/41); print_op(c); print_char(/*(*/40);
		print_exp(null,0); print(/*)}*/829); end_diagnostic(false);
	}
		#pragma endregion

	#pragma region <Sidestep |independent| cases in capsule |p|>
	switch(type(p)) {
		case transform_type: case pair_type: 
			old_p = tarnished(p);
			break;
		case independent:
			old_p = _void;
			break;
		default: old_p = null; break;
	}
	if (old_p != null)
	{ 
		q = stash_cur_exp(); old_p = p; make_exp_copy(old_p);
		p = stash_cur_exp(); unstash_cur_exp(q);
	}
	#pragma endregion

	#pragma region <Sidestep |independent| cases in the current expression>
	switch(cur_type) {
		case transform_type: case pair_type:
			old_exp = tarnished(cur_exp);
			break;
		case independent:
			old_exp = _void;
			break;
		default: old_exp = null; break;
	}
	if (old_exp != null)
	{ 
		old_exp = cur_exp; make_exp_copy(old_exp);
	}
	#pragma endregion

	switch(c) {
		case plus: case minus:
			#pragma region <Add or subtract the current expression from |p|>
			if (cur_type < pair_type || type(p) < pair_type)
				if (cur_type == picture_type && type(p) == picture_type)
				{ 
					if (c == minus) negate_edges(cur_exp);
					cur_edges = cur_exp; merge_edges(value(p));
				}
				else bad_binary(p,c);
			else if (cur_type == pair_type)
				if (type(p) != pair_type) bad_binary(p,c);
				else { 
					q = value(p); r = value(cur_exp);
					add_or_subtract(x_part_loc(q),x_part_loc(r),c);
					add_or_subtract(y_part_loc(q),y_part_loc(r),c);
				}
			else if (type(p) == pair_type) bad_binary(p,c);
			else add_or_subtract(p,null,c);
			#pragma endregion
			break;
			
		#pragma region <Additional cases of binary operators>
		
		case less_than: case less_or_equal: case greater_than: case greater_or_equal: case equal_to: case unequal_to:
			if (cur_type > pair_type && type(p) > pair_type)
				add_or_subtract(p,null,minus); //{|cur_exp:=(p)-cur_exp|}
			else if (cur_type != type(p))
			{ 
				bad_binary(p,c); goto done;
			}
			else if (cur_type == string_type)
				flush_cur_exp(str_vs_str(value(p),cur_exp));
			else if (cur_type == unknown_string || cur_type == unknown_boolean)
				#pragma region <Check if unknowns have been equated>
			{ 
				q = value(cur_exp);
				while (q != cur_exp && q != p) q = value(q);
				if (q == p) flush_cur_exp(0);
			}
				#pragma endregion
			else if (cur_type == pair_type || cur_type == transform_type)
				#pragma region <Reduce comparison of big nodes to comparison of scalars>
			{ 
				q = value(p); r = value(cur_exp);
				rr = r + big_node_size[cur_type] - 2;
				while (true) {
					add_or_subtract(q,r,minus);
					if (type(r) != known) goto done1;
					if (value(r) != 0) goto done1;
					if (r == rr) goto done1;
					q = q + 2; r = r + 2;
				}
			done1:
				take_part(x_part + half(r - value(cur_exp)));
			}
				#pragma endregion
			else if (cur_type == boolean_type) flush_cur_exp(cur_exp - value(p));
			else {
				bad_binary(p,c); goto done;
			}
			#pragma region <Compare the current expression with zero>
			if (cur_type != known)
			{
				if (cur_type < known)
				{ 
					disp_err(p,/**/289);
					help1(/*The quantities shown above have not been equated.*/842);
				}
				else help2(/*Oh dear. I can't decide if the expression above is positive,*/843,
								/*negative, or zero. So this comparison test won't be `true'.*/844);
				exp_err(/*Unknown relation will be considered false*/845);
				put_get_flush_error(false_code);
			}
			else switch(c) {
				case less_than: boolean_reset(cur_exp < 0); break;
				case less_or_equal: boolean_reset(cur_exp <= 0); break;
				case greater_than: boolean_reset(cur_exp > 0); break;
				case greater_or_equal: boolean_reset(cur_exp >= 0); break;
				case equal_to: boolean_reset(cur_exp == 0); break;
				case unequal_to: boolean_reset(cur_exp != 0); break;
			} //{there are no other cases}
			cur_type = boolean_type;
			#pragma endregion
		done:
			break;
			
			
		case and_op: case or_op: 
			if (type(p) != boolean_type || cur_type != boolean_type)
				bad_binary(p,c);
			else if (value(p) == c + false_code - and_op) cur_exp = value(p);
			break;
			
			
		case times: 
			if (cur_type < pair_type || type(p) < pair_type) bad_binary(p,times);
			else if (cur_type == known || type(p) == known)
				#pragma region <Multiply when at least one operand is known>
			{ 
				if (type(p) == known)
				{ 
					v = value(p); free_node(p,value_node_size);
				}
				else  { 
					v = cur_exp; unstash_cur_exp(p);
				}
				if (cur_type == known) cur_exp = take_scaled(cur_exp,v);
				else if (cur_type == pair_type)
				{ 
					p = value(cur_exp);
					dep_mult(x_part_loc(p),v,true);
					dep_mult(y_part_loc(p),v,true);
				}
				else dep_mult(null,v,true);
				goto _exit;
			}
				#pragma endregion
			else if ((nice_pair(p,type(p)) && cur_type > pair_type) || (nice_pair(cur_exp,cur_type) && type(p) >pair_type))
			{ 
				hard_times(p); goto _exit;
			}
			else bad_binary(p,times);
			break;
			
			
		case over: 
			if (cur_type != known || type(p) < pair_type) bad_binary(p,over);
			else  {
				v = cur_exp; unstash_cur_exp(p);
				if (v == 0) 
					#pragma region <Squeal about division by zero>
				{ 
					exp_err(/*Division by zero*/771);
					help2(/*You're trying to divide the quantity shown above the error*/846,
						/*message by zero. I'm going to divide it by one instead.*/847);
					put_get_error();
				}
					#pragma endregion
				else { 
					if (cur_type == known) cur_exp = make_scaled(cur_exp,v);
					else if (cur_type == pair_type)
					{ 
						p = value(cur_exp);
						dep_div(x_part_loc(p),v);
						dep_div(y_part_loc(p),v);
					}
					else dep_div(null,v);
				}
				goto _exit;
			}
			break;
			
				
		case pythag_add: case pythag_sub: 
			if (cur_type == known && type(p) == known)
				if (c == pythag_add) cur_exp = pyth_add(value(p),cur_exp);
				else cur_exp = pyth_sub(value(p),cur_exp);
			else bad_binary(p,c);
			break;
			
			
		case rotated_by: case slanted_by: case scaled_by: case shifted_by: case transformed_by:
		case x_scaled: case y_scaled: case z_scaled:
			if (type(p) == path_type || type(p) == future_pen || type(p) == pen_type)
			{ 
				path_trans(p,c); goto _exit;
			}
			else if (type(p) == pair_type || type(p) == transform_type) big_trans(p,c);
			else if (type(p) == picture_type)
			{ 
				edges_trans(p,c); goto _exit;
			}
			else bad_binary(p,c);
			break;
			
			
		case concatenate: 
			if (cur_type == string_type && type(p) == string_type) cat(p);
			else bad_binary(p,concatenate);
			break;
		case substring_of: 
			if (nice_pair(p,type(p)) && cur_type == string_type)
				chop_string(value(p));
			else bad_binary(p,substring_of);
			break;
		case subpath_of: 
			if (cur_type == pair_type) pair_to_path();
			if (nice_pair(p,type(p)) &&  cur_type == path_type)
				chop_path(value(p));
			else bad_binary(p,subpath_of);
			break;
			
		case point_of: case precontrol_of: case postcontrol_of: 
			if (cur_type == pair_type)
				pair_to_path();
			if (cur_type == path_type && type(p) == known)
				find_point(value(p),c);
			else bad_binary(p,c);
			break;
		case pen_offset_of: 
			if (cur_type == future_pen) materialize_pen();
			if (cur_type == pen_type && nice_pair(p,type(p)))
				set_up_offset(value(p));
			else bad_binary(p,pen_offset_of);
			break;
		case direction_time_of:
			if (cur_type == pair_type) pair_to_path();
			if (cur_type == path_type && nice_pair(p,type(p)))
				set_up_direction_time(value(p));
			else bad_binary(p,direction_time_of);
			break;


		case intersect: 
			if (type(p) == pair_type)
			{ 
				q = stash_cur_exp(); unstash_cur_exp(p);
				pair_to_path(); p = stash_cur_exp(); unstash_cur_exp(q);
			}
			if (cur_type == pair_type) pair_to_path();
			if (cur_type == path_type && type(p) == path_type)
			{ 
				path_intersection(value(p),cur_exp);
				pair_value(cur_t,cur_tt);
			}
			else bad_binary(p,intersect);
			break;
		
		#pragma endregion
	} //{there are no other cases}

	recycle_value(p); free_node(p,value_node_size); //{|return| to avoid this}
_exit:
	check_arith; 
	#pragma region <Recycle any sidestepped |independent| capsules>
	if (old_p != null)
	{ 
		recycle_value(old_p); free_node(old_p,value_node_size);
	}
	if (old_exp != null)
	{ 
		recycle_value(old_exp); free_node(old_exp,value_node_size);
	}	
	#pragma endregion
}




// 923
void bad_binary(pointer p, quarterword c)
{
	disp_err(p, /**/289); exp_err(/*Not implemented: */832);
	if (c >= min_of) print_op(c);
	print_known_or_unknown_type(type(p), p);
	if (c >= min_of) print(/*of*/540); else print_op(c);
	print_known_or_unknown_type(cur_type, cur_exp);
	help3(/*I'm afraid I don't know how to apply that operation to that*/833,
		/*combination of types. Continue, and I'll return the second*/848,
		/*argument (see above) as the result of the operation.*/849); put_get_error();
}

// 928
pointer tarnished(pointer p)
{
	pointer q; // beginning of the big node
	pointer r; // current position in the big node

	q = value(p); r = q + big_node_size[type(p)];
	do {
		r = r - 2;
		if (type(r) == independent) {
			return _void;
		}
	} while(!(r == q));
	return null;
}

// 930
void add_or_subtract(pointer p, pointer q, quarterword c)
{
	small_number s, t; // operand types
	pointer r; // list traverser
	int v; // second operand value

	if (q == null) {
		t = cur_type;
		if (t < dependent) v = cur_exp; else v = dep_list(cur_exp);
	}
	else {
		t = type(q);
		if (t < dependent) v = value(q); else v = dep_list(q);
	}
	if (t == known) {
		if (c == minus) negate(v);
		if (type(p) == known) {
			v = slow_add(value(p), v);
			if (q == null) cur_exp = v; else value(q) = v;
			return;
		}
		#pragma region <Add a known value to the constant term of dep_list(p) 931>
		r = dep_list(p);
		while (info(r) != null) r = link(r);
		value(r) = slow_add(value(r), v);
		if (q == null) {
			q = get_node(value_node_size); cur_exp = q; cur_type = type(p); name_type(q) = capsule;
		}
		dep_list(q) = dep_list(p); type(q) = type(p); prev_dep(q) = prev_dep(p); link(prev_dep(p)) = q;
		type(p) = known; // this will keep the recycler from collecting non-garbage
		#pragma endregion

	}
	else {
		if (c == minus) negate_dep_list(v);
		#pragma region <Add operand p to the dependency list v 932>
		if (type(p) == known) 
			#pragma region <Add the known value(p) to the constant term of v 933>
		{
			while (info(v) != null) v = link(v);
			value(v) = slow_add(value(p), value(v));
		}
			#pragma endregion
		else { s = type(p); r = dep_list(p);
		  if (t == dependent)
			{ if (s == dependent)
			  if (max_coef(r) + max_coef(v) < coef_bound)
				{ v = p_plus_q(v,r,dependent); goto done;
				} //fix needed will necessarily be false
			t = proto_dependent; v = p_over_v(v,unity,dependent,proto_dependent);
			}
		if (s == proto_dependent) v = p_plus_q(v,r,proto_dependent);
		else v = p_plus_fq(v,unity,r,proto_dependent,dependent);
	done:
		#pragma region <Output the answer, v (which might have become known) 934>
		if (q != null) dep_finish(v, q, t);
		else {
			cur_type = t; dep_finish(v, null, t);
		}
		#pragma endregion
		}

		#pragma endregion
	}
}



// 935
void dep_finish(pointer v, pointer q, small_number t)
{
	pointer p; // the destination
	scaled vv; // the value, if it is known

	if (q == null) p = cur_exp; else p = q;
	dep_list(p) = v; type(p) = t;
	if (info(v) == null) {
		vv = value(v);
		if (q == null) flush_cur_exp(vv);
		else {
			recycle_value(p); type(q) = known; value(q) = vv;
		}
	}
	else if (q == null) cur_type = t;
	if (fix_needed) fix_dependencies();
}

// 943
void dep_mult(pointer p, int v, bool v_is_scaled)
{
	pointer q; // the dependency list being multiplied by v
	small_number s, t; // its type, before and after

	if (p == null) q = cur_exp;
	else if (type(p) != known) q = p;
	else {
		if (v_is_scaled) value(p) = take_scaled(value(p), v);
		else (value(p) = take_fraction(value(p), v));
		return;
	}
	t = type(q); q = dep_list(q); s = t;
	if (t == dependent)
		if (v_is_scaled)
			if (ab_vs_cd(max_coef(q), myabs(v), coef_bound - 1, unity) >= 0) t = proto_dependent;
	q = p_times_v(q, v, s, t, v_is_scaled); dep_finish(q, p, t);
}

// 944
void frac_mult(scaled n, scaled d) // multiplies cur_exp by n/d
{
	pointer p; // a pair node
	pointer old_exp; // a capsule to recycle
	fraction v; // n/d

	if (internal[tracing_commands] > two)
		#pragma region <Trace the fraction multiplication 945>
	{
		begin_diagnostic(); print_nl(/*{(*/841); print_scaled(n); print_char(/*/*/47); print_scaled(d);
		print(/*)*(*/850); print_exp(null, 0); print(/*)}*/829); end_diagnostic(false);
	}
		#pragma endregion
	switch(cur_type) {
		case transform_type:
		case pair_type:
			old_exp = tarnished(cur_exp);
			break;
		case independent:
			old_exp = _void;
			break;
		default:
			old_exp = null;
			break;
	}
	if (old_exp != null) {
		old_exp = cur_exp; make_exp_copy(old_exp);
	}
	v = make_fraction(n, d);
	if (cur_type == known) cur_exp = take_fraction(cur_exp, v);
	else if (cur_type == pair_type) {
		p = value(cur_exp); dep_mult(x_part_loc(p), v, false); dep_mult(y_part_loc(p), v, false);
	}
	else dep_mult(null, v, false);
	if (old_exp != null) {
		recycle_value(old_exp); free_node(old_exp, value_node_size);
	}
}

// 946
void hard_times(pointer p)
{
	pointer q; //{a copy of the_dependent variable p}
	pointer r; //{the big node for the nice pair}
	scaled u,v; //{the known values of the nice pair}
	if (type(p) == pair_type)
	{ 
		q = stash_cur_exp(); unstash_cur_exp(p); p = q;
	} //{now cur type = pair_type }
	r = value(cur_exp); u = value(x_part_loc(r)); v = value(y_part_loc(r));
	#pragma region <Move the dependent variable p into both_parts of the pair node r 947>
	type(y_part_loc(r)) = type(p); new_dep(y_part_loc(r),copy_dep_list(dep_list(p)));
	type(x_part_loc(r)) = type(p); mem[value_loc(x_part_loc(r))] = mem[value_loc(p)];
	link(prev_dep(p)) = x_part_loc(r); free_node(p,value_node_size);
	#pragma endregion
	dep_mult(x_part_loc(r),u,true); dep_mult(y_part_loc(r),v,true);
}


// 949
void dep_div(pointer p, scaled v)
{
	pointer q; //{the dependency list being divided by v }
	small_number s,t; //{its type, before and after}
	if (p == null) q = cur_exp;
	else if (type(p) != known) q = p;
	else { 
		value(p) = make_scaled(value(p),v); return;
	}
	t = type(q); q = dep_list(q); s = t;
	if (t == dependent)
		if (ab_vs_cd(max_coef(q),unity,coef_bound - 1,myabs(v)) >= 0) t = proto_dependent;
	q = p_over_v(q,v,s,t); dep_finish(q,p,t);
}

// 953
void set_up_trans(quarterword c)
{
	pointer p, q, r; //{list manipulation registers}
	if (c != transformed_by || cur_type != transform_type)
		#pragma region <Put the current transform into |cur_exp|>
	{
		p = stash_cur_exp(); cur_exp = id_transform(); cur_type = transform_type;
		q = value(cur_exp);
		switch(c) {
			#pragma region <For each of the eight cases, change the relevant fields of |cur_exp| and |goto done|; but do nothing if capsule |p| doesnt have the appropriate type>
			case rotated_by:
				if (type(p) == known)
					#pragma region <Install sines and cosines, then |goto done|>
				{ 
					n_sin_cos((value(p) % three_sixty_units) * 16);
					value(xx_part_loc(q)) = round_fraction(n_cos);
					value(yx_part_loc(q)) = round_fraction(n_sin);
					value(xy_part_loc(q)) = -value(yx_part_loc(q));
					value(yy_part_loc(q)) = value(xx_part_loc(q));
					goto done;
				}				
					#pragma endregion
				break;
			case slanted_by:
				if (type(p) > pair_type)
				{ 
					install(xy_part_loc(q),p); goto done;
				}
				break;
			case scaled_by:
				if (type(p) > pair_type)
				{ 
					install(xx_part_loc(q),p); install(yy_part_loc(q),p); goto done;
				}
				break;
			case shifted_by:
				if (type(p) == pair_type)
				{ 
					r = value(p); install(x_part_loc(q),x_part_loc(r));
					install(y_part_loc(q),y_part_loc(r)); goto done;
				}
				break;
			case x_scaled:
				if (type(p) > pair_type)
				{ 
					install(xx_part_loc(q),p); goto done;
				}
				break;
			case y_scaled:
				if (type(p) > pair_type)
				{ 
					install(yy_part_loc(q),p); goto done;
				}
				break;
			case z_scaled:
				if (type(p) == pair_type)
					#pragma region <Install a complex multiplier, then |goto done|>
				{ 
					r = value(p);
					install(xx_part_loc(q),x_part_loc(r));
					install(yy_part_loc(q),x_part_loc(r));
					install(yx_part_loc(q),y_part_loc(r));
					if (type(y_part_loc(r)) == known) negate(value(y_part_loc(r)));
					else negate_dep_list(dep_list(y_part_loc(r)));
					install(xy_part_loc(q),y_part_loc(r));
					goto done;
				}
					#pragma endregion
				break;
			case transformed_by:
				//do_nothing;
				break;
			#pragma endregion
		} //{there are no other cases}
		disp_err(p,/*Improper transformation argument*/851);
		help3(/*The expression shown above has the wrong type,*/852,
			/*so I can't transform anything using it.*/853,
			/*Proceed, and I'll omit the transformation.*/467);
		put_get_error();
	done: 
		recycle_value(p); free_node(p,value_node_size);
	}
		#pragma endregion
	
	#pragma region <If the current transform is entirely known, stash it in global variables; otherwise |return|>
	q = value(cur_exp); r = q + transform_node_size;
	do { 
		r = r - 2;
		if (type(r) != known) return;
	} while (!(r == q));
	txx = value(xx_part_loc(q));
	txy = value(xy_part_loc(q));
	tyx = value(yx_part_loc(q));
	tyy = value(yy_part_loc(q));
	tx = value(x_part_loc(q));
	ty = value(y_part_loc(q));
	flush_cur_exp(0);	
	#pragma endregion
}



// 960
void set_up_known_trans(quarterword c)
{ 
	set_up_trans(c);
	if (cur_type != known)
	{ 
		exp_err(/*Transform components aren't all known*/854);
		help3(/*I'm unable to apply a partially specified transformation*/855,
			/*except to a fully known pair or transform.*/856,
			/*Proceed, and I'll omit the transformation.*/467); put_get_flush_error(0); txx = unity; txy = 0;
		tyx = 0; tyy = unity; tx = 0; ty = 0;
	}
}


// 961
void trans(pointer p, pointer q)
{
	scaled v; //{the new x value}
	v = take_scaled(mem[p].sc,txx) + take_scaled(mem[q].sc,txy) + tx;
	mem[q].sc = take_scaled(mem[p].sc,tyx) + take_scaled(mem[q].sc,tyy) + ty; mem[p].sc = v;
}

// 962
void path_trans(pointer p, quarterword c)
{
	pointer q; //{list traverser}
	set_up_known_trans(c); unstash_cur_exp(p);
	if (cur_type == pen_type)
	{ 
		if (max_offset(cur_exp) == 0)
			if (tx == 0)
				if (ty == 0) return;
		flush_cur_exp(make_path(cur_exp)); cur_type = future_pen;
	}
	q = cur_exp;
	do { 
		if (left_type(q) != endpoint) trans(q + 3,q + 4); //{that�s left x and left y }
		trans(q + 1,q + 2); //{that�s x coord and y coord }
		if (right_type(q) != endpoint) trans(q + 5,q + 6); //{that�s right x and right y }
		q = link(q);
	} while (!( q == cur_exp));
}

// 963
void edges_trans(pointer p, quarterword c)
{
	set_up_known_trans(c); unstash_cur_exp(p); cur_edges = cur_exp;
	if (empty_edges(cur_edges)) return; //{the empty set is easy to transform}
	if (txx == 0)
		if (tyy == 0)
			if (txy % unity == 0)
				if (tyx % unity == 0) 
				{
					xy_swap_edges(); txx = txy; tyy = tyx; txy = 0; tyx = 0;
					if (empty_edges(cur_edges))  return;
				}
	if (txy == 0)
		if (tyx == 0)
			if (txx % unity == 0)
				if (tyy % unity == 0)
					#pragma region <Scale the_edges, shift them, and return 964>
				{ 
					if (txx == 0 || tyy == 0)
					{ 
						toss_edges(cur_edges); cur_exp = get_node(edge_header_size); init_edges(cur_exp);
					}
					else { 
					if (txx < 0)
					{ 
						x_reflect_edges(); txx = -txx;
					}
					if (tyy < 0)
					{ 
						y_reflect_edges(); tyy = -tyy;
					}
					if (txx != unity) x_scale_edges(txx / unity);
					if (tyy != unity) y_scale_edges(tyy / unity);
					#pragma region <Shift the_edges by (tx,ty), rounded 965>
					
					tx = round_unscaled(tx); ty = round_unscaled(ty);
					if (m_min(cur_edges) + tx <= 0 || m_max(cur_edges) + tx >= 8192 ||
						n_min(cur_edges) + ty <= 0 || n_max(cur_edges) + ty >= 8191 ||
						myabs(tx) >= 4096 || myabs(ty) >= 4096)
					{ 
						print_err(/*Too far to shift*/857);
						help3(/*I can't shift the picture as requested---it would*/858,
						/*make some coordinates too large or too small.*/466,
						/*Proceed, and I'll omit the transformation.*/467); put_get_error();
					}
					else { 
						if (tx != 0)
						{ 
							if (!valid_range(m_offset(cur_edges) - tx)) fix_offset();
							m_min(cur_edges) = m_min(cur_edges) + tx; m_max(cur_edges) = m_max(cur_edges) + tx;
							m_offset(cur_edges) = m_offset(cur_edges) - tx; last_window_time(cur_edges) = 0;
						}
						if (ty != 0)
						{ 
							n_min(cur_edges) = n_min(cur_edges) + ty; n_max(cur_edges) = n_max(cur_edges) + ty;
							n_pos(cur_edges) = n_pos(cur_edges) + ty; last_window_time(cur_edges) = 0;
						}
					}		
					
					#pragma endregion
					}
					return;
				}
					#pragma endregion
	print_err(/*That transformation is too hard*/859);
	help3(/*I can apply complicated transformations to paths,*/860,
	/*but I can only do integer operations on pictures.*/861,
	/*Proceed, and I'll omit the transformation.*/467); put_get_error();
}



// 966
void big_trans(pointer p, quarterword c)
{
	pointer q, r, pp, qq; //{list manipulation registers}
	small_number s; //{size of a big node}
	s = big_node_size[type(p)]; q = value(p); r = q + s;
	do {
		r = r - 2;
		if (type(r) != known) 
			#pragma region <Transform an unknown big node and |return|>
		{ 
			set_up_known_trans(c); make_exp_copy(p); r = value(cur_exp);
			if (cur_type == transform_type)
			{ 
				bilin1(yy_part_loc(r),tyy,xy_part_loc(q),tyx,0);
				bilin1(yx_part_loc(r),tyy,xx_part_loc(q),tyx,0);
				bilin1(xy_part_loc(r),txx,yy_part_loc(q),txy,0);
				bilin1(xx_part_loc(r),txx,yx_part_loc(q),txy,0);
			}
			bilin1(y_part_loc(r),tyy,x_part_loc(q),tyx,ty);
			bilin1(x_part_loc(r),txx,y_part_loc(q),txy,tx);
			return;
		}
			#pragma endregion
	} while (!(r == q));
	
	#pragma region <Transform a known big node>
	set_up_trans(c);
	if (cur_type == known) 
		#pragma region <Transform known by known>
	{ 
		make_exp_copy(p); r = value(cur_exp);
		if (cur_type == transform_type)
		{ 
			bilin3(yy_part_loc(r),tyy,value(xy_part_loc(q)),tyx,0);
			bilin3(yx_part_loc(r),tyy,value(xx_part_loc(q)),tyx,0);
			bilin3(xy_part_loc(r),txx,value(yy_part_loc(q)),txy,0);
			bilin3(xx_part_loc(r),txx,value(yx_part_loc(q)),txy,0);
		}
		bilin3(y_part_loc(r),tyy,value(x_part_loc(q)),tyx,ty);
		bilin3(x_part_loc(r),txx,value(y_part_loc(q)),txy,tx);
	}
		#pragma endregion
	else { 
		pp = stash_cur_exp(); qq = value(pp);
		make_exp_copy(p); r = value(cur_exp);
		if (cur_type == transform_type)
		{ 
			bilin2(yy_part_loc(r),yy_part_loc(qq),
			value(xy_part_loc(q)),yx_part_loc(qq),null);
			bilin2(yx_part_loc(r),yy_part_loc(qq),
			value(xx_part_loc(q)),yx_part_loc(qq),null);
			bilin2(xy_part_loc(r),xx_part_loc(qq),
			value(yy_part_loc(q)),xy_part_loc(qq),null);
			bilin2(xx_part_loc(r),xx_part_loc(qq),
			value(yx_part_loc(q)),xy_part_loc(qq),null);
		}
		bilin2(y_part_loc(r),yy_part_loc(qq),
		value(x_part_loc(q)),yx_part_loc(qq),y_part_loc(qq));
		bilin2(x_part_loc(r),xx_part_loc(qq),
		value(y_part_loc(q)),xy_part_loc(qq),x_part_loc(qq));
		recycle_value(pp); free_node(pp,value_node_size);
	}
	#pragma endregion
} //{node |p| will now be recycled by |do_binary|}



// 968
void bilin1(pointer p, scaled t, pointer q, scaled u, scaled delta)
{
	pointer r; //{list traverser}
	if (t != unity) dep_mult(p,t,true);
	if (u != 0)
		if (type(q) == known) delta = delta + take_scaled(value(q),u);
	else { 
		#pragma region <Ensure that |type(p)=proto_dependent|>
		if (type(p) != proto_dependent)
		{ 
			if (type(p) == known) new_dep(p,const_dependency(value(p)));
			else dep_list(p) = p_times_v(dep_list(p),unity,dependent,proto_dependent,true);
			type(p) = proto_dependent;
		}
			
		#pragma endregion
		dep_list(p) = p_plus_fq(dep_list(p),u,dep_list(q),proto_dependent,type(q));
	}
	if (type(p) == known) value(p) = value(p) + delta;
	else { 
		r = dep_list(p);
		while (info(r) != null) r = link(r);
		delta = value(r) + delta;
		if (r != dep_list(p)) value(r) = delta;
		else { 
			recycle_value(p); type(p) = known; value(p) = delta;
		}
	}
	if (fix_needed) fix_dependencies();
}


// 971
void add_mult_dep(pointer p, scaled v, pointer r)
{
	if (type(r) == known) value(dep_final) = value(dep_final) + take_scaled(value(r),v);
	else { 
		dep_list(p) = p_plus_fq(dep_list(p),v,dep_list(r),proto_dependent,type(r));
		if (fix_needed) fix_dependencies();
	}
}

// 972
void bilin2(pointer p,pointer t,scaled v,pointer u,pointer q)
{
	scaled vv; //{temporary storage for |value(p)|}
	vv = value(p); type(p) = proto_dependent;
	new_dep(p,const_dependency(0)); //{this sets |dep_final|}
	if (vv != 0) add_mult_dep(p,vv,t); //{|dep_final| doesn't change}
	if (v != 0) add_mult_dep(p,v,u);
	if (q != null) add_mult_dep(p,unity,q);
	if (dep_list(p) == dep_final)
	{
		vv = value(dep_final); recycle_value(p);
		type(p) = known; value(p) = vv;
	}
}

// 974
void bilin3(pointer p,scaled t,scaled v,scaled u,scaled delta)
{
	if (t != unity) delta = delta + take_scaled(value(p),t);
	else delta = delta + value(p);
	if (u != 0) value(p) = delta + take_scaled(v,u);
	else value(p) = delta;
}


// 976
void cat(pointer p)
{
	str_number a, b; // the strings being concatenated
	pool_pointer k; // index into str_pool
	a = value(p); b = cur_exp; str_room(length(a) + length(b));
	for (k = str_start[a]; k <= str_start[a + 1] - 1; k++) append_char(str_pool[k]);
	for (k = str_start[b]; k <= str_start[b + 1] - 1; k++) append_char(str_pool[k]);
	cur_exp = make_string(); delete_str_ref(b);
}

// 977
void chop_string(pointer p)
{
	int a,b; //{start and stop points}
	int l; //{length of the original string}
	int k; //{runs from a to b}
	str_number s; //{the original string}
	bool reversed; //{was a > b?}

	a = round_unscaled(value(x_part_loc(p))); b = round_unscaled(value(y_part_loc(p)));
	if (a <= b) reversed = false;
	else { 
		reversed = true; k = a; a = b; b = k;
	}
	s = cur_exp; l = length(s);
	if (a < 0)
	{ 
	a = 0;
	if (b < 0) b = 0;
	}
	if (b > l)
	{ b = l;
	if (a > l) a = l;
	}
	str_room(b - a);
	if (reversed)
		for (k = str_start[s] + b - 1; k >= str_start[s] + a; k--) append_char(str_pool[k]);
	else for (k = str_start[s] + a; k <= str_start[s] + b - 1; k++) append_char(str_pool[k]);
	cur_exp = make_string(); delete_str_ref (s);
}

// 978
void chop_path(pointer p)
{
	pointer q; //{a knot in the original path}
	pointer  pp, qq, rr, ss; //{link variables for copies of path nodes}
	scaled  a, b, k, l; //{indices for chopping}
	bool reversed; //{was |a>b|?}
	
	l = path_length(); a = value(x_part_loc(p)); b = value(y_part_loc(p));
	if (a <= b) reversed = false;
	else { 
		reversed = true; k = a; a = b; b = k;
	}
	#pragma region <Dispense with the cases |a<0| and/or |b>l|>
	if (a < 0)
		if (left_type(cur_exp) == endpoint)
		{ 
			a = 0; 
			if (b < 0) b = 0;
		}
	else 
		do {
		a = a + l; b = b + l;
	} while (!(a >= 0)); //{a cycle always has length |l>0|}
	if (b > l) 
		if (left_type(cur_exp) == endpoint)
		{ 
			b = l; 
			if (a > l) a = l;
		}
	else 
		while (a >= l)
		{ 
			a = a - l; b = b - l;
		}

	#pragma endregion
	q = cur_exp;
	while (a >= unity)
	{ 
		q = link(q); a = a - unity; b = b - unity;
	}
	if (b == a) 
		#pragma region <Construct a path from |pp| to |qq| of length zero>
	{ 
		if (a > 0)
		{ 
			qq = link(q);
			split_cubic(q,a * 010000,x_coord(qq),y_coord(qq)); q = link(q);
		}
		pp = copy_knot(q); qq = pp;
	}
		#pragma endregion
	else 
		#pragma region <Construct a path from |pp| to |qq| of length $lceil brceil$>
	{ 
		pp = copy_knot(q); qq = pp;
		do {
			q = link(q); rr = qq; qq = copy_knot(q); link(rr) = qq; b = b - unity;
		} while (!(b <= 0));
		if (a > 0)
		{ 
			ss = pp; pp = link(pp);
			split_cubic(ss,a * 010000,x_coord(pp),y_coord(pp)); pp = link(ss);
			free_node(ss,knot_node_size);
			if (rr == ss)
			{ 
				b = make_scaled(b,unity - a); rr = pp;
			}
		}
		if (b < 0)
		{ 
			split_cubic(rr,(b + unity) * 010000,x_coord(qq),y_coord(qq));
			free_node(qq,knot_node_size);
			qq = link(rr);
		}
	}
		#pragma endregion
	left_type(pp) = endpoint; right_type(qq) = endpoint; link(qq) = pp;
	toss_knot_list(cur_exp);
	if (reversed)
	{ 
		cur_exp = link(htap_ypoc(pp)); toss_knot_list(pp);
	}
	else cur_exp = pp;
}


// 982
void pair_value(scaled x, scaled y)
{
	pointer p; //{a pair node}
	p = get_node(value_node_size); flush_cur_exp(p); cur_type = pair_type; type(p) = pair_type;
	name_type(p) = capsule; init_big_node(p); p = value(p);
	type(x_part_loc(p)) = known; value(x_part_loc(p)) = x;
	type(y_part_loc(p)) = known; value(y_part_loc(p)) = y;
}


// 984
void set_up_offset(pointer p)
{
	find_offset(value(x_part_loc(p)), value(y_part_loc(p)), cur_exp); pair_value(cur_x, cur_y);
}

void set_up_direction_time(pointer p)
{
	flush_cur_exp(find_direction_time(value(x_part_loc(p)), value(y_part_loc(p)), cur_exp));
}

// 985
void find_point(scaled v, quarterword c)
{
	pointer p; //{the path}
	scaled n; //{its length}
	pointer q; //{successor of |p|}
	
	p = cur_exp;
	if (left_type(p) == endpoint) n = -unity; else n = 0;
	do {
		p = link(p); n = n + unity;
	} while (!(p == cur_exp));
	if (n == 0) v = 0;
	else if (v < 0)
		if (left_type(p) == endpoint) v = 0;
		else v = n - 1 - ((-v - 1) % n);
	else if (v > n)
		if (left_type(p) == endpoint) v = n;
		else v = v % n;
	p = cur_exp;
	while (v >= unity)
	{ 
		p = link(p); v = v - unity;
	}
	if (v != 0) 
		#pragma region <Insert a fractional node by splitting the cubic>
	{ 
		q = link(p); split_cubic(p,v * 010000,x_coord(q),y_coord(q)); p = link(p);
	}	
		#pragma endregion
		
	#pragma region <Set the current expression to the desired path coordinates>
	switch(c) {
		case point_of: 
			pair_value(x_coord(p),y_coord(p));
			break;
		case precontrol_of: 
			if (left_type(p) == endpoint) pair_value(x_coord(p),y_coord(p));
			else pair_value(left_x(p),left_y(p));
			break;
		case postcontrol_of:
			if (right_type(p) == endpoint) pair_value(x_coord(p),y_coord(p));
			else pair_value(right_x(p),right_y(p));
			break;
	} //{there are no other cases}
	#pragma endregion
}



// 989
void do_statement() //{governs \MF's activities}
{
	cur_type = vacuous; get_x_next();
	if (cur_cmd > max_primary_command)
		#pragma region <Worry about bad statement>
		{
			if (cur_cmd < semicolon)
			{
				print_err(/*A statement can't begin with `*/862);
				print_cmd_mod(cur_cmd,cur_mod); print_char(/*'*/39);
				help5(/*I was looking for the beginning of a new statement.*/863,
				/*If you just proceed without changing anything, I'll ignore*/864,
				/*everything up to the next `;'. Please insert a semicolon*/865,
				/*now in front of anything that you don't want me to delete.*/866,
				/*(See Chapter 27 of The METAFONTbook for an example.)*/867);
				back_error(); get_x_next();
			}
		}	
		#pragma endregion
	else if (cur_cmd > max_statement_command)
		#pragma region <Do an equation, assignment, title, or $langle$expression$rangle${endgroup}>
	{ 
		var_flag = assignment; scan_expression();
		if (cur_cmd < end_group)
		{ 
			if (cur_cmd == equals) do_equation();
			else if (cur_cmd == assignment) do_assignment();
			else if (cur_type == string_type) 
				#pragma region <Do a title>
			{
				if (internal[tracing_titles] > 0)
				{ 
					print_nl(/**/289); slow_print(cur_exp); update_terminal();
				}
				if (internal[proofing] > 0)
					#pragma region <Send the current expression as a title to the output file>
				{
					check_gf; gf_string(/*title */868,cur_exp);
				}				
					#pragma endregion
			}			
				#pragma endregion
			else if (cur_type != vacuous)
			{ 
				exp_err(/*Isolated expression*/869);
				help3(/*I couldn't find an `=' or `:=' after the*/870,
				/*expression that is shown above this error message,*/871,
				/*so I guess I'll just ignore it and carry on.*/872);
				put_get_error();
			}
			flush_cur_exp(0); cur_type = vacuous;
		}
	}	
		#pragma endregion
	else 
		#pragma region <Do a statement that doesnt begin with an expression>
		{ 
			if (internal[tracing_commands] > 0) show_cur_cmd_mod;
			switch(cur_cmd) {
				case type_name:
					do_type_declaration();
					break;
				case macro_def:
					if (cur_mod > var_def) make_op_def();
					else if (cur_mod > end_def) scan_def();
					break;
					
				#pragma region <Cases of |do_statement| that invoke particular commands>
			case random_seed:
				do_random_seed();
				break;

			// 1023
			case mode_command:
				print_ln(); interaction = cur_mod;
				#pragma region <Initialize the print selector based on interaction 70>
				if (interaction == batch_mode) selector = no_print; else selector = term_only;
				#pragma endregion
				if (log_opened) selector = selector + 2;
				get_x_next();
				break;

			// 1026
			case protection_command:
				do_protection();
				break;

			// 1030
			case delimiters:
				def_delims();
				break;

			// 1033
			case save_command:
				do {
					get_symbol(); save_variable(cur_sym); get_x_next();
				} while (!(cur_cmd != comma));
				break;

			case interim_command:
				do_interim();
				break;
			case let_command:
				do_let();
				break;
			case new_internal:
				do_new_internal();
				break;

			// 1039
			case show_command:
				do_show_whatever();
				break;

			case add_to_command:
				do_add_to();
				break;

			// 1069
			case ship_out_command:
				do_ship_out();
				break;
			case display_command:
				do_display();
				break;
			case open_window:
				do_open_window();
				break;
			case cull_command:
				do_cull();
				break;

			// 1076
			case every_job_command:
				get_symbol();
				start_sym = cur_sym; get_x_next();
				break;

			// 1081
			case message_command:
				do_message();
				break;

			// 1100
			case tfm_command:
				do_tfm_command();
				break;

			// 1175
			case special_command:
				do_special();
				break;				
				#pragma endregion
				
			} //{there are no other cases}
			cur_type = vacuous;
		}	
		#pragma endregion
	if (cur_cmd < semicolon)
		#pragma region <Flush unparsable junk that was found after the statement>
	{ 
		print_err(/*Extra tokens will be flushed*/873);
		help6(/*I've just read as much of that statement as I could fathom,*/874,
			/*so a semicolon should have been next. It's very puzzling...*/875,
			/*but I'll try to get myself back together, by ignoring*/876,
			/*everything up to the next `;'. Please insert a semicolon*/865,
			/*now in front of anything that you don't want me to delete.*/866,
			/*(See Chapter 27 of The METAFONTbook for an example.)*/867);
		back_error(); scanner_status = flushing;
		do { 
			get_next();
			#pragma region <Decrease the string reference count...>
			if (cur_cmd == string_token) delete_str_ref(cur_mod);
			#pragma endregion
		} while (!( end_of_statement)); //{|cur_cmd=semicolon|, |end_group|, or |stop|}
		scanner_status = normal;
	}	
		#pragma endregion
	error_count = 0;
}

// 995
void do_equation()
{
	pointer lhs; //{capsule for the left-hand side}
	pointer p; //{temporary register}

	lhs = stash_cur_exp(); get_x_next(); var_flag = assignment; scan_expression();
	if (cur_cmd == equals) do_equation();
	else if (cur_cmd == assignment) do_assignment();
	if (internal[tracing_commands] > two)
		#pragma region <Trace the current equation>
	{ 
		begin_diagnostic(); print_nl(/*{(*/841); print_exp(lhs,0);
		print(/*)=(*/877); print_exp(null,0); print(/*)}*/829); end_diagnostic(false);
	}
		#pragma endregion
	if (cur_type == unknown_path)
		if (type(lhs) == pair_type)
		{  
			p = stash_cur_exp(); unstash_cur_exp(lhs); lhs = p;
		} //{in this case |make_eq| will change the pair to a path}
	make_eq(lhs); //{equate |lhs| to |(cur_type,cur_exp)|}
}

// 996
void do_assignment()
{
	pointer lhs; //{token list for the left-hand side}
	pointer p; //{where the left-hand value is stored}
	pointer q; //{temporary capsule for the right-hand value}

	if (cur_type != token_list)
	{
		exp_err(/*Improper `:=' will be changed to `='*/878);
		help2(/*I didn't find a variable name at the left of the `:=',*/879,
		/*so I'm going to pretend that you said `=' instead.*/880);
		error(); do_equation();
	}
	else {
		lhs = cur_exp; cur_type = vacuous;
		get_x_next(); var_flag = assignment; scan_expression();
		if (cur_cmd == equals) do_equation();
		else if (cur_cmd == assignment) do_assignment();
		if (internal[tracing_commands] > two)
			#pragma region <Trace the current assignment>
		{
			begin_diagnostic(); print_nl(/*{*/123);
			if (info(lhs) > hash_end) slow_print(int_name[info(lhs)-(hash_end)]);
			else show_token_list(lhs,null,1000,0);
			print(/*:=*/521); print_exp(null,0); print_char(/*}*/125); end_diagnostic(false);
		}		
			#pragma endregion
		if (info(lhs) > hash_end)
			#pragma region <Assign the current expression to an internal variable>
			if (cur_type == known) internal[info(lhs)-(hash_end)] = cur_exp;
			else {
				exp_err(/*Internal quantity `*/881);
				slow_print(int_name[info(lhs)-(hash_end)]);
				print(/*' must receive a known value*/882);
				help2(/*I can't set an internal quantity to anything but a known*/883,
				/*numeric value, so I'll have to ignore this assignment.*/884);
				put_get_error();
			}		
			#pragma endregion
		else
			#pragma region <Assign the current expression to the variable |lhs|>
		{
			p = find_variable(lhs);
			if (p != null)
			{
				q = stash_cur_exp(); cur_type = und_type(p); recycle_value(p);
				type(p) = cur_type; value(p) = null; make_exp_copy(p);
				p = stash_cur_exp(); unstash_cur_exp(q); make_eq(p);
			}
			else {
				obliterated(lhs); put_get_error();
			}
		}
			#pragma endregion
		flush_node_list(lhs);
	}
}

// 1001
void make_eq(pointer lhs)
{
	small_number t; //{type of the left-hand side}
	int v = INT_MAX; //{value of the left-hand side}
	pointer p,q; //{pointers inside of big nodes}
restart: 
	t = type(lhs);
	if (t <= pair_type) v = value(lhs);
	switch(t) {
		#pragma region <For each type |t|, make an equation and |goto done| unless |cur_type| is incompatible with~|t|>
		case boolean_type: case string_type: case pen_type: case path_type: case picture_type:
			if (cur_type == t + unknown_tag)
			{ 
				nonlinear_eq(v,cur_exp,false); goto done;
			}
			else if (cur_type == t)
			#pragma region <Report redundant or inconsistent equation and |goto done|>
			{
				if (cur_type <= string_type) {
					if (cur_type == string_type) {
						if (str_vs_str(v, cur_exp) != 0) goto not_found;
					}
					else if (v != cur_exp) goto not_found;
					#pragma region <Exclaim about a redundant equation 623>
					print_err(/*Redundant equation*/517);
					help2(/*I already knew that this equation was true.*/518,
						/*But perhaps no harm has been done; let's continue.*/519);
					put_get_error();
					#pragma endregion
					goto done;
				}
				print_err(/*Redundant or inconsistent equation*/885);
				help2(/*An equation between already-known quantities can't help.*/886,
					/*But don't worry; continue and I'll just ignore it.*/887); put_get_error(); goto done;
			not_found:
				print_err(/*Inconsistent equation*/888);
				help2(/*The equation I just read contradicts what was said before.*/889,
					/*But don't worry; continue and I'll just ignore it.*/887); put_get_error(); goto done;
			}		
			#pragma endregion
			break;
		case unknown_types:
			if (cur_type == t - unknown_tag)
			{ 
				nonlinear_eq(cur_exp,lhs,true); goto done;
			}
			else if (cur_type == t)
			{ 
				ring_merge(lhs,cur_exp); goto done;
			}
			else if (cur_type == pair_type)
				if (t == unknown_path)
				{ 
					pair_to_path(); goto restart;
				}
			break;
		case transform_type: case pair_type:
			if (cur_type == t)
				#pragma region <Do multiple equations and |goto done|>
			{ 
				p = v + big_node_size[t]; q = value(cur_exp) + big_node_size[t];
				do { 
					p = p - 2; q = q - 2; try_eq(p,q);
				} while (!(p == v));
				goto done;
			}			
				#pragma endregion
			break;
		case known: case dependent: case proto_dependent: case independent:
			if (cur_type >= known)
			{ 
				try_eq(lhs,null); goto done;
			}
			break;
		case vacuous: //do_nothing;
			break;
		#pragma endregion
	} // {all cases have been listed}
	
	#pragma region <Announce that the equation cannot be performed>
	disp_err(lhs,/**/289); exp_err(/*Equation cannot be performed (*/890);
	if (type(lhs) <= pair_type) print_type(type(lhs)); else print(/*numeric*/348);
	print_char(/*=*/61);
	if (cur_type <= pair_type) print_type(cur_type); else print(/*numeric*/348);
	print_char(/*)*/41);
	help2(/*I'm sorry, but I don't know how to make such things equal.*/891,
		/*(See the two expressions just above the error message.)*/892);
	put_get_error();
	#pragma endregion
done: check_arith; recycle_value(lhs); free_node(lhs,value_node_size);
}

// 1006
void try_eq(pointer l, pointer r)
{
	pointer p; //{dependency list for right operand minus left operand}
	int t; //known..independent; //{the type of list |p|}
	pointer q; //{the constant term of |p| is here}
	pointer pp; //{dependency list for right operand}
	int tt; //:dependent..independent; //{the type of list |pp|}
	bool copied; //{have we copied a list that ought to be recycled?}

	#pragma region <Remove the left operand from its container, negate it, and put it into dependency list~|p| with constant term~|q|>
	t = type(l);
	if (t == known)
	{ 
		t = dependent; p = const_dependency(-value(l)); q = p;
	}
	else if (t == independent)
	{ 
		t = dependent; p = single_dependency(l); negate(value(p));
		q = dep_final;
	}
	else  { 
		p = dep_list(l); q = p;
		while (true)  { 
			negate(value(q));
			if (info(q) == null) goto done;
			q = link(q);
		}
	done:  link(prev_dep(l)) = link(q); prev_dep(link(q)) = prev_dep(l);
		type(l) = known;
	}
	#pragma endregion

	#pragma region <Add the right operand to list |p|>
	if (r == null)
		if (cur_type == known)
		{
			value(q) = value(q)+cur_exp; goto done1;
		}
		else  { 
			tt = cur_type;
			if (tt == independent) pp = single_dependency(cur_exp);
			else pp = dep_list(cur_exp);
		}
	else if (type(r) == known)
	{ 
		value(q) = value(q)+value(r); goto done1;
	}
	else  { 
		tt = type(r);
		if (tt == independent) pp = single_dependency(r);
		else pp = dep_list(r);
	}
	if (tt != independent) copied = false;
	else {
		copied = true; tt = dependent;
	}
	#pragma region <Add dependency list |pp| of type |tt| to dependency list~|p| of type~|t|>
	watch_coefs = false;
	if (t == tt) p = p_plus_q(p,pp,t);
	else if (t == proto_dependent)
		p = p_plus_fq(p,unity,pp,proto_dependent,dependent);
	else {
		q = p;
		while (info(q) != null)
		{ 
			value(q) = round_fraction(value(q)); q = link(q);
		}
		t = proto_dependent; p = p_plus_q(p,pp,t);
	}
	watch_coefs = true;
	#pragma endregion
	if (copied) flush_node_list(pp);
	done1:
	#pragma endregion
	if (info(p) == null)
	#pragma region <Deal with redundant or inconsistent equation>
	{ 
		if (myabs(value(p)) > 64) //{off by .001 or more}
		{ 
			print_err(/*Inconsistent equation*/888);
			print(/* (off by */893); print_scaled(value(p)); print_char(/*)*/41);
			help2(/*The equation I just read contradicts what was said before.*/889,
				/*But don't worry; continue and I'll just ignore it.*/887);
			put_get_error();
		}
		else if (r == null)
		#pragma region <Exclaim about a redundant equation>
		{
			print_err(/*Redundant equation*/517);
			help2(/*I already knew that this equation was true.*/518,
			/*But perhaps no harm has been done; let's continue.*/519);
			put_get_error();
		}
		#pragma endregion
		free_node(p,dep_node_size);
	}
	#pragma endregion
	else  {
		linear_eq(p,t);
		if (r == null)
			if (cur_type != known)
				if (type(cur_exp) == known)
				{ 
					pp = cur_exp; cur_exp = value(cur_exp); cur_type = known;
					free_node(pp,value_node_size);
				}
	}
}


// 1011
pointer scan_declared_variable()
{
	pointer x; //{hash address of the variable's root}
	pointer h, t; //{head and tail of the token list to be returned}
	pointer l; //{hash address of left bracket}
	get_symbol(); x = cur_sym;
	if (cur_cmd != tag_token) clear_symbol(x,false);
	h = get_avail(); info(h) = x; t = h;
	while (true) {
		get_x_next();
		if (cur_sym == 0) goto done;
		if (cur_cmd != tag_token)
			if (cur_cmd != internal_quantity)
				if (cur_cmd == left_bracket) 
					#pragma region <Descend past a collective subscript>
				{
					l = cur_sym; get_x_next();
					if (cur_cmd != right_bracket)
					{
						back_input(); cur_sym = l; cur_cmd = left_bracket; goto done;
					}
					else cur_sym = collective_subscript;
				}	
					#pragma endregion
				else goto done;
		link(t) = get_avail(); t = link(t); info(t) = cur_sym;
	}
done: 
	if (eq_type(x) % outer_tag != tag_token) clear_symbol(x,false);
	if (equiv(x) == null) new_root(x);
	return h;
}



// 1015
void do_type_declaration()
{
	small_number t; //{the type being declared}
	pointer p; //{token list for a declared variable}
	pointer q; //{value node for the variable}

	if (cur_mod >= transform_type) t = cur_mod; else t = cur_mod + unknown_tag;
	do {
		p = scan_declared_variable();
		flush_variable(equiv(info(p)),link(p),false);
		q = find_variable(p);
		if (q != null)
		{
			type(q) = t; value(q) = null;
		}
		else {
			print_err(/*Declared variable conflicts with previous vardef*/894);
			help2(/*You can't use, e.g., `numeric foo[]' after `vardef foo'.*/895,
				/*Proceed, and I'll ignore the illegal redeclaration.*/896);
			put_get_error();
		}
		flush_list(p);
		if (cur_cmd < comma)
		#pragma region <Flush spurious symbols after the declared variable>
		{
			print_err(/*Illegal suffix of declared variable will be flushed*/897);
			help5(/*Variables in declarations must consist entirely of*/898,
				/*names and collective subscripts, e.g., `x[]a'.*/899,
				/*Are you trying to use a reserved word in a variable name?*/900,
				/*I'm going to discard the junk I found here,*/901,
				/*up to the next comma or the end of the declaration.*/902);
			if (cur_cmd == numeric_token)
				help_line[2] = /*Explicit subscripts like `x15a' aren't permitted.*/903;
			put_get_error(); scanner_status = flushing;
			do {
				get_next();
				#pragma region <Decrease the string reference count...>
				if (cur_cmd == string_token) delete_str_ref(cur_mod);
				#pragma endregion
			} while (!(cur_cmd >= comma)); //{either |end_of_statement| or |cur_cmd=comma|}
			scanner_status = normal;
		}
		#pragma endregion
	} while (!(end_of_statement));
}



// 1017
void main_control()
{
	do {
		do_statement();
		if (cur_cmd == end_group) {
			print_err(/*Extra `endgroup'*/904);
			help2(/*I'm not currently working on a `begingroup',*/905,
			      /*so I had better not try to end anything.*/691); flush_error(0);
		}
	} while (!(cur_cmd == stop));
}

// 1021
void do_random_seed()
{
	get_x_next();
	if (cur_cmd != assignment) {
		missing_err(/*:=*/521);
		help1(/*Always say `randomseed:=<numeric expression>'.*/906);
		back_error();
	}
	get_x_next(); scan_expression();
	if (cur_type != known) {
		exp_err(/*Unknown value will be ignored*/907);
		help2(/*Your expression was too random for me to handle,*/908,
			/*so I won't change the random seed just now.*/909);
		put_get_flush_error(0);
	}
	else
		#pragma region <Initialize the random seed to cur_exp 1022>
	{
		init_randoms(cur_exp);
		if (selector >= log_only) {
			old_setting = selector; selector = log_only; print_nl(/*{randomseed:=*/910);
			print_scaled(cur_exp); print_char(/*}*/125); print_nl(/**/289); selector = old_setting;
		}
	}
		#pragma endregion
}

// 1032
void check_delimiter(pointer l_delim, pointer r_delim)
{
	if (cur_cmd == right_delimiter)
		if (cur_mod == l_delim) return;
	if (cur_sym != r_delim) {
		missing_err(text(r_delim));
		help2(/*I found no right delimiter to match a left one. So I've*/911,
			/*put one in, behind the scenes; this may fix the problem.*/912); back_error();
	}
	else {
		print_err(/*The token `*/913); slow_print(text(r_delim));
		print(/*' is no longer a right delimiter*/914);
		help3(/*Strange: This token has lost its former meaning!*/915,
			/*I'll read it as a right delimiter this time;*/916,
			/*but watch out, I'll probably miss it later.*/917); error();
	}
}


// 1029
void do_protection()
{
	int m; // 0..1, 0 to unprotect, 1 to protect
	halfword t; // the eq_type before we change it

	m = cur_mod;
	do {
		get_symbol(); t = eq_type(cur_sym);
		if (m == 0) {
			if (t >= outer_tag) eq_type(cur_sym) = t - outer_tag;
		}
		else if (t < outer_tag) eq_type(cur_sym) = t + outer_tag;
		get_x_next();
	} while(!(cur_cmd != comma));
}

// 1031
void def_delims()
{
	pointer l_delim, r_delim; // the new delimiter pair
	get_clear_symbol(); l_delim = cur_sym;
	get_clear_symbol(); r_delim = cur_sym;
	eq_type(l_delim) = left_delimiter; equiv(l_delim) = r_delim;
	eq_type(r_delim) = right_delimiter; equiv(r_delim) = l_delim;
	get_x_next();
}

// 1034
void do_interim()
{
	get_x_next();
	if (cur_cmd != internal_quantity) {
		print_err(/*The token `*/913);
		if (cur_sym == 0) print(/*(%CAPSULE)*/918);
		else slow_print(text(cur_sym));
		print(/*' isn't an internal quantity*/919);
		help1(/*Something like `tracingonline' should follow `interim'.*/920); back_error();
	}
	else {
		save_internal(cur_mod); back_input();
	}
	do_statement();
}


// 1035
void do_let()
{
	pointer l; // hash location of the left-hand symbol
	get_symbol(); l = cur_sym; get_x_next();
	if (cur_cmd != equals)
		if (cur_cmd != assignment) {
			missing_err(/*=*/61);
			help3(/*You should have said `let symbol = something'.*/921,
				/*But don't worry; I'll pretend that an equals sign*/678,
				/*was present. The next token I read will be `something'.*/922); back_error();
		}
	get_symbol();
	switch(cur_cmd) {
		case defined_macro:
		case secondary_primary_macro:
		case tertiary_secondary_macro:
		case expression_tertiary_macro:
			add_mac_ref(cur_mod);
			break;
		default:
			// do_nothing
			break;
	}
	clear_symbol(l, false); eq_type(l) = cur_cmd;
	if (cur_cmd == tag_token) equiv(l) = null;
	else equiv(l) = cur_mod;
	get_x_next();

}

// 1036
void do_new_internal()
{
	do {
		if (int_ptr == max_internal) overflow(/*number of internals*/923, max_internal);
		get_clear_symbol(); int_ptr++; eq_type(cur_sym) = internal_quantity; equiv(cur_sym) = int_ptr;
		int_name[int_ptr] = text(cur_sym); internal[int_ptr] = 0; get_x_next();
	} while (!(cur_cmd != comma));
}

// 1040
void do_show()
{
	do { 
		get_x_next(); scan_expression();
		print_nl(/*>> */757);
		print_exp(null, 2); flush_cur_exp(0);
	} while (!( cur_cmd != comma));
}

// 1041
void disp_token()
{
	print_nl(/*> */924);
	if (cur_sym == 0)
	#pragma region <Show a numeric or string or capsule token 1042>
	{ 
		if (cur_cmd == numeric_token) print_scaled(cur_mod);
		else if (cur_cmd == capsule_token)
		{ 
			g_pointer = cur_mod; print_capsule();
		}
		else {
			print_char(/*"*/34); slow_print(cur_mod); print_char(/*"*/34);
			delete_str_ref(cur_mod);
		}
	}
	#pragma endregion
	else  { 
		slow_print(text(cur_sym)); print_char(/*=*/61);
		if (eq_type(cur_sym) >= outer_tag) print(/*(outer) */925);
		print_cmd_mod(cur_cmd,cur_mod);
		if (cur_cmd == defined_macro)
		{ 
			print_ln(); show_macro(cur_mod, null, 100000);
		} // {this avoids recursion between |show_macro| and |print_cmd_mod|}
	}
}

// 1044
void do_show_token()
{
	do {
		get_next(); disp_token(); get_x_next();
	} while(!(cur_cmd != comma));
}

// 1045
void do_show_stats()
{
	print_nl(/*Memory usage */926);
	#ifndef NO_STAT
	print_int(var_used); print_char(/*&*/38); print_int(dyn_used);
	if (false)
	#endif
	print(/*unknown*/365);
	print(/* (*/479); print_int(hi_mem_min-lo_mem_max-1);
	print(/* still untouched)*/927); print_ln();
	print_nl(/*String usage */928);
	print_int(str_ptr-init_str_ptr); print_char(/*&*/38);
	print_int(pool_ptr-init_pool_ptr);
	print(/* (*/479);
	print_int(max_strings-max_str_ptr); print_char(/*&*/38);
	print_int(pool_size-max_pool_ptr); print(/* still untouched)*/927); print_ln();
	get_x_next();
}

// 1046
void disp_var(pointer p)
{
	pointer q; //{traverses attributes and subscripts}
	int n; // 0..max_print_line; //{amount of macro text to show}

	if (type(p)== structured) 
		#pragma region <Descend the structure 1047>
	{ 
		q = attr_head(p);
		do { 
			disp_var(q); q = link(q);
		} while (!( q == end_attr));
		q = subscr_head(p);
		while (name_type(q) == subscr)
		{ 
			disp_var(q); q = link(q);
		}
	}
		#pragma endregion
	else if (type(p) >= unsuffixed_macro) 
		#pragma region <Display a variable macro 1048>
	{ 
		print_nl(/**/289); print_variable_name(p);
		if (type(p) > unsuffixed_macro) print(/*@#*/563); //{|suffixed_macro|}
		print(/*=macro:*/929);
		if (file_offset >= max_print_line - 20) n = 5;
		else n = max_print_line - file_offset - 15;
		show_macro(value(p),null,n);
	}
		#pragma endregion
	else if (type(p) != undefined)
	 {
		print_nl(/**/289); print_variable_name(p); print_char(/*=*/61);
		print_exp(p,0);
	 }
}

// 1049
void do_show_var()
{
	do { 
		get_next();
		if (cur_sym > 0)
			if (cur_sym <= hash_end)
				if (cur_cmd == tag_token)
					if (cur_mod != null)
					  { 
						disp_var(cur_mod); goto done;
					  }
		disp_token();
		done: get_x_next();
	} while (!( cur_cmd != comma));
}

// 1050
void do_show_dependencies()
{
	pointer p; //{link that runs through all dependencies}
	p = link(dep_head);
	while (p != dep_head)
	{ 
		if (interesting(p))
		{
			print_nl(/**/289); print_variable_name(p);
			if (type(p) == dependent) print_char(/*=*/61);
			else print(/* = */760); //{extra spaces imply proto-dependency}
			print_dependency(dep_list(p), type(p));
		}
		p = dep_list(p);
		while (info(p) != null) p = link(p);
		p = link(p);
	}
	get_x_next();
}


// 1051
void do_show_whatever()
{
	if (interaction == error_stop_mode) wake_up_terminal();
	switch(cur_mod)
	{
		case show_token_code:
			do_show_token();
			break;
		case show_stats_code:
			do_show_stats();
			break;
		case show_code:
			do_show();
			break;
		case show_var_code:
			do_show_var();
			break;
		case show_dependencies_code:
			do_show_dependencies();
			break;
	}
	if (internal[showstopping] > 0) {
		print_err(/*OK*/930);
		if (interaction < error_stop_mode) {
			help0; error_count--;
		}
		else help1(/*This isn't an error message; I'm just showing something.*/931);
		if (cur_cmd == semicolon) error(); else put_get_error();
	}
}

// 1054
bool scan_with()
{
	small_number t; //{known or pen type }
	bool result; //{the value to return}
	
	t = cur_mod; cur_type = vacuous; get_x_next(); scan_expression(); result = false;
	if (cur_type != t)
	#pragma region <Complain about improper type 1055>
	{
		exp_err(/*Improper type*/932);
		help2(/*Next time say `withweight <known numeric expression>';*/933,
		/*I'll ignore the bad `with' clause and look for another.*/934);
		if (t == pen_type) help_line[1] = /*Next time say `withpen <known pen expression>';*/935;
		put_get_flush_error(0);
	}
	#pragma endregion
	else if (cur_type == pen_type) result = true;
	else 
		#pragma region<Check the tentative weight 1056>
	{
		cur_exp = round_unscaled(cur_exp);
		if (myabs(cur_exp) < 4 && cur_exp != 0) result = true;
		else { 
			print_err(/*Weight must be -3, -2, -1, +1, +2, or +3*/936);
			help1(/*I'll ignore the bad `with' clause and look for another.*/934); put_get_flush_error(0);
		}
	}
		#pragma endregion
	return result;
}


// 1057
void find_edges_var(pointer t)
{
	pointer p;
	
	p = find_variable(t); cur_edges = null;
	if (p == null)
	{ 
		obliterated(t); put_get_error();
	}
	else if (type(p) != picture_type)
	{ 
		print_err(/*Variable */788); show_token_list(t,null,1000,0); print(/* is the wrong type (*/937);
		print_type(type(p)); print_char(/*)*/41);
		help2(/*I was looking for a "known" picture variable.*/938,
		/*So I'll not change anything just now.*/939); put_get_error();
	}
	else cur_edges = value(p);
	flush_node_list(t);
}


// 1059
void do_add_to()
{
	pointer lhs,rhs; //{variable on left, path on right}
	int w; //{tentative weight}
	pointer p; //{list manipulation register}
	pointer q; //{beginning of second half of doubled path}
	int add_to_type;//:double_path_code..also_code; //{modifier of \&{addto}}

	get_x_next(); var_flag = thing_to_add; scan_primary();
	if (cur_type != token_list)
		#pragma region <Abandon edges command because theres no variable>
	{ 
		exp_err(/*Not a suitable variable*/940);
		help4(/*At this point I needed to see the name of a picture variable.*/941,
			/*(Or perhaps you have indeed presented me with one; I might*/942,
			/*have missed it, if it wasn't followed by the proper token.)*/943,
			/*So I'll not change anything just now.*/939);
		put_get_flush_error(0);
	}
		#pragma endregion

	
	
	else { 
		lhs = cur_exp; add_to_type = cur_mod;
		cur_type = vacuous; get_x_next(); scan_expression();
		if (add_to_type == also_code)
			#pragma region <Augment some edges by others>
		{ 
			find_edges_var(lhs);
			if (cur_edges == null) flush_cur_exp(0);
			else if (cur_type != picture_type)
			{ 
				exp_err(/*Improper `addto'*/944);
				help2(/*This expression should have specified a known picture.*/945,
					/*So I'll not change anything just now.*/939); put_get_flush_error(0);
			}
			else  { 
				merge_edges(cur_exp); flush_cur_exp(0);
			}
		}		
			#pragma endregion
		else 
			#pragma region <Get ready to fill a contour, and fill it>
		{ 
			if (cur_type == pair_type) pair_to_path();
			if (cur_type != path_type)
			{ 
				exp_err(/*Improper `addto'*/944);
				help2(/*This expression should have been a known path.*/946,
					/*So I'll not change anything just now.*/939);
				put_get_flush_error(0); flush_token_list(lhs);
			}
			else  { 
				rhs = cur_exp; w = 1; cur_pen = null_pen;
				while (cur_cmd == with_option)
					if (scan_with())
						if (cur_type == known) w = cur_exp;
						else
							#pragma region <Change the tentative pen>
						{ 
							delete_pen_ref(cur_pen); cur_pen = cur_exp;
						}
							#pragma endregion
				
				#pragma region <Complete the contour filling operation>
				find_edges_var(lhs);
				if (cur_edges == null) toss_knot_list(rhs);
				else {
					lhs = null; cur_path_type = add_to_type;
					if (left_type(rhs) == endpoint)
						if (cur_path_type == double_path_code) 
							#pragma region <Double the path>
							if (link(rhs) == rhs) 
								#pragma region <Make a trivial one-point path cycle>
							{ 
								right_x(rhs) = x_coord(rhs); right_y(rhs) = y_coord(rhs);
								left_x(rhs) = x_coord(rhs); left_y(rhs) = y_coord(rhs);
								left_type(rhs) = _explicit; right_type(rhs) = _explicit;
							}					
								#pragma endregion
							else  { 
								p = htap_ypoc(rhs); q = link(p);
								right_x(path_tail) = right_x(q); right_y(path_tail) = right_y(q);
								right_type(path_tail) = right_type(q);
								link(path_tail) = link(q); free_node(q,knot_node_size);
								right_x(p) = right_x(rhs); right_y(p) = right_y(rhs);
								right_type(p) = right_type(rhs);
								link(p) = link(rhs); free_node(rhs,knot_node_size);
								rhs = p;
							}					
							#pragma endregion
						else 
							#pragma region <Complain about non-cycle and |goto not_found|>
						{ 
							print_err(/*Not a cycle*/947);
							help2(/*That contour should have ended with `..cycle' or `&cycle'.*/948,
								/*So I'll not change anything just now.*/939); put_get_error();
							toss_knot_list(rhs); goto not_found;
						}
							#pragma endregion 
					else if (cur_path_type == double_path_code) lhs = htap_ypoc(rhs);
					cur_wt = w; rhs = make_spec(rhs,max_offset(cur_pen),internal[tracing_specs]);
					#pragma region <Check the turning number>
					if (turning_number <= 0)
						if (cur_path_type != double_path_code)
							if (internal[turning_check] > 0)
								if (turning_number < 0 && link(cur_pen) == null) negate(cur_wt);
								else { 
									if (turning_number == 0)
										if (internal[turning_check ] <= unity && link(cur_pen) == null) goto done;
										else print_strange(/*Strange path (turning number is zero)*/949);
									else print_strange(/*Backwards path (turning number is negative)*/950);
									help3(/*The path doesn't have a counterclockwise orientation,*/951,
										/*so I'll probably have trouble drawing it.*/952,
										/*(See Chapter 27 of The METAFONTbook for more help.)*/953);
									put_get_error();
								}
					done:
					#pragma endregion
					if (max_offset(cur_pen) == 0) fill_spec(rhs);
					else fill_envelope(rhs);
					if (lhs != null)
					{ 
						rev_turns = true;
						lhs = make_spec(lhs,max_offset(cur_pen),internal[tracing_specs]);
						rev_turns = false;
						if (max_offset(cur_pen) == 0) fill_spec(lhs);
						else fill_envelope(lhs);
					}
			not_found:
					;
				}		
				#pragma endregion
				
				delete_pen_ref(cur_pen);
			}
		}	
			#pragma endregion
	}
}

// 1165
void ship_out(eight_bits c)
{
	int f; //{current character extension}
	int prev_m, m, mm; //{previous and current pixel column numbers}
	int prev_n,n; //{previous and current pixel row numbers}
	pointer p,q; //{for list traversal}
	int prev_w,w,ww; //{old and new weights}
	int d; //{data from edge-weight node}
	int delta; //{number of rows to skip}
	int cur_min_m = INT_MAX; //{starting column, relative to the current offset}
	int x_off,y_off; //{offsets, rounded to ints}

	check_gf; f = round_unscaled(internal[char_ext]);
	x_off = round_unscaled(internal[x_offset]);
	y_off = round_unscaled(internal[y_offset]);
	if (term_offset > max_print_line - 9) print_ln();
	else if (term_offset > 0 || file_offset > 0) print_char(/* */32);
	print_char(/*[*/91); print_int(c);
	if (f != 0)
	{
		print_char(/*.*/46); print_int(f);
	}
	update_terminal();
	boc_c = 256 * f + c; boc_p = char_ptr[c]; char_ptr[c] = gf_prev_ptr;
	if (internal[proofing] > 0) 
		#pragma region <Send nonzero offsets to the output file>
	{ 
		if (x_off != 0)
		{ 
			gf_string(/*xoffset*/954,0); gf_out(yyy); gf_four(x_off*unity);
		}
		if (y_off != 0)
		{
			gf_string(/*yoffset*/955,0); gf_out(yyy); gf_four(y_off*unity);
		}
	}		
		#pragma endregion
	#pragma region <Output the character represented in |cur_edges|>
	prev_n = 4096; p = knil(cur_edges); n = n_max(cur_edges) - zero_field;
	while (p != cur_edges)
	{
		#pragma region <Output the pixels of edge row |p| to font row |n|>
		if (unsorted(p) > _void) sort_edges(p);
		q = sorted(p); w = 0; prev_m = -fraction_one; //{$|fraction_one|\approx\infty$}
		ww = 0; prev_w = 0; m = prev_m;
		do { 
			if (q==sentinel) mm = fraction_one;
			else { 
				d = ho(info(q)); mm = d / 8; ww = ww + (d % 8) - zero_w;
			}
			if (mm != m)
			{ 
				if (prev_w <= 0)
				{ 
					if (w > 0)
						#pragma region <Start black at $(m,n)$>
					{ 
						if (prev_m == -fraction_one) 
							#pragma region <Start a new row at $(m,n)$>
						{ 
							if (prev_n == 4096)
							{ 
								gf_boc(m_min(cur_edges) + x_off - zero_field, m_max(cur_edges) + x_off - zero_field, 
										n_min(cur_edges) + y_off - zero_field, n + y_off);
								cur_min_m = m_min(cur_edges) - zero_field + m_offset(cur_edges);
							}
							else if (prev_n > n + 1) 
								#pragma region <Skip down |prev_n-n| rows>
							{
								delta = prev_n - n - 1;
								if (delta < 0400)
								{ 
									gf_out(skip1); gf_out(delta);
								}
								else  { 
									gf_out(skip1 + 1); gf_two(delta);
								}
							}
								#pragma endregion
							else 
								#pragma region <Skip to column $m$ in the next row and |goto done|, or skip zero rows>
							{
								delta = m - cur_min_m;
								if (delta > max_new_row) gf_out(skip0);
								else  { 
									gf_out(new_row_0 + delta); goto done;
								}
							}						
								#pragma endregion
							gf_paint(m - cur_min_m); //{skip to column $m$, painting white}
						done:
							prev_n = n;
						}					
							#pragma endregion
						else gf_paint(m - prev_m);
						prev_m = m; prev_w = w;
					}				
						#pragma endregion
				}
				else if (w <= 0) 
					#pragma region <Stop black at $(m,n)$>
				{ 
					gf_paint(m - prev_m); prev_m = m; prev_w = w;
				}		
					#pragma endregion
				m = mm;
			}
			w = ww; q = link(q);
		} while (!(mm == fraction_one));
		if (w != 0) //{this should be impossible}
			print_nl(/*(There's unbounded black in character shipped out!)*/956);
		if (prev_m - m_offset(cur_edges) + x_off > gf_max_m)
			gf_max_m = prev_m - m_offset(cur_edges) + x_off;
			
		#pragma endregion
		p = knil(p); n--;
	}
	if (prev_n == 4096)
		#pragma region <Finish off an entirely blank character>
	{
		gf_boc(0,0,0,0);
		if (gf_max_m < 0) gf_max_m = 0;
		if (gf_min_n > 0) gf_min_n = 0;
	}	
		#pragma endregion
	else if (prev_n + y_off < gf_min_n)
		gf_min_n = prev_n + y_off;
	#pragma endregion
	gf_out(eoc); gf_prev_ptr = gf_offset + gf_ptr; total_chars++;
	print_char(/*]*/93); update_terminal(); //{progress report}
	if (internal[tracing_output] > 0)
		print_edges(/* (just shipped out)*/957,true,x_off,y_off);
}

// 1070
void do_ship_out()
{
	int c; //{the character code}
	get_x_next(); var_flag = semicolon; scan_expression();
	if (cur_type != token_list)
	if (cur_type == picture_type) cur_edges = cur_exp;
	else {
		#pragma region <Abandon edges command because theres no variable>
		{ 
			exp_err(/*Not a suitable variable*/940);
			help4(/*At this point I needed to see the name of a picture variable.*/941,
				/*(Or perhaps you have indeed presented me with one; I might*/942,
				/*have missed it, if it wasn't followed by the proper token.)*/943,
				/*So I'll not change anything just now.*/939);
			put_get_flush_error(0);
		}
		#pragma endregion
		return;
	}
	else { 
	find_edges_var(cur_exp); cur_type = vacuous;
	}
	if (cur_edges != null)
	{
		c = round_unscaled(internal[char_code]) % 256;
		if (c < 0) c = c + 256;
		#pragma region <Store the width information for character code~|c|>
		if (c < bc) bc = c;
		if (c > ec) ec = c;
		char_exists[c] = true;
		gf_dx[c] = internal[char_dx]; gf_dy[c] = internal[char_dy];
		tfm_width[c] = tfm_check(char_wd);
		tfm_height[c] = tfm_check(char_ht);
		tfm_depth[c] = tfm_check(char_dp);
		tfm_ital_corr[c] = tfm_check(char_ic);
		#pragma endregion
		if (internal[proofing] >= 0) ship_out(c);
	}
	flush_cur_exp(0);
}


// 1071
void do_display()
{
	pointer e; //{token list for a picture variable}
	get_x_next(); var_flag = in_window; scan_primary();
	if (cur_type != token_list)
	#pragma region <Abandon edges command because theres no variable>
	{ 
		exp_err(/*Not a suitable variable*/940);
		help4(/*At this point I needed to see the name of a picture variable.*/941,
			/*(Or perhaps you have indeed presented me with one; I might*/942,
			/*have missed it, if it wasn't followed by the proper token.)*/943,
			/*So I'll not change anything just now.*/939);
		put_get_flush_error(0);
	}
	#pragma endregion
	else  { 
		e = cur_exp; cur_type = vacuous;
		get_x_next(); scan_expression();
		if (cur_type != known) goto common_ending;
		cur_exp = round_unscaled(cur_exp);
		if (cur_exp < 0) goto not_found;
		if (cur_exp > 15) goto not_found;
		if (!window_open[cur_exp]) goto not_found;
		find_edges_var(e);
		if (cur_edges != null) disp_edges(cur_exp);
		return;
	not_found: 
		cur_exp = cur_exp * unity;
	common_ending:
		exp_err(/*Bad window number*/958);
		help1(/*It should be the number of an open window.*/959);
		put_get_flush_error(0); flush_token_list(e);
	}
}

// 1072
bool get_pair(command_code c)
{
	pointer p; //{a pair of values that are known (we hope)}
	bool b; //{did we find such a pair?}
	if (cur_cmd != c) return false;
	else {
		get_x_next(); scan_expression();
		if (nice_pair(cur_exp,cur_type))
		{ 
			p = value(cur_exp);
			cur_x = value(x_part_loc(p)); cur_y = value(y_part_loc(p));
			b = true;
		}
		else b = false;
		flush_cur_exp(0); return b;
	}
}



// 1073
void do_open_window()
{
	int k; //{the window number in question}
	scaled r0, c0, r1, c1; //{window coordinates}
	get_x_next(); scan_expression();
	if (cur_type != known) goto not_found;
	k = round_unscaled(cur_exp);
	if (k < 0) goto not_found;
	if (k > 15) goto not_found;
	if (!get_pair(from_token)) goto not_found;
	r0 = cur_x; c0 = cur_y;
	if (!get_pair(to_token)) goto not_found;
	r1 = cur_x; c1 = cur_y;
	if (!get_pair(at_token)) goto not_found;
	open_a_window(k,r0,c0,r1,c1,cur_x,cur_y); return;
not_found:
	print_err(/*Improper `openwindow'*/960);
	help2(/*Say `openwindow k from (r0,c0) to (r1,c1) at (x,y)',*/961,
	/*where all quantities are known and k is between 0 and 15.*/962);
	put_get_error();
}


// 1074
void do_cull()
{
	pointer e; //{token list for a picture variable}
	int keeping; //:drop_code..keep_code; //{modifier of |cull_op|}
	int w,w_in,w_out; //{culling weights}
	w = 1;
	get_x_next(); var_flag = cull_op; scan_primary();
	if (cur_type != token_list)
		#pragma region <Abandon edges command because theres no variable>
	{ 
		exp_err(/*Not a suitable variable*/940);
		help4(/*At this point I needed to see the name of a picture variable.*/941,
		/*(Or perhaps you have indeed presented me with one; I might*/942,
		/*have missed it, if it wasn't followed by the proper token.)*/943,
		/*So I'll not change anything just now.*/939);
		put_get_flush_error(0);
	}	
		#pragma endregion
	else { 
	e = cur_exp; cur_type = vacuous; keeping = cur_mod;
	if (!get_pair(cull_op)) goto not_found;
	while (cur_cmd == with_option && cur_mod == known)
		if (scan_with()) w = cur_exp;
	#pragma region <Set up the culling weights,or |goto not_found| if the thresholds are bad>
	if (cur_x > cur_y) goto not_found;
	if (keeping == drop_code)
	{ 
		if (cur_x > 0 || cur_y < 0) goto not_found;
		w_out = w; w_in = 0;
	}
	else {
		if (cur_x <= 0 && cur_y >= 0) goto not_found;
		w_out = 0; w_in = w;
	}
	#pragma endregion
	find_edges_var(e);
	if (cur_edges != null)
		cull_edges(floor_unscaled(cur_x + unity - 1),floor_unscaled(cur_y),w_out,w_in);
	return;
	not_found: 
	print_err(/*Bad culling amounts*/963);
	help1(/*Always cull by known amounts that exclude 0.*/964);
	put_get_error(); flush_token_list(e);
	}
}


// 1082
void do_message()
{
	int m; // message_code..err_help_code, the type of message
	m = cur_mod; get_x_next(); scan_expression();
	if (cur_type != string_type) {
		exp_err(/*Not a string*/700);
		help1(/*A message should be a known string expression.*/965);
		put_get_error();
	}
	else switch(m) {
		case message_code:
			print_nl(/**/289); slow_print(cur_exp);
			break;
		case err_message_code:
			#pragma region <Print string cur_exp as an error message 1086>
			print_err(/**/289); slow_print(cur_exp);
			if (err_help != 0) use_err_help = true;
			else if (long_help_seen)
				help1(/*(That was another `errmessage'.)*/966);
			else {
				if (interaction < error_stop_mode) long_help_seen = true;
				help4(/*This error message was generated by an `errmessage'*/967,
					/*command, so I can't give any explicit help.*/968,
					/*Pretend that you're Miss Marple: Examine all clues,*/969,
					/*and deduce the truth by inspired guesses.*/970);
			}
			put_get_error(); use_err_help = false;
			#pragma endregion
			break;
		case err_help_code:
			#pragma region <Save string cur_exp as the err_help 1083>
			if (err_help != 0) delete_str_ref(err_help);
			if (length(cur_exp) == 0) err_help = 0;
			else {
				err_help = cur_exp; add_str_ref(err_help);
			}
			#pragma endregion
			break;
	}
	flush_cur_exp(0);

}

// 1103
eight_bits get_code() //{scans a character code value}
{
	int c; //{the code value found}
	get_x_next(); scan_expression();
	if (cur_type == known)
	{ 
		c = round_unscaled(cur_exp);
		if (c >= 0)
			if (c < 256) goto found;
	}
	else if (cur_type == string_type)
		if (length(cur_exp) == 1)
	{ 
		c = so(str_pool[str_start[cur_exp]]); goto found;
	}
	exp_err(/*Invalid code has been replaced by 0*/971);
	help2(/*I was looking for a number between 0 and 255, or for a*/972,
		/*string of length 1. Didn't find it; will use 0 instead.*/973);
	put_get_flush_error(0); c = 0;
found:
	return c;
}


// 1098
scaled tfm_check(small_number m)
{
	if (myabs(internal[m]) >= fraction_half)
	{ 
		print_err(/*Enormous */974); print(int_name[m]);
		print(/* has been reduced*/975);
		help1(/*Font metric dimensions must be less than 2048pt.*/976);
		put_get_error();
		if (internal[m] > 0) return fraction_half - 1;
		else return 1 - fraction_half;
	}
	else return internal[m];
}

// 1104
void set_tag(halfword c, small_number t, halfword r)
{
	if (char_tag[c] == no_tag)
	{
		char_tag[c] = t; char_remainder[c] = r;
		if (t == lig_tag)
		{
			label_ptr++; label_loc[label_ptr] = r; label_char[label_ptr] = (eight_bits)c;
		}
	}
	else 
		#pragma region <Complain about a character tag conflict>
	{ 
		print_err(/*Character */977);
		if ( c > /* */32 && c < 127) print(c);
		else if (c == 256) print(/*||*/978);
		else { 
			print(/*code */979); print_int(c);
		}
		print(/* is already */980);
		switch(char_tag[c]) {
			case lig_tag: 
				print(/*in a ligtable*/981);
				break;
			case list_tag: 
				print(/*in a charlist*/982);
				break;
			case ext_tag: 
				print(/*extensible*/607);
				break;
		} // {there are no other cases}
		help2(/*It's not legal to label a character more than once.*/983,
			/*So I'll not change anything just now.*/939);
		put_get_error(); 
	}

		#pragma endregion
}

// 1106
void do_tfm_command()
{
	int c, cc; //:0..256; {character codes}
	int k; //:0..max_kerns; {index into the |kern| array}
	int j; //{index into |header_byte| or |param|}
	switch(cur_mod)
	{
		case char_list_code: 
			c = get_code();
			//{we will store a list of character successors}
			while (cur_cmd == colon)
			{ 
				cc = get_code(); set_tag(c, list_tag, cc); c = cc;
			}
			break;
		case lig_table_code: 
			#pragma region <Store a list of ligature/kern steps>
			lk_started = false;
		mycontinue: 
			get_x_next();
			if(cur_cmd == skip_to && lk_started)
			#pragma region <Process a |skip_to| command and |goto done|>
			{ 
				c = get_code();
				if (nl - skip_table[c] > 128)
				{ 
					skip_error(skip_table[c]); skip_table[c] = undefined_label;
				}
				if (skip_table[c] == undefined_label) skip_byte(nl - 1) = qi(0);
				else skip_byte(nl-1) = qi(nl - skip_table[c] - 1);
				skip_table[c] = nl - 1; goto done;
			}
			#pragma endregion
			if (cur_cmd == bchar_label)
			{ 
				c = 256; cur_cmd = colon;
			}
			else {
				back_input(); c = get_code();
			}
			if(cur_cmd == colon || cur_cmd == double_colon)
			#pragma region <Record a label in a lig/kern subprogram and |goto continue|>
			{ 
				if (cur_cmd == colon)
					if (c == 256) bch_label = nl;
					else set_tag(c,lig_tag,nl);
				else if (skip_table[c] < undefined_label)
				{ 
					ll = skip_table[c]; skip_table[c] = undefined_label;
					do {
						lll = qo(skip_byte(ll));
						if (nl - ll > 128)
						{ 
							skip_error(ll); goto mycontinue;
						}
						skip_byte(ll) = qi(nl - ll - 1); ll = ll - lll;
					} while (!(lll == 0));
				}
				goto mycontinue;
			}	
			#pragma endregion
			if (cur_cmd == lig_kern_token)
				#pragma region <Compile a ligature/kern command>
			{ 
				next_char(nl) = qi(c); skip_byte(nl) = qi(0);
				if (cur_mod < 128) //{ligature op}
				{
					op_byte(nl) = qi(cur_mod); rem_byte(nl) = qi(get_code());
				}
				else { 
					get_x_next(); scan_expression();
					if (cur_type != known)
					{ 
						exp_err(/*Improper kern*/984);
						help2(/*The amount of kern should be a known numeric value.*/985,
							/*I'm zeroing this one. Proceed, with fingers crossed.*/313);
						put_get_flush_error(0);
					}
					kern[nk] = cur_exp;
					k = 0;
					while (kern[k] != cur_exp) k++;
					if (k == nk)
					{ 
						if (nk == max_kerns) overflow(/*kern*/618,max_kerns);
						nk++;
					}
					op_byte(nl) = kern_flag + (k / 256);
					rem_byte(nl) = qi((k % 256));
				}
				lk_started = true;
			}		
				#pragma endregion
			else { print_err(/*Illegal ligtable step*/986);
			help1(/*I was looking for `=:' or `kern' here.*/987);
			back_error(); next_char(nl) = qi(0); op_byte(nl) = qi(0); rem_byte(nl) = qi(0);
			skip_byte(nl) = stop_flag + 1; //{this specifies an unconditional stop}
			}
			if (nl == lig_table_size) overflow(/*ligtable size*/988,lig_table_size);
			nl++;
			if (cur_cmd == comma) goto mycontinue;
			if (skip_byte(nl - 1) < stop_flag) skip_byte(nl - 1) = stop_flag;
		done:			
			#pragma endregion
			break;
		case extensible_code: 
			#pragma region <Define an extensible recipe>
			if (ne == 256) overflow(/*extensible*/607,256);
			c = get_code(); set_tag(c,ext_tag,ne);
			if (cur_cmd != colon) missing_extensible_punctuation(/*:*/58);
			ext_top(ne) = qi(get_code());
			if (cur_cmd != comma) missing_extensible_punctuation(/*,*/44);
			ext_mid(ne) = qi(get_code());
			if (cur_cmd != comma) missing_extensible_punctuation(/*,*/44);
			ext_bot(ne) = qi(get_code());
			if (cur_cmd != comma) missing_extensible_punctuation(/*,*/44);
			ext_rep(ne) = qi(get_code());
			ne++;		
			#pragma endregion
			break;
		case header_byte_code: case font_dimen_code: 
			c = cur_mod; get_x_next();
			scan_expression();
			if (cur_type != known || cur_exp < half_unit)
			{
				exp_err(/*Improper location*/989);
				help2(/*I was looking for a known, positive number.*/990,
					/*For safety's sake I'll ignore the present command.*/991);
				put_get_error();
			}
			else { 
				j = round_unscaled(cur_exp);
				if (cur_cmd != colon)
				{
					missing_err(/*:*/58);
					help1(/*A colon should follow a headerbyte or fontinfo location.*/992);
					back_error();
				}
				if (c == header_byte_code)
					#pragma region <Store a list of header bytes>
					do { 
						if (j > header_size) overflow(/*headerbyte*/608,header_size);
						header_byte[j] = get_code(); j++;
					} while (!(cur_cmd != comma));
				
					#pragma endregion
				else 
					#pragma region <Store a list of font dimensions>
					do { 
						if (j > max_font_dimen) overflow(/*fontdimen*/609,max_font_dimen);
						while (j > np)
						{ 
							np++; param[np] = 0;
						}
						get_x_next(); scan_expression();
						if (cur_type != known)
						{ 
							exp_err(/*Improper font parameter*/993);
							help1(/*I'm zeroing this one. Proceed, with fingers crossed.*/313);
							put_get_flush_error(0);
						}
						param[j] = cur_exp; j++;
					} while (!(cur_cmd != comma));
					#pragma endregion
			}
			break;
	} //{there are no other cases}
}

// 1117
pointer sort_in(scaled v)
{
	pointer p, q, r;
	p = temp_head;
	while (true) {
		q = link(p);
		if (v <= value(q)) goto found;
		p = q;
	}
found:
	if (v < value(q)) {
		r = get_node(value_node_size); value(r) = v; link(r) = q; link(p) = r;
	}
	return link(p);
}

// 1118
int min_cover(scaled d)
{
	pointer p; // runs through the current list
	scaled l; // the least element covered by the current interval
	int m; // lower bound on the size of the minimum cover

	m = 0; p = link(temp_head); perturbation = el_gordo;
	while (p != inf_val) {
		m++; l = value(p);
		do {
			p = link(p);
		} while (!(value(p) > l + d));
		if (value(p) - l < perturbation) perturbation = value(p) - l;
	}
	return m;
}


// 1120
scaled threshold(int m)
{
	scaled d; // lower bound on the smallest interval size
	excess = min_cover(0) - m;
	if (excess <= 0) return 0;
	else {
		do {
			d = perturbation;
		} while (!(min_cover(d + d) <= m));

		while (min_cover(d) > m) d = perturbation;
		return d;
	}
}


// 1121
int skimp(int m)
{
	scaled d; // the size of intervals being coalesced
	pointer p, q, r; // list manipulation registers
	scaled l; // the least value in the current interval
	scaled v; // a compromise value

	d = threshold(m); perturbation = 0; q = temp_head; m = 0; p = link(temp_head);
	while (p != inf_val) {
		m++; l = value(p); info(p) = m;
		if (value(link(p)) <= l + d)
			#pragma region <Replace an interval of values by its midpoint 1122>
		{
			do {
				p = link(p); info(p) = m; excess--; if (excess == 0) d = 0;
			} while (!(value(link(p)) > l + d));
			v = l + half(value(p) - l);
			if (value(p) - v > perturbation) perturbation = value(p) - v;
			r = q;
			do {
				r = link(r); value(r) = v;
			} while(!(r == p));
			link(q) = p; // remove duplicate values from the current list
		}
			#pragma endregion
		q = p; p = link(p);

	}
	return m;
}

// 1123
void tfm_warning(small_number m)
{
	print_nl(/*(some */994); print(int_name[m]);
	print(/* values had to be adjusted by as much as */995); print_scaled(perturbation); print(/*pt)*/996);
}

// 1128
void fix_design_size()
{
	scaled d; // the design size
	d = internal[design_size];
	if (d < unity || d >= fraction_half) {
		if (d != 0) print_nl(/*(illegal design size has been changed to 128pt)*/997);
		d = 040000000; internal[design_size] = d;
	}
	if (header_byte[5] < 0)
		if (header_byte[6] < 0)
			if (header_byte[7] < 0)
				if (header_byte[8] < 0) {
					header_byte[5] = d / 04000000; header_byte[6] = (d / 4096) % 256;
					header_byte[7] = (d / 16) % 256; header_byte[8] = (d % 16) * 16;
				}
	max_tfm_dimen = 16 * internal[design_size] - 1 - internal[design_size] / 010000000;
	if (max_tfm_dimen >= fraction_half) max_tfm_dimen = fraction_half - 1;
}

// 1129
int dimen_out(scaled x)
{
	if (myabs(x) > max_tfm_dimen) {
		tfm_changed++;
		if (x > 0) x = max_tfm_dimen; else x = -max_tfm_dimen;
	}
	x = make_scaled(x * 16, internal[design_size]);
	return x;
}

// 1131
void fix_check_sum()
{
	eight_bits k; // runs through character codes
	eight_bits b1, b2, b3, b4; // bytes of the check sum
	int x; // hash value used in check sum computation

	if (header_byte[1] < 0)
		if (header_byte[2] < 0)
			if (header_byte[3] < 0)
				if (header_byte[4] < 0) {

					#pragma region <Compute a check sum in (b1, b2, b3, b4) 1132>
					b1 = bc; b2 = ec; b3 = bc; b4 = ec; tfm_changed = 0;
					for (k = bc; k <= ec; k++)
						if (char_exists[k]) {
							x = dimen_out(value(tfm_width[k])) + (k + 4) * 020000000; // this is positive
							b1 = (b1 + b1 + x) % 255; b2 = (b2 + b2 + x) % 253; b3 = (b3 + b3 + x) % 251;
							b4 = (b4 + b4 + x) % 247;
						}
					#pragma endregion

					header_byte[1] = b1; header_byte[2] = b2; header_byte[3] = b3; header_byte[4] = b4; return;
				}
	for (k = 1; k <= 4; k++)
		if (header_byte[k] < 0) header_byte[k] = 0;

}

// 1133
void tfm_out(int c)
{
	fputc(c, tfm_file);
}

void tfm_two(int x) // output two bytes to tfm_file
{
	tfm_out(x / 256); tfm_out(x % 256);
}

void tfm_four(int x) // ouput four bytes to tfm_file
{
	if (x >= 0) tfm_out(x / three_bytes);
	else {
		x = x + 010000000000; // use two's complement for negative values
		x = x + 010000000000; tfm_out((x / three_bytes) + 128);
	}
	x = x % three_bytes; tfm_out(x / unity); x = x % unity; tfm_out(x / 0400);
	tfm_out(x % 0400);
}

void tfm_qqqq(four_quarters x) // output four quarterwords to tfm_file
{
	tfm_out(qo(x.b0)); tfm_out(qo(x.b1)); tfm_out(qo(x.b2)); tfm_out(qo(x.b3));
}

// 1154
void write_gf(gf_index a, gf_index b)
{
	gf_index k;

	for (k = a; k <= b; k++) fputc(gf_buf[k], gf_file);
}


// 1155
void gf_swap() // outputs half of the buffer
{
	if (gf_limit == gf_buf_size) {
		write_gf(0, half_buf - 1); gf_limit = half_buf; gf_offset = gf_offset + gf_buf_size; gf_ptr = 0;
	}
	else {
		write_gf(half_buf, gf_buf_size - 1); gf_limit = gf_buf_size;
	}
}

// 1157
void gf_four(int x)
{
	if (x >= 0) gf_out(x / three_bytes);
	else {
		x = x + 010000000000; x = x + 010000000000; gf_out((x / three_bytes) + 128);
	}
	x = x % three_bytes; gf_out(x / unity); x = x % unity; gf_out(x / 0400); gf_out(x % 0400);
}

// 1158
void gf_two(int x)
{
	gf_out(x / 0400); gf_out(x % 0400);
}

void gf_three(int x)
{
	gf_out(x / unity); gf_out((x % unity) / 0400); gf_out(x % 0400);
}

// 1159
void gf_paint(int d) //{here |0<=d<65536|}
{
	if (d < 64) gf_out(paint_0 + d);
	else if (d < 256)
	{ 
		gf_out(paint1); gf_out(d);
	}
	else {
		gf_out(paint1 + 1); gf_two(d);
	}
}



// 1160
void gf_string(str_number s, str_number t)
{
	pool_pointer k;
	int l;// {length of the strings to output}
	if (s != 0)
	{
		l = length(s);
		if (t != 0) l = l + length(t);
		if (l <= 255)
		{
			gf_out(xxx1); gf_out(l);
		}
		else {
			gf_out(xxx3); gf_three(l);
		}
		for (k = str_start[s]; k <= str_start[s + 1] - 1; k++) gf_out(so(str_pool[k]));
	}
	if (t != 0)
		for (k = str_start[t]; k <= str_start[t + 1] - 1; k++) gf_out(so(str_pool[k]));
}

// 1161
void gf_boc(int min_m, int max_m, int min_n, int max_n)
{
	if (min_m < gf_min_m) gf_min_m = min_m;
	if (max_n > gf_max_n) gf_max_n = max_n;
	if (boc_p == -1)
		if (one_byte(boc_c))
			if (one_byte(max_m - min_m))
				if (one_byte(max_m))
					if (one_byte(max_n - min_n)) 
						if (one_byte(max_n))
						{
							gf_out(boc1); gf_out(boc_c);
							gf_out(max_m - min_m); gf_out(max_m);
							gf_out(max_n - min_n); gf_out(max_n); 
							return;
						}
	gf_out(boc); gf_four(boc_c); gf_four(boc_p);
	gf_four(min_m); gf_four(max_m); gf_four(min_n); gf_four(max_n);
}




// 1163
void init_gf()
{
	eight_bits k; //{runs through all possible character codes}
	int t; //{the time of this run}
	gf_min_m = 4096; gf_max_m = -4096; gf_min_n = 4096; gf_max_n = -4096;
	k = 0;
	do {
		char_ptr[k] = -1;
	} while (k++ != 255);
	//for (k = 0; k > 255; k++) char_ptr[k] = -1;
	#pragma region <Determine the file extension, |gf_ext|>
	if (internal[hppp] <= 0) gf_ext = /*.gf*/998;
	else {
		old_setting = selector; selector = new_string; print_char(/*.*/46);
		print_int(make_scaled(internal[hppp],59429463));
		// {$2^{32}/72.27\approx59429463.07$}
		print(/*gf*/999); gf_ext = make_string(); selector = old_setting;
	}
	#pragma endregion
	set_output_file_name;
	gf_out(pre); gf_out(gf_id_byte); //{begin to output the preamble}
	old_setting = selector; selector = new_string; print(/* METAFONT output */1000);
	print_int(round_unscaled(internal[year])); print_char(/*.*/46);
	print_dd(round_unscaled(internal[month])); print_char(/*.*/46);
	print_dd(round_unscaled(internal[day])); print_char(/*:*/58);
	t = round_unscaled(internal[_time]);
	print_dd(t / 60); print_dd(t % 60);
	selector = old_setting; gf_out(cur_length);
	str_start[str_ptr + 1] = pool_ptr; gf_string(0,str_ptr);
	pool_ptr = str_start[str_ptr]; //{flush that string from memory}
	gf_prev_ptr = gf_offset + gf_ptr;
}



// 1177
void do_special()
{
	small_number m; //{either |string_type| or |known|}

	m = cur_mod; get_x_next(); scan_expression();
	if (internal[proofing] >= 0)
		if (cur_type != m)
		#pragma region <Complain about improper special operation>
		{
			exp_err(/*Unsuitable expression*/1001);
			help1(/*The expression shown above has the wrong type to be output.*/1002);
			put_get_error();
		}
		#pragma endregion
		else {
			check_gf;
			if (m == string_type) gf_string(cur_exp,0);
			else {
				gf_out(yyy); gf_four(cur_exp);
			}
		}
	flush_cur_exp(0);
}


// 1186
#ifndef NO_INIT

void store_base_file()
{
	int k; // all-purpose index
	pointer p, q; // all-purpose pointers
	int x; // something to dump
	four_quarters w; // four ASCII codes

	#pragma region <Create the base_ident, open the base file, and inform the user that dumping has begun 1200>
	selector = new_string; print(/* (preloaded base=*/1003); print(job_name); print_char(/* */32);
	print_int(round_unscaled(internal[year])); print_char(/*.*/46); print_int(round_unscaled(internal[month]));
	print_char(/*.*/46); print_int(round_unscaled(internal[day])); print_char(/*)*/41);
	if (interaction == batch_mode) selector = log_only;
	else selector = term_and_log;
	str_room(1); base_ident = make_string(); str_ref[base_ident] = max_str_ref;
	pack_job_name(base_extension);
	while (!w_open_out(&base_file)) prompt_file_name(/*base file name*/1004, base_extension);
	print_nl(/*Beginning to dump on file */1005); slow_print(w_make_name_string(base_file));
	flush_string(str_ptr - 1); print_nl(/**/289); slow_print(base_ident);
	#pragma endregion


	#pragma region <Dump constants for consistency check 1190>
	dump_int(271969154);
	dump_int(mem_min);
	dump_int(mem_top);
	dump_int(hash_size);
	dump_int(hash_prime);
	dump_int(max_in_open);
	#pragma endregion


	#pragma region <Dump the string pool 1192>
	dump_int(pool_ptr); dump_int(str_ptr);
	for (k = 0; k <= str_ptr; k++) dump_int(str_start[k]);
	k = 0;
	while (k + 4 < pool_ptr) {
		dump_four_ASCII; k = k + 4;
	}
	k = pool_ptr - 4; dump_four_ASCII; print_ln(); print_int(str_ptr);
	print(/* strings of total length */1006); print_int(pool_ptr);
	#pragma endregion


	#pragma region <Dump the dynamic memory 1194>
	sort_avail(); var_used = 0; dump_int(lo_mem_max); dump_int(rover); p = mem_min; q = rover; x = 0;
	do {
		for (k = p; k <= q + 1; k++) dump_wd(mem[k]);
		x = x + q + 2 - p; var_used = var_used + q - p; p = q + node_size(q); q = rlink(q);
	} while (!(q == rover));
	var_used = var_used + lo_mem_max - p; dyn_used = mem_end + 1 - hi_mem_min;
	for (k = p; k <= lo_mem_max; k++) dump_wd(mem[k]);
	x = x + lo_mem_max + 1 - p; dump_int(hi_mem_min); dump_int(avail);
	for (k = hi_mem_min; k <= mem_end; k++) dump_wd(mem[k]);
	x = x + mem_end + 1 - hi_mem_min; p = avail;
	while (p != null) {
		dyn_used--; p = link(p);
	}
	dump_int(var_used); dump_int(dyn_used); print_ln(); print_int(x);
	print(/* memory locations dumped; current usage is */1007); print_int(var_used); print_char(/*&*/38);
	print_int(dyn_used);
	#pragma endregion


	#pragma region <Dump the table of equivalents and the hash table 1196>
	dump_int(hash_used); st_count = frozen_inaccessible - 1 - hash_used;
	for (p = 1; p <= hash_used; p++)
		if (text(p) != 0) {
			dump_int(p); dump_hh(hash[p]); dump_hh(eqtb[p]); st_count++;
		}
	for (p = hash_used + 1; p <= hash_end; p++) {
		dump_hh(hash[p]); dump_hh(eqtb[p]);
	}
	dump_int(st_count);
	print_ln(); print_int(st_count); print(/* symbolic tokens*/1008);
	#pragma endregion


	#pragma region <Dump a few more things and the closing check word 1198>
	dump_int(int_ptr);
	for (k = 1; k <= int_ptr; k++) {
		dump_int(internal[k]); dump_int(int_name[k]);
	}
	dump_int(start_sym); dump_int(interaction); dump_int(base_ident); dump_int(bg_loc);
	dump_int(eg_loc); dump_int(serial_no); dump_int(69069); internal[tracing_stats] = 0;
	#pragma endregion


	#pragma region <Close the base file 1201>
	w_close(base_file);
	#pragma endregion





}
#endif



// 1187
bool load_base_file()
{
	int k; // all-purpose index
	pointer p, q; // all-purpose pointers
	int x; // something undumped
	four_quarters w; // four ASCII codes

	#pragma region <Undump constants for consistency check 1191>
	undump_int(&x);
	if (x != 271969154) goto off_base; // check that strings are the same
	undump_int(&x);
	if (x != mem_min) goto off_base;
	undump_int(&x);
	if (x != mem_top) goto off_base;
	undump_int(&x);
	if (x != hash_size) goto off_base;
	undump_int(&x);
	if (x != hash_prime) goto off_base;
	undump_int(&x);
	if (x != max_in_open) goto off_base;
	#pragma endregion


	#pragma region <Undump the string pool 1193>
	undump_size(0, pool_size, "string pool size", pool_ptr);
	undump_size(0, max_strings, "max strings", str_ptr);
	for (k = 0; k <= str_ptr; k++) {
		undump(0, pool_ptr, str_start[k]); str_ref[k] = max_str_ref;
	}
	k = 0;
	while (k + 4 < pool_ptr) {
		undump_four_ASCII; k = k + 4;
	}
	k = pool_ptr - 4; undump_four_ASCII; init_str_ptr = str_ptr; init_pool_ptr = pool_ptr;
	max_str_ptr = str_ptr; max_pool_ptr = pool_ptr;
	#pragma endregion


	#pragma region <Undump the dynamic memory 1195>
	undump(lo_mem_stat_max + 1000, hi_mem_stat_min - 1, lo_mem_max);
	undump(lo_mem_stat_max + 1, lo_mem_max, rover); p = mem_min; q = rover;
	do {
		for (k = p; k <= q + 1; k++) undump_wd(&mem[k]);
		p = q + node_size(q);
		if (p > lo_mem_max || (q >= rlink(q) && rlink(q) != rover)) goto off_base;
		q = rlink(q);
	} while (!(q == rover));
	for (k = p; k <= lo_mem_max; k++) undump_wd(&mem[k]);
	undump(lo_mem_max + 1, hi_mem_stat_min, hi_mem_min); undump(null, mem_top, avail);
	mem_end = mem_top;
	for (k = hi_mem_min; k <= mem_end; k++) undump_wd(&mem[k]);
	undump_int(&var_used); undump_int(&dyn_used);
	#pragma endregion


	#pragma region <Undump the table of equivalents and the hash table 1197>
	undump(1, frozen_inaccessible, hash_used); p = 0;
	do {
		undump(p + 1, hash_used, p); undump_hh(&hash[p]); undump_hh(&eqtb[p]);
	} while (!(p == hash_used));
	for (p = hash_used + 1; p <= hash_end; p++) {
		undump_hh(&hash[p]); undump_hh(&eqtb[p]);
	}
	undump_int(&st_count);
	#pragma endregion


	#pragma region <Undump a few more things and the closing check word 1199>
	undump(max_given_internal, max_internal, int_ptr);
	for (k = 1; k <= int_ptr; k++) {
		undump_int(&internal[k]); undump(0, str_ptr, int_name[k]);
	}
	undump(0, frozen_inaccessible, start_sym); undump(batch_mode, error_stop_mode, interaction);
	undump(0, str_ptr, base_ident); undump(1, hash_end, bg_loc); undump(1, hash_end, eg_loc);
	undump_int(&serial_no);
	undump_int(&x); if (x != 69069 || feof(base_file)) goto off_base;
	#pragma endregion


	return true;
off_base:
	wake_up_terminal(); wterm_ln_s("(Fatal base file error; I'm stymied)");
	return false;
}

// 1188
void dump_wd(memory_word memword)
{
	fwrite(&memword, sizeof memword, 1, base_file);
}

void dump_int(int n)
{
	fwrite(&n, sizeof n, 1, base_file);
}

void dump_hh(two_halves halves)
{
	fwrite(&halves, sizeof halves, 1, base_file);
}

void dump_qqqq(four_quarters fq)
{
	fwrite(&fq, sizeof fq, 1, base_file);
}

// 1189
void undump_wd(memory_word *memword)
{
	fread(memword, sizeof *memword, 1, base_file);
}

void undump_int(int *anint)
{
	fread(anint, sizeof *anint, 1, base_file);
}

void undump_hh(two_halves *hh)
{
	fread(hh, sizeof *hh, 1, base_file);
}

void undump_qqqq(four_quarters *qqqq)
{
	fread(qqqq, sizeof *qqqq, 1, base_file);
}


// 1210
#ifndef NO_INIT

void init_prim()
{
	#pragma region <Put each of METAFONTs primitives into the hash table 192>
	primitive(/*tracingtitles*/1009,internal_quantity,tracing_titles);
	primitive(/*tracingequations*/1010,internal_quantity,tracing_equations);
	primitive(/*tracingcapsules*/1011,internal_quantity,tracing_capsules);
	primitive(/*tracingchoices*/1012,internal_quantity,tracing_choices);
	primitive(/*tracingspecs*/1013,internal_quantity,tracing_specs);
	primitive(/*tracingpens*/1014,internal_quantity,tracing_pens);
	primitive(/*tracingcommands*/1015,internal_quantity,tracing_commands);
	primitive(/*tracingrestores*/1016,internal_quantity,tracing_restores);
	primitive(/*tracingmacros*/1017,internal_quantity,tracing_macros);
	primitive(/*tracingedges*/1018,internal_quantity,tracing_edges);
	primitive(/*tracingoutput*/1019,internal_quantity,tracing_output);
	primitive(/*tracingstats*/1020,internal_quantity,tracing_stats);
	primitive(/*tracingonline*/1021,internal_quantity,tracing_online);
	primitive(/*year*/1022,internal_quantity,year);
	primitive(/*month*/1023,internal_quantity,month);
	primitive(/*day*/1024,internal_quantity,day);
	primitive(/*time*/1025,internal_quantity,_time);
	primitive(/*charcode*/1026,internal_quantity,char_code);
	primitive(/*charext*/1027,internal_quantity,char_ext);
	primitive(/*charwd*/1028,internal_quantity,char_wd);
	primitive(/*charht*/1029,internal_quantity,char_ht);
	primitive(/*chardp*/1030,internal_quantity,char_dp);
	primitive(/*charic*/1031,internal_quantity,char_ic);
	primitive(/*chardx*/1032,internal_quantity,char_dx);
	primitive(/*chardy*/1033,internal_quantity,char_dy);
	primitive(/*designsize*/1034,internal_quantity,design_size);
	primitive(/*hppp*/1035,internal_quantity,hppp);
	primitive(/*vppp*/1036,internal_quantity,vppp);
	primitive(/*xoffset*/954,internal_quantity,x_offset);
	primitive(/*yoffset*/955,internal_quantity,y_offset);
	primitive(/*pausing*/1037,internal_quantity,pausing);
	primitive(/*showstopping*/1038,internal_quantity,showstopping);
	primitive(/*fontmaking*/1039,internal_quantity,fontmaking);
	primitive(/*proofing*/1040,internal_quantity,proofing);
	primitive(/*smoothing*/1041,internal_quantity,smoothing);
	primitive(/*autorounding*/1042,internal_quantity,autorounding);
	primitive(/*granularity*/1043,internal_quantity,granularity);
	primitive(/*fillin*/1044,internal_quantity,fillin);
	primitive(/*turningcheck*/1045,internal_quantity,turning_check);
	primitive(/*warningcheck*/1046,internal_quantity,warning_check);
	primitive(/*boundarychar*/1047,internal_quantity,boundary_char);

	// 211
	primitive(/*..*/416,path_join,0);
	primitive(/*[*/91,left_bracket,0); eqtb[frozen_left_bracket] = eqtb[cur_sym];
	primitive(/*]*/93,right_bracket,0);
	primitive(/*}*/125,right_brace,0);
	primitive(/*{*/123,left_brace,0);
	primitive(/*:*/58,colon,0); eqtb[frozen_colon] = eqtb[cur_sym];
	primitive(/*::*/530,double_colon,0);
	primitive(/*||:*/523,bchar_label,0);
	primitive(/*:=*/521,assignment,0);
	primitive(/*,*/44,comma,0);
	primitive(/*;*/59,semicolon,0);eqtb[frozen_semicolon] = eqtb[cur_sym];
	primitive(/*\*/92,relax,0);
	primitive(/*addto*/520,add_to_command,0);
	primitive(/*at*/522,at_token,0);
	primitive(/*atleast*/453,at_least,0);
	primitive(/*begingroup*/524,begin_group,0); bg_loc = cur_sym;
	primitive(/*controls*/525,controls,0);
	primitive(/*cull*/526,cull_command,0);
	primitive(/*curl*/527,curl_command,0);
	primitive(/*delimiters*/528,delimiters,0);
	primitive(/*display*/529,display_command,0);
	primitive(/*endgroup*/531,end_group,0); eqtb[frozen_end_group] = eqtb[cur_sym]; eg_loc = cur_sym;
	primitive(/*everyjob*/532,every_job_command,0);
	primitive(/*exitif*/533,exit_test,0);
	primitive(/*expandafter*/534,expand_after,0);
	primitive(/*from*/535,from_token,0);
	primitive(/*inwindow*/536,in_window,0);
	primitive(/*interim*/537,interim_command,0);
	primitive(/*let*/538,let_command,0);
	primitive(/*newinternal*/539,new_internal,0);
	primitive(/*of*/540,of_token,0);
	primitive(/*openwindow*/541,open_window,0);
	primitive(/*randomseed*/542,random_seed,0);
	primitive(/*save*/543,save_command,0);
	primitive(/*scantokens*/544,scan_tokens,0);
	primitive(/*shipout*/545,ship_out_command,0);
	primitive(/*skipto*/546,skip_to,0);
	primitive(/*step*/547,step_token,0);
	primitive(/*str*/548,str_op,0);
	primitive(/*tension*/549,tension,0);
	primitive(/*to*/550,to_token,0);
	primitive(/*until*/551,until_token,0);

	// 683
	primitive(/*def*/552,macro_def ,start_def);
	primitive(/*vardef*/554,macro_def ,var_def);
	primitive(/*primarydef*/555,macro_def ,secondary_primary_macro);
	primitive(/*secondarydef*/556,macro_def ,tertiary_secondary_macro);
	primitive(/*tertiarydef*/557,macro_def ,expression_tertiary_macro);
	primitive(/*enddef*/553,macro_def ,end_def); eqtb[frozen_end_def] = eqtb[cur_sym];
	primitive(/*for*/560,iteration,expr_base);
	primitive(/*forsuffixes*/561,iteration,suffix_base);
	primitive(/*forever*/558,iteration,start_forever);
	primitive(/*endfor*/559,iteration,end_for); eqtb[frozen_end_for] = eqtb[cur_sym];

	// 688
	primitive(/*quote*/564, macro_special, quote);
	primitive(/*#@*/562, macro_special, macro_prefix);
	primitive(/*@*/64, macro_special, macro_at);
	primitive(/*@#*/563, macro_special, macro_suffix);

	// 695
	primitive(/*expr*/565, param_type, expr_base);
	primitive(/*suffix*/566, param_type, suffix_base);
	primitive(/*text*/567, param_type, text_base);
	primitive(/*primary*/568, param_type, primary_macro);
	primitive(/*secondary*/569, param_type, secondary_macro);
	primitive(/*tertiary*/570, param_type, tertiary_macro);

	// 709
	primitive(/*input*/571, input, 0);
	primitive(/*endinput*/572, input, 1);

	// 740
	primitive(/*if*/573, if_test, if_code);
	primitive(/*fi*/574, fi_or_else, fi_code); eqtb[frozen_fi] = eqtb[cur_sym];
	primitive(/*else*/575, fi_or_else, else_code);
	primitive(/*elseif*/576, fi_or_else, else_if_code);

	// 893

	primitive(/*true*/355,nullary,true_code);
	primitive(/*false*/356,nullary,false_code);
	primitive(/*nullpicture*/357,nullary,null_picture_code);
	primitive(/*nullpen*/358,nullary,null_pen_code);
	primitive(/*jobname*/359,nullary,job_name_op);
	primitive(/*readstring*/360,nullary,read_string_op);
	primitive(/*pencircle*/361,nullary,pen_circle);
	primitive(/*normaldeviate*/362,nullary,normal_deviate);
	primitive(/*odd*/363,unary,odd_op);
	primitive(/*known*/364,unary,known_op);
	primitive(/*unknown*/365,unary,unknown_op);
	primitive(/*not*/366,unary,not_op);
	primitive(/*decimal*/367,unary,decimal);
	primitive(/*reverse*/368,unary,reverse);
	primitive(/*makepath*/369,unary,make_path_op);
	primitive(/*makepen*/370,unary,make_pen_op);
	primitive(/*totalweight*/371,unary,total_weight_op);
	primitive(/*oct*/372,unary,oct_op);
	primitive(/*hex*/373,unary,hex_op);
	primitive(/*ASCII*/374,unary,ASCII_op);
	primitive(/*char*/375,unary,char_op);
	primitive(/*length*/376,unary,length_op);
	primitive(/*turningnumber*/377,unary,turning_op);
	primitive(/*xpart*/378,unary,x_part);
	primitive(/*ypart*/379,unary,y_part);
	primitive(/*xxpart*/380,unary,xx_part);
	primitive(/*xypart*/381,unary,xy_part);
	primitive(/*yxpart*/382,unary,yx_part);
	primitive(/*yypart*/383,unary,yy_part);
	primitive(/*sqrt*/384,unary,sqrt_op);
	primitive(/*mexp*/385,unary,m_exp_op);
	primitive(/*mlog*/386,unary,m_log_op);
	primitive(/*sind*/387,unary,sin_d_op);
	primitive(/*cosd*/388,unary,cos_d_op);
	primitive(/*floor*/389,unary,floor_op);
	primitive(/*uniformdeviate*/390,unary,uniform_deviate);
	primitive(/*charexists*/391,unary,char_exists_op);
	primitive(/*angle*/392,unary,angle_op);
	primitive(/*cycle*/393,cycle,cycle_op);
	primitive(/*+*/43,plus_or_minus,plus);
	primitive(/*-*/45,plus_or_minus,minus);
	primitive(/***/42,secondary_binary,times);
	primitive(/*/*/47,slash,over);eqtb[frozen_slash]=eqtb[cur_sym];
	primitive(/*++*/394,tertiary_binary,pythag_add);
	primitive(/*+-+*/315,tertiary_binary,pythag_sub);
	primitive(/*and*/396,and_command,and_op);
	primitive(/*or*/395,tertiary_binary,or_op);
	primitive(/*<*/60,expression_binary,less_than);
	primitive(/*<=*/397,expression_binary,less_or_equal);
	primitive(/*>*/62,expression_binary,greater_than);
	primitive(/*>=*/398,expression_binary,greater_or_equal);
	primitive(/*=*/61,equals,equal_to);
	primitive(/*<>*/399,expression_binary,unequal_to);
	primitive(/*substring*/409,primary_binary,substring_of);
	primitive(/*subpath*/410,primary_binary,subpath_of);
	primitive(/*directiontime*/411,primary_binary,direction_time_of);
	primitive(/*point*/412,primary_binary,point_of);
	primitive(/*precontrol*/413,primary_binary,precontrol_of);
	primitive(/*postcontrol*/414,primary_binary,postcontrol_of);
	primitive(/*penoffset*/415,primary_binary,pen_offset_of);
	primitive(/*&*/38,ampersand,concatenate);
	primitive(/*rotated*/400,secondary_binary,rotated_by);
	primitive(/*slanted*/401,secondary_binary,slanted_by);
	primitive(/*scaled*/402,secondary_binary,scaled_by);
	primitive(/*shifted*/403,secondary_binary,shifted_by);
	primitive(/*transformed*/404,secondary_binary,transformed_by);
	primitive(/*xscaled*/405,secondary_binary,x_scaled);
	primitive(/*yscaled*/406,secondary_binary,y_scaled);
	primitive(/*zscaled*/407,secondary_binary,z_scaled);
	primitive(/*intersectiontimes*/408,tertiary_binary,intersect);

	// 1013
	primitive(/*numeric*/348, type_name, numeric_type);
	primitive(/*string*/334, type_name, string_type);
	primitive(/*boolean*/332, type_name, boolean_type);
	primitive(/*path*/339, type_name, path_type);
	primitive(/*pen*/336, type_name, pen_type);
	primitive(/*picture*/341, type_name, picture_type);
	primitive(/*transform*/343, type_name, transform_type);
	primitive(/*pair*/344, type_name, pair_type);

	// 1018
	primitive(/*end*/577, stop, 0);
	primitive(/*dump*/578, stop, 1);

	// 1024
	primitive(/*batchmode*/278, mode_command, batch_mode);
	primitive(/*nonstopmode*/279, mode_command, nonstop_mode);
	primitive(/*scrollmode*/280, mode_command, scroll_mode);
	primitive(/*errorstopmode*/579, mode_command, error_stop_mode);

	// 1027
	primitive(/*inner*/580, protection_command, 0);
	primitive(/*outer*/581, protection_command, 1);

	// 1037
	primitive(/*showtoken*/582, show_command, show_token_code);
	primitive(/*showstats*/583, show_command, show_stats_code);
	primitive(/*show*/584, show_command, show_code);
	primitive(/*showvariable*/585, show_command, show_var_code);
	primitive(/*showdependencies*/586, show_command, show_dependencies_code);

	// 1052
	primitive(/*contour*/595, thing_to_add, contour_code);
	primitive(/*doublepath*/596, thing_to_add, double_path_code);
	primitive(/*also*/597, thing_to_add, also_code);
	primitive(/*withpen*/598, with_option, pen_type);
	primitive(/*withweight*/599, with_option, known);
	primitive(/*dropping*/600, cull_op, drop_code);
	primitive(/*keeping*/601, cull_op, keep_code);

	// 1079
	primitive(/*message*/602, message_command, message_code);
	primitive(/*errmessage*/603, message_command, err_message_code);
	primitive(/*errhelp*/604, message_command, err_help_code);

	// 1101
	primitive(/*charlist*/605, tfm_command, char_list_code);
	primitive(/*ligtable*/606, tfm_command, lig_table_code);
	primitive(/*extensible*/607, tfm_command, extensible_code);
	primitive(/*headerbyte*/608, tfm_command, header_byte_code);
	primitive(/*fontdimen*/609, tfm_command, font_dimen_code);

	// 1108
	primitive(/*=:*/610, lig_kern_token, 0); primitive(/*=:|*/611, lig_kern_token, 1);
	primitive(/*=:|>*/614, lig_kern_token, 5); primitive(/*|=:*/612, lig_kern_token, 2);
	primitive(/*|=:>*/615, lig_kern_token, 6); primitive(/*|=:|*/613, lig_kern_token, 3);
	primitive(/*|=:|>*/616, lig_kern_token, 7); primitive(/*|=:|>>*/617, lig_kern_token, 11);
	primitive(/*kern*/618, lig_kern_token, 128);

	// 1176
	primitive(/*special*/620, special_command, string_type);
	primitive(/*numspecial*/619, special_command, known);


	#pragma endregion
}

void init_tab()
{
	int k; // all-purpose index
	#pragma region <Initialize table entries (done by INIMF only) 176>
	rover = lo_mem_stat_max + 1; // initialize the dynamic memory
	link(rover) = empty_flag; node_size(rover) = 1000; // which is a 1000-word available node
	llink(rover) = rover; rlink(rover) = rover;
	lo_mem_max = rover + 1000; link(lo_mem_max) = null; info(lo_mem_max) = null;
	for (k = hi_mem_stat_min; k <= mem_top; k++) mem[k] = mem[lo_mem_max]; // clear list heads
	avail = null; mem_end = mem_top; hi_mem_min = hi_mem_stat_min; // initialize the one-word memory
	var_used = lo_mem_stat_max + 1 - mem_min; dyn_used = mem_top + 1 - hi_mem_min; // initialize statistics

	// 193
	int_name[tracing_titles] = /*tracingtitles*/1009;
	int_name[tracing_equations] = /*tracingequations*/1010;
	int_name[tracing_capsules] = /*tracingcapsules*/1011;
	int_name[tracing_choices] = /*tracingchoices*/1012;
	int_name[tracing_specs] = /*tracingspecs*/1013;
	int_name[tracing_pens] = /*tracingpens*/1014;
	int_name[tracing_commands] = /*tracingcommands*/1015;
	int_name[tracing_restores] = /*tracingrestores*/1016;
	int_name[tracing_macros] = /*tracingmacros*/1017;
	int_name[tracing_edges] = /*tracingedges*/1018;
	int_name[tracing_output] = /*tracingoutput*/1019;
	int_name[tracing_stats] = /*tracingstats*/1020;
	int_name[tracing_online] = /*tracingonline*/1021;
	int_name[year] = /*year*/1022;
	int_name[month] = /*month*/1023;
	int_name[day] = /*day*/1024;
	int_name[_time] = /*time*/1025;
	int_name[char_code] = /*charcode*/1026;
	int_name[char_ext] = /*charext*/1027;
	int_name[char_wd] = /*charwd*/1028;
	int_name[char_ht] = /*charht*/1029;
	int_name[char_dp] = /*chardp*/1030;
	int_name[char_ic] = /*charic*/1031;
	int_name[char_dx] = /*chardx*/1032;
	int_name[char_dy] = /*chardy*/1033;
	int_name[design_size] = /*designsize*/1034;
	int_name[hppp] = /*hppp*/1035;
	int_name[vppp] = /*vppp*/1036;
	int_name[x_offset] = /*xoffset*/954;
	int_name[y_offset] = /*yoffset*/955;
	int_name[pausing] = /*pausing*/1037;
	int_name[showstopping] = /*showstopping*/1038;
	int_name[fontmaking] = /*fontmaking*/1039;
	int_name[proofing] = /*proofing*/1040;
	int_name[smoothing] = /*smoothing*/1041;
	int_name[autorounding] = /*autorounding*/1042;
	int_name[granularity] = /*granularity*/1043;
	int_name[fillin] = /*fillin*/1044;
	int_name[turning_check] = /*turningcheck*/1045;
	int_name[warning_check] = /*warningcheck*/1046;
	int_name[boundary_char] = /*boundarychar*/1047;


	// 203
	hash_used = frozen_inaccessible; // nothing is used
	st_count = 0;
	text(frozen_bad_vardef) = /*a bad variable*/1048; text(frozen_fi) = /*fi*/574;
	text(frozen_end_group) = /*endgroup*/531; text(frozen_end_def) = /*enddef*/553;
	text(frozen_end_for) = /*endfor*/559;
	text(frozen_semicolon) = /*;*/59; text(frozen_colon) = /*:*/58; text(frozen_slash) = /*/*/47;
	text(frozen_left_bracket) = /*[*/91; text(frozen_right_delimiter) = /*)*/41;
	text(frozen_inaccessible) = /* INACCESSIBLE*/1049;
	eq_type(frozen_right_delimiter) = right_delimiter;

	// 229
	attr_loc(end_attr) = hash_end + 1; parent(end_attr) = null;

	// 324
	info(sentinel) = max_halfword; // link(sentinel) = null

	// 475
	ref_count(null_pen) = null; link(null_pen) = null;
	info(null_pen + 1) = 1; link(null_pen + 1) = null_coords;
	for (k = null_pen + 2; k <= null_pen + 8; k++) mem[k] = mem[null_pen + 1];
	max_offset(null_pen) = 0;
	link(null_coords) = null_coords; knil(null_coords) = null_coords;
	x_coord(null_coords) = 0; y_coord(null_coords) = 0;

	// 587
	serial_no = 0; link(dep_head) = dep_head; prev_dep(dep_head) = dep_head; info(dep_head) = null;
	dep_list(dep_head) = null;

	// 702
	name_type(bad_vardef) = root; link(bad_vardef) = frozen_bad_vardef;
	equiv(frozen_bad_vardef) = bad_vardef; eq_type(frozen_bad_vardef) = tag_token;

	// 759
	eq_type(frozen_repeat_loop) = repeat_loop + outer_tag; text(frozen_repeat_loop) = /* ENDFOR*/1050;

	// 911
	name_type(temp_val) = capsule;

	// 1116
	value(inf_val) = fraction_four;

	// 1127
	value(zero_val) = 0; info(zero_val) = 0;

	// 1185
	base_ident = /* (INIMF)*/1051;




	#pragma endregion

}
#endif


// 1205
void close_files_and_terminate()
{
	int k; // all-purpose index
	int lh; // the length of the TFM header, in words
	int lk_offset; // 0..256, extra words inserted at the beginning of lig_kern array
	pointer p; // runs throuhg a list of TFM dimensions
	scaled x; // a tfm_width value being output to the GF file

#ifndef NO_STAT
	if (internal[tracing_stats] > 0)
		#pragma region <Output statistics about this job 1208>
	{
		if (log_opened) {
			wlog_ln_s(" "); wlog_ln_s("Here is how much of METAFONT's memory you used:");
			fprintf(log_file, " %d string", max_str_ptr - init_str_ptr);
			if (max_str_ptr != init_str_ptr + 1) wlog_s("s");
			fprintf(log_file, " out of %d\n", max_strings - init_str_ptr);
			fprintf(log_file, " %d string characters out of %d\n", max_pool_ptr - init_pool_ptr, pool_size - init_pool_ptr);
			fprintf(log_file, " %d words of memory out of %d\n", lo_mem_max - mem_min + mem_end - hi_mem_min + 2,
				mem_end + 1 - mem_min);
			fprintf(log_file, " %d symbolic tokens out of %d\n", st_count, hash_size);
			fprintf(log_file, " %di,%dn,%dr,%dp,%db stack positions out of %di,%dn,%dr,%dp,%db\n",
				max_in_stack, int_ptr, max_rounding_ptr, max_param_stack, max_buf_stack + 1, 
				stack_size, max_internal, max_wiggle, param_size, buf_size);
		}
	}
		#pragma endregion
#endif

	wake_up_terminal();

	#pragma region <Finish the TFM and GF files 1206>

	if (gf_prev_ptr > 0 || internal[fontmaking] > 0) {

		#pragma region <Make the dynamic memory into one big available node 1207>
		rover = lo_mem_stat_max + 1; link(rover)  = empty_flag; lo_mem_max = hi_mem_min - 1;
		if (lo_mem_max - rover > max_halfword) lo_mem_max = max_halfword + rover;
		node_size(rover) = lo_mem_max - rover; llink(rover) = rover; rlink(rover) = rover;
		link(lo_mem_max) = null; info(lo_mem_max) = null;
		#pragma endregion

		#pragma region <Massage the TFM widths 1124>
		clear_the_list;
		for (k = bc; k <= ec; k++)
			if (char_exists[k]) tfm_width[k] = sort_in(tfm_width[k]);
		nw = skimp(255) + 1; dimen_head[1] = link(temp_head);
		if (perturbation >= 010000) tfm_warning(char_wd);
		#pragma endregion

		fix_design_size(); fix_check_sum();
		if (internal[fontmaking] > 0) {

			#pragma region <Massage the TFM heights, depths, and italic corrections 1126>
			clear_the_list;
			for (k = bc; k <= ec; k++)
				if (char_exists[k])
					if (tfm_height[k] == 0) tfm_height[k] = zero_val;
					else tfm_height[k] = sort_in(tfm_height[k]);
			nh = skimp(15) + 1; dimen_head[2] = link(temp_head);
			if (perturbation >= 010000) tfm_warning(char_ht);
			clear_the_list;
			for (k = bc; k <= ec; k++)
				if (char_exists[k])
					if (tfm_depth[k] == 0) tfm_depth[k] = zero_val;
					else tfm_depth[k] = sort_in(tfm_depth[k]);
			nd = skimp(15) + 1; dimen_head[3] = link(temp_head);
			if (perturbation >= 010000) tfm_warning(char_dp);
			clear_the_list;
			for (k = bc; k <= ec; k++)
				if (char_exists[k])
					if (tfm_ital_corr[k] == 0) tfm_ital_corr[k] = zero_val;
					else tfm_ital_corr[k] = sort_in(tfm_ital_corr[k]);
			ni = skimp(63) + 1; dimen_head[4] = link(temp_head);
			if (perturbation >= 010000) tfm_warning(char_ic);
			#pragma endregion


			internal[fontmaking] = 0; // avoid loop in case of fatal error

			#pragma region <Finish the TFM file 1134>
			if (job_name == 0) open_log_file();
			pack_job_name(/*.tfm*/1052);
			while (!b_open_out(&tfm_file)) prompt_file_name(/*file name for font metrics*/1053, /*.tfm*/1052);
			metric_file_name = b_make_name_string(tfm_file);

			#pragma region <Output the subfile sizes and header bytes 1135>
			k = header_size;
			while (header_byte[k] < 0) k--;
			lh = (k + 3) / 4; // this is the number of header words
			if (bc > ec) bc = 1; // if there are no characters, ec = 0 and bc = 1

			#pragma region <Compute the ligature/kern program offset and implant the left boundary label 1137>
			bchar = round_unscaled(internal[boundary_char]);
			if (bchar < 0 || bchar > 255) {
				bchar = -1; lk_started = false; lk_offset = 0;
			}
			else {
				lk_started = true; lk_offset = 1;
			}

			#pragma region <Find the minimum lk_offset and adjust all remainders 1138>
			k = label_ptr; // pointer to the largest unallocated label
			if (label_loc[k] + lk_offset > 255) {
				lk_offset = 0; lk_started = false; // location 0 can do double duty
				do {
					char_remainder[label_char[k]] = lk_offset;

					while (label_loc[k - 1] == label_loc[k]) {
						k--; char_remainder[label_char[k]] = lk_offset;
					}
					lk_offset++; k--;

				} while (!(lk_offset + label_loc[k] < 256)); // N.B.: lk_offset = 256 satisfies this when k = 0


			}
			if (lk_offset > 0)
				while(k > 0) {
					char_remainder[label_char[k]] = char_remainder[label_char[k]] + lk_offset; k--;
				}

			#pragma endregion

			if (bch_label < undefined_label) {
				skip_byte(nl) = qi(255); next_char(nl) = qi(0);
				op_byte(nl) = qi(((bch_label + lk_offset) / 256));
				rem_byte(nl) = qi(((bch_label + lk_offset) % 256)); nl++; // possibly nl = lig_table_size + 1
			}


			#pragma endregion

			tfm_two(6 + lh + (ec - bc + 1) + nw + nh + nd + ni + nl + lk_offset + nk + ne + np);
			// this is the total number of file words that will be output
			tfm_two(lh); tfm_two(bc); tfm_two(ec); tfm_two(nw); tfm_two(nh); tfm_two(nd); tfm_two(ni);
			tfm_two(nl + lk_offset); tfm_two(nk); tfm_two(ne); tfm_two(np);
			for (k = 1; k <= 4 * lh; k++) {
				if (header_byte[k] < 0) header_byte[k] = 0;
				tfm_out(header_byte[k]);
			}



			#pragma endregion

			#pragma region <Output the character information bytes, then output the dimensions themselves 1136>
			for (k = bc; k <= ec; k++)
				if (!char_exists[k]) tfm_four(0);
				else {
					tfm_out(info(tfm_width[k])); // the width index
					tfm_out((info(tfm_height[k]))*16 + info(tfm_depth[k]));
					tfm_out((info(tfm_ital_corr[k]))*4 + char_tag[k]); tfm_out(char_remainder[k]);
				}
			tfm_changed = 0;
			for (k = 1; k <= 4; k++) {
				tfm_four(0); p = dimen_head[k];
				while(p != inf_val) {
					tfm_four(dimen_out(value(p))); p = link(p);
				}
			}
			#pragma endregion


			#pragma region <Output the ligature/kern program 1139>
			for (k = 0; k <= 255; k++)
				if (skip_table[k] < undefined_label) {
					print_nl(/*(local label */1054); print_int(k); print(/*:: was missing)*/1055);
					cancel_skips(skip_table[k]);
				}
			if (lk_started) {// lk_offset = 1 for special bchar
				tfm_out(255); tfm_out(bchar); tfm_two(0);
			}
			else for (k = 1; k <= lk_offset; k++) { // output teh redirection specs
				ll = label_loc[label_ptr];
				if (bchar < 0) {
					tfm_out(254); tfm_out(0);
				}
				else {
					tfm_out(255); tfm_out(bchar);
				}
				tfm_two(ll + lk_offset);
				do {
					label_ptr--;
				} while (!(label_loc[label_ptr] < ll));
			}
			for (k = 0; k <= nl - 1; k++) tfm_qqqq(lig_kern[k]);
			for (k = 0; k <= nk - 1; k++) tfm_four(dimen_out(kern[k]));
			#pragma endregion


			#pragma region <Output the extensible character recipes and the font metric parameters 1140>
			for (k = 0; k <= ne -1; k++) tfm_qqqq(exten[k]);
			for (k = 1; k <= np; k++)
				if (k == 1)
					if (myabs(param[1]) < fraction_half) tfm_four(param[1]*16);
					else {
						tfm_changed++;
						if (param[1] > 0) tfm_four(el_gordo);
						else tfm_four(-el_gordo);
					}
				else tfm_four(dimen_out(param[k]));
			if (tfm_changed > 0) {
				if (tfm_changed == 1) print_nl(/*(a font metric dimension*/1056);
				else {
					print_nl(/*(*/40); print_int(tfm_changed); print(/* font metric dimensions*/1057);
				}
				print(/* had to be decreased)*/1058);
			}
			#pragma endregion

#ifndef NO_STAT
			if (internal[tracing_stats] > 0)
				#pragma region <Log the subfile sizes of the TFM file 1141>
			{
				wlog_ln_s(" ");
				if(bch_label < undefined_label) nl--;
				fprintf(log_file, "(You used %dw,%dh,%dd,%di,%dl,%dk,%de,%dp metric file positions\n",
									nw, nh, nd, ni, nl, nk, ne, np);
				fprintf(log_file, "  out of 256w,16h,16d,64i,%dl,%dk,256e,%dp)",
								lig_table_size, max_kerns, max_font_dimen);
			}
				#pragma endregion
#endif
			
			print_nl(/*Font metrics written on */1059); slow_print(metric_file_name); print_char(/*.*/46);
			b_close(tfm_file);

			#pragma endregion


		}
		if (gf_prev_ptr > 0)
			#pragma region <Finish the GF file 1182>
		{
			gf_out(post); // beginning of the postamble
			gf_four(gf_prev_ptr); gf_prev_ptr = gf_offset + gf_ptr - 5; // post location
			gf_four(internal[design_size] * 16);
			for (k = 1; k <= 4; k++) gf_out(header_byte[k]); // the check sum
			gf_four(internal[hppp]); gf_four(internal[vppp]);
			gf_four(gf_min_m); gf_four(gf_max_m); gf_four(gf_min_n); gf_four(gf_max_n);

			for (k = 0; k <= 255; k++)
				if (char_exists[k]) {
					x = gf_dx[k] / unity;
					if (gf_dy[k] == 0 && x >= 0 && x < 256 && gf_dx[k] == x * unity) {
						gf_out(char_loc + 1); gf_out(k); gf_out(x);
					}
					else {
						gf_out(char_loc); gf_out(k); gf_four(gf_dx[k]); gf_four(gf_dy[k]);
					}
					x = value(tfm_width[k]);
					if (myabs(x) > max_tfm_dimen)
						if (x > 0) x = three_bytes - 1; else x = 1 - three_bytes;
					else x = make_scaled(x * 16, internal[design_size]);
					gf_four(x); gf_four(char_ptr[k]);
				
				}
			gf_out(post_post); gf_four(gf_prev_ptr); gf_out(gf_id_byte);
			k = 4 + ((gf_buf_size - gf_ptr) % 4); // the number of 223's
			while (k > 0) {
				gf_out(223); k--;
			}

			#pragma region <Empty the last bytes out of gf_buf 1156>
			if (gf_limit == half_buf) write_gf(half_buf, gf_buf_size - 1);
			if (gf_ptr > 0) write_gf(0, gf_ptr - 1);
			#pragma endregion

			print_nl(/*Output written on */1060); slow_print(output_file_name); print(/* (*/479); print_int(total_chars);
			print(/* character*/1061);
			if (total_chars != 1) print_char(/*s*/115);
			print(/*, */1062); print_int(gf_offset + gf_ptr); print(/* bytes).*/1063); b_close(gf_file);

		}
			#pragma endregion


	}

	#pragma endregion
	if (log_opened) {
		wlog_cr; a_close(log_file); selector = selector - 2;
		if (selector == term_only) {
			print_nl(/*Transcript written on */1064); slow_print(log_name); print_char(/*.*/46);
		}
	}
	print_ln();
	if (ed_name_start!=0 && interaction>batch_mode)
		call_edit(&str_pool[ed_name_start], ed_name_length, edit_line);
}

#pragma warning(push)
#pragma warning(disable:4702)
// 1209
void final_cleanup()
{
	small_number c; // 0 for end, 1 for dump
	c = cur_mod;
	if (job_name == 0) open_log_file();
	while (input_ptr > 0)
		if (token_state) end_token_list(); else end_file_reading();
	while (loop_ptr != null) stop_iteration();
	while (open_parens > 0) {
		print(/* )*/1065); open_parens--;
	}
	while (cond_ptr != null) {
		print_nl(/*(end occurred when */1066);
		print_cmd_mod(fi_or_else, cur_if); // if or elseif or else
		if (if_line != 0) {
			print(/* on line */1067); print_int(if_line);
		}
		print(/* was incomplete)*/1068); if_line = if_line_field(cond_ptr); cur_if = name_type(cond_ptr);
		loop_ptr = cond_ptr; cond_ptr = link(cond_ptr); free_node(loop_ptr, if_node_size);

	}
	if (history != spotless)
		if (history == warning_issued || interaction < error_stop_mode)
			if (selector == term_and_log) {
				selector = term_only;
				print_nl(/*(see the transcript file for additional information)*/1069);
				selector = term_and_log;

			}
	if (c == 1) {
#ifndef NO_INIT
		store_base_file(); return;
#endif
		print_nl(/*(dump is performed only by INIMF)*/1070); return;
	}
}
#pragma warning(pop)


// 1212
#ifndef NO_DEBUG
void debug_help() //{routine to display various things}
{

	int k, l, m, n;
	while (true)
	{
		wake_up_terminal(); print_nl(/*debug # (-1 to exit):*/1071); update_terminal(); std::cin >> m; //read(term_in, m);
		if (m < 0) return;
		else if (m == 0)
		{
			/* NOTE: This was used to break into Knuth's Pascal debugger
			goto breakpoint; @\ {go to every label at least once}
			breakpoint: m = 0; @{'BREAKPOINT'@}@\
			*/
			// Instead print something and continue
			fputs("Option 0 not implemented, sorry.", term_out);
			continue;
		}
		else {
			std::cin >> n;
			switch (m)
			{
				#pragma region <Numbered cases for debug help 1339>
				case 1: print_word(mem[n]); break;// {display mem[n] in all forms}
				case 2: print_int(info(n)); break;
				case 3: print_int(link(n)); break;
				case 4: print_int(eq_type(n)); print_char(/*:*/58); print_int(equiv(n)); break;
				case 5: print_variable_name(n); break;
				case 6: print_int(internal[n]); break;
				case 7: do_show_dependencies(); break;
				case 9: show_token_list(n, null, 100000, 0); break;
				case 10: slow_print(n); break;
				case 11: check_mem(n > 0);  break; // {check wellformedness; print new busy locations if n > 0}
				case 12: search_mem(n);  break; // {look for pointers to n}
				case 13: std::cin >> l;   /*read(term_in, l);*/ print_cmd_mod(n, l);  break;
				case 14: for (k = 0; k <= n; k++) print(buffer[k]);  break;
				case 15: panicking = !panicking;  break;
				#pragma endregion
				default: print(/*?*/63); break;
			}
		}
	}

}
#endif


int main(int argc, char**argv)
{
	history = fatal_error_stop;
	//t_open_out();

	// set environment variables
	set_paths();

#pragma warning(push)
#pragma warning(disable:4127)
	#pragma region <Check the constant values for consistency 14>
	bad = 0;
	if (half_error_line < 30 || half_error_line > error_line - 15) bad = 1;
	if (max_print_line < 60) bad = 2;
	if (gf_buf_size % 8 != 0) bad = 3;
	if (mem_min + 1100 > mem_top) bad = 4;
	if (hash_prime > hash_size) bad = 5;
	if (header_size % 4 != 0) bad = 6;
	if (lig_table_size < 255 || lig_table_size > 32510) bad = 7;
	

	// 154
#ifndef NO_INIT
	if (mem_max != mem_top) bad = 10;
#endif
	if (mem_max < mem_top) bad = 10;
	if (min_quarterword > 0 || max_quarterword < 127) bad = 11;
	if (min_halfword > 0 || max_halfword < 32767) bad = 12;
	if (min_quarterword < min_halfword || max_quarterword > max_halfword) bad = 13;
	if (mem_min < min_halfword || mem_max >= max_halfword) bad = 14;
	if (max_strings > max_halfword) bad = 15;
	if (buf_size > max_halfword) bad = 16;
	if (max_quarterword - min_quarterword < 255 || max_halfword - min_halfword < 65535)
		bad = 17;

	// 204
	if (hash_end + max_internal > max_halfword) bad = 21;
	if (text_base + param_size > max_halfword) bad = 22;

	// 310
	if (15 * move_increment > bistack_size) bad = 31;


	// 553
	if (int_packets + 17 * int_increment > bistack_size) bad = 32;

	// 777
	if (strlen(MF_base_default) + 1 > file_name_size) bad = 41;


	#pragma endregion
#pragma warning(pop)



	if (bad > 0) {
		fprintf(term_out, "Ouch---my internal constants have been clobbered!\n---case %d\n", bad);
		goto final_end;
	}
	initialize();
#ifndef NO_INIT
	if (!get_strings_started()) goto final_end;
	init_tab();
	init_prim();
	init_str_ptr = str_ptr; init_pool_ptr = pool_ptr;
	max_str_ptr = str_ptr; max_pool_ptr = pool_ptr; fix_date_and_time();
#endif
//start_of_MF:
	#pragma region <Initialize the output routines 55>
	selector = term_only; tally = 0; term_offset = 0; file_offset = 0;
	
	
	/////////////////////////////////////////////////////////////////
	// System dependent loading of format file specified on command line

	char basename[257];
	basename[0] = 0;

	if (argc > 1) {
		// check for base=
		if (argv[1][0] == '-') {
			if (strlen(argv[1] + 1) > 6 && strncmp(argv[1] + 1, "base=", 5) == 0)
				strcpy(basename, argv[1] + 6);
		}
	}

	if (basename[0] != 0) {
		size_t flen = strlen(basename);
		if (flen > 5 && strcmp(basename + flen - 5, ".base") == 0)
			;
		else {
			if (flen + 5 < sizeof basename - 1)
				strcat(basename, ".base");
		}

		strcpy(name_of_file.get_c_str(), basename);
		bool base_loaded = false;
		if (w_open_in(&base_file)) {
			base_loaded = load_base_file();
			w_close(base_file);
		}
		if (!base_loaded) {
			printf("! Error loading base file %s\n", basename);
			goto final_end;
		}
	}

	/////////////////////////////////////////////////////////////////
	
	
	// 61
	wterm_s(banner);
	if (base_ident == 0) wterm_ln_s(" (no base preloaded)");
	else {
		slow_print(base_ident); print_ln();
	}
	update_terminal();

	// 783
	job_name = 0; log_opened = false;

	// 792
	output_file_name = 0;
	#pragma endregion

	#pragma region <Get the first line of input and prepare to start 1211>


	#pragma region <Initialize the input routines 657>
	input_ptr = 0; max_in_stack = 0; in_open = 0; open_parens = 0; max_buf_stack = 0;
	param_ptr = 0; max_param_stack = 0; first = 1; start = 1; index = 0; line = 0; name = 0;
	force_eof = false;

	if (!init_terminal(argc, argv)) goto final_end;
	limit = last; first = last + 1; // init_terminal has set loc and last

	// 660
	scanner_status = normal;

	#pragma endregion

	if (base_ident == 0 || buffer[loc] == /*&*/38) {
		if (base_ident != 0) initialize(); // erase preloaded base
		if (!open_base_file()) goto final_end;
		if (!load_base_file()) {
			w_close(base_file); goto final_end;
		}
		w_close(base_file);
		while (loc < limit && buffer[loc] == /* */32) loc++;
	}
	buffer[limit] = /*%*/37;
	fix_date_and_time(); init_randoms((internal[_time] / unity) + internal[day]);

	#pragma region <Initialize the print selector based on interaction 70>
	if (interaction == batch_mode) selector = no_print; else selector = term_only;
	#pragma endregion

	if (loc < limit)
		if (buffer[loc] != /*\*/92) start_input(); // input assumed



	#pragma endregion

	history = spotless;
	if (start_sym > 0) {
		cur_sym = start_sym;
		back_input();
	}

	main_control();
	final_cleanup();
//end_of_MF:
	close_files_and_terminate();
final_end:
	return 0;
}
