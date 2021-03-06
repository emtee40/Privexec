////
#define NOMINMAX
#include "Precompiled.h"
#include "resource.h"
#include <Shlobj.h>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
/// include json.hpp
#include "../Privexec.Core/Privexec.Core.hpp"
#include "../inc/version.h"
#include "json.hpp"

inline std::wstring utf8towide(std::string_view str) {
  std::wstring wstr;
  auto N =
      MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
  if (N > 0) {
    wstr.resize(N);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], N);
  }
  return wstr;
}

class DotComInitialize {
public:
  DotComInitialize() {
    if (FAILED(CoInitialize(NULL))) {
      throw std::runtime_error("CoInitialize failed");
    }
  }
  ~DotComInitialize() { CoUninitialize(); }
};

HINSTANCE g_hInst = nullptr;

std::vector<std::pair<int, const wchar_t *>> users;
std::unordered_map<std::wstring, std::wstring> Aliascmd;

bool InitializeCombobox(HWND hCombox) {
  int usersindex = 0;
  users.push_back(std::make_pair(kAppContainer, L"App Container"));
  users.push_back(std::make_pair(kMandatoryIntegrityControl,
                                 L"Mandatory Integrity Control"));
  users.push_back(std::make_pair(kUACElevated, L"UAC Elevated"));
  users.push_back(std::make_pair(kAdministrator, L"Administrator"));
  usersindex = (int)(users.size() - 1);
  if (IsUserAdministratorsGroup()) {
    users.push_back(std::make_pair(kSystem, L"System"));
    users.push_back(std::make_pair(kTrustedInstaller, L"TrustedInstaller"));
  }
  for (const auto &i : users) {
    ::SendMessageW(hCombox, CB_ADDSTRING, 0, (LPARAM)i.second);
  }
  ::SendMessageW(hCombox, CB_SETCURSEL, (WPARAM)usersindex, 0);
  return true;
}

bool Execute(int cur, const std::wstring &cmdline) {
  auto iter = Aliascmd.find(cmdline);

  std::wstring xcmd(PATHCCH_MAX_CCH, L'\0');
  DWORD N = 0;
  if (iter != Aliascmd.end()) {
    N = ExpandEnvironmentStringsW(iter->second.data(), &xcmd[0],
                                  PATHCCH_MAX_CCH);
  } else {
    N = ExpandEnvironmentStringsW(cmdline.data(), &xcmd[0], PATHCCH_MAX_CCH);
  }

  xcmd.resize(N - 1);
  if (cur >= users.size()) {
    return false;
  }
  DWORD dwProcessId;
  return PrivCreateProcess(users[cur].first, &xcmd[0], dwProcessId);
}

bool PathAppImageCombineExists(std::wstring &file, const wchar_t *cmd) {
  if (PathFileExistsW(L"Privexec.json")) {
    file.assign(L"Privexec.json");
    return true;
  }
  file.resize(PATHCCH_MAX_CCH, L'\0');
  auto pszFile = &file[0];
  auto N = GetModuleFileNameW(nullptr, pszFile, PATHCCH_MAX_CCH);
  if (PathCchRemoveExtension(pszFile, N + 8) != S_OK) {
    return false;
  }

  if (PathCchAddExtension(pszFile, N + 8, L".json") != S_OK) {
    return false;
  }
  auto k = wcslen(pszFile);
  file.resize(k);
  if (PathFileExistsW(file.data()))
    return true;
  file.clear();
  return false;
}

bool InitializePrivApp(HWND hWnd) {
  std::wstring file;
  if (!PathAppImageCombineExists(file, L"Privexec.json")) {
    return false;
  }
  auto hCombox = GetDlgItem(hWnd, IDC_COMMAND_COMBOX);
  if (IsUserAdministratorsGroup()) {
    WCHAR title[256];
    auto N = GetWindowTextW(hWnd, title, ARRAYSIZE(title));
    wcscat_s(title, L" [Administrator]");
    SetWindowTextW(hWnd, title);
  }
  try {
    std::ifstream fs;
    fs.open(file);
    auto json = nlohmann::json::parse(fs);
    auto cmds = json["Alias"];
    for (auto &cmd : cmds) {
      auto name = utf8towide(cmd["Name"].get<std::string>());
      auto command = utf8towide(cmd["Command"].get<std::string>());
      Aliascmd.insert(std::make_pair(name, command));
      ::SendMessage(hCombox, CB_ADDSTRING, 0, (LPARAM)name.data());
    }
  } catch (const std::exception &e) {
    OutputDebugStringA(e.what());
    return false;
  }
  return true;
}

INT_PTR WINAPI ApplicationProc(HWND hWndDlg, UINT message, WPARAM wParam,
                               LPARAM lParam) {
  switch (message) {
  // DarkGray background
  case WM_CTLCOLORDLG:
  case WM_CTLCOLORSTATIC:
    // case WM_CTLCOLORBTN:
    // return (INT_PTR)CreateSolidBrush(RGB(0, 191, 255));
    return (INT_PTR)CreateSolidBrush(RGB(255, 255, 255));
  case WM_INITDIALOG: {
    HICON icon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_APPLICATION_ICON));
    SendMessage(hWndDlg, WM_SETICON, ICON_BIG, (LPARAM)icon);
    InitializePrivApp(hWndDlg);
    auto hCombox = GetDlgItem(hWndDlg, IDC_USER_COMBOX);
    InitializeCombobox(hCombox);
    HMENU hSystemMenu = ::GetSystemMenu(hWndDlg, FALSE);
    InsertMenuW(hSystemMenu, SC_CLOSE, MF_ENABLED, IDM_PRIVEXEC_ABOUT,
                L"About Privexec\tAlt+F1");
    return 0;
  } break;
  case WM_SYSCOMMAND:
    switch (LOWORD(wParam)) {
    case IDM_PRIVEXEC_ABOUT: {
      MessageWindowEx(
          hWndDlg, L"About Privexec",
          L"Prerelease:"
          L" " PRIVEXEC_BUILD_VERSION L"\nCopyright \xA9 2018, Force "
          L"Charlie. All Rights Reserved.",
          L"For more information about this tool.\nVisit: <a "
          L"href=\"http://forcemz.net/\">forcemz.net</a>",
          kAboutWindow);
    }
      return 0;
    default:
      return DefWindowProcW(hWndDlg, message, wParam, lParam);
    }
    break;
  case WM_COMMAND: {
    switch (LOWORD(wParam)) {
    case IDB_EXECUTE_BUTTON: {
      std::wstring cmd;
      auto hCombox = GetDlgItem(hWndDlg, IDC_COMMAND_COMBOX);
      auto Length = GetWindowTextLengthW(hCombox);
      if (Length == 0) {
        return 0;
      }
      cmd.resize(Length + 1);
      GetWindowTextW(hCombox, &cmd[0], Length + 1); //// Null T
      cmd.resize(Length);

      auto N =
          SendMessage(GetDlgItem(hWndDlg, IDC_USER_COMBOX), CB_GETCURSEL, 0, 0);
      if (!Execute((int)N, cmd)) {
        ErrorMessage err(GetLastError());
        MessageBoxW(hWndDlg, err.message(), L"Privexec create process failed",
                    MB_OK | MB_ICONERROR);
      }
    } break;
    case IDB_EXIT_BUTTON:
      DestroyWindow(hWndDlg);
      break;
    }
    return 0;
  }

  case WM_CLOSE: {
    DestroyWindow(hWndDlg);
    return 0;
  }

  case WM_DESTROY: {
    PostQuitMessage(0);
    return 0;
  }
  default:
    break;
  }
  return FALSE;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
  g_hInst = hInstance;
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER(nCmdShow);
  DialogBoxParamW(hInstance, MAKEINTRESOURCE(IDD_APPLICATION_DIALOG), NULL,
                  ApplicationProc, 0L);
  return 0;
}
