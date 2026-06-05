/*
  Copyright (C) 2019  Stefan Sundin
  Copyright (C) 2025  borrageiros

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This is a modified version of SuperF4 by Stefan Sundin.
*/

#define AUTOSTART_RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define AUTOSTART_APPROVED_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run"

static void WriteAutostartIni(int on, int elevate) {
  WritePrivateProfileString(L"General", L"Autostart", on ? L"1" : L"0", inipath);
  WritePrivateProfileString(L"General", L"AutostartElevate", (on && elevate) ? L"1" : L"0", inipath);
}

static void SetStartupApproved(int on) {
  HKEY key;
  if (RegCreateKeyEx(HKEY_CURRENT_USER, AUTOSTART_APPROVED_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) {
    return;
  }
  if (on) {
    BYTE enabled[12] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    RegSetValueEx(key, APP_NAME, 0, REG_BINARY, enabled, sizeof(enabled));
  }
  else {
    RegDeleteValue(key, APP_NAME);
  }
  RegCloseKey(key);
}

static void WriteAutostartRegistry(int on, int elevate) {
  HKEY key;
  int error = RegCreateKeyEx(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
  if (error != ERROR_SUCCESS) {
    Error(L"RegCreateKeyEx(HKEY_CURRENT_USER,'Software\\Microsoft\\Windows\\CurrentVersion\\Run')", L"Error opening the registry.", error);
    return;
  }
  if (on) {
    wchar_t path[MAX_PATH], value[MAX_PATH+32];
    DWORD len = GetModuleFileName(NULL, path, ARRAY_SIZE(path));
    if (len == 0 || len >= ARRAY_SIZE(path)) {
      RegCloseKey(key);
      return;
    }
    swprintf(value, ARRAY_SIZE(value), L"\"%s\"%s", path, (elevate ? L" -elevate" : L""));
    error = RegSetValueEx(key, APP_NAME, 0, REG_SZ, (LPBYTE)value, (wcslen(value)+1)*sizeof(value[0]));
    if (error != ERROR_SUCCESS) {
      Error(L"RegSetValueEx('"APP_NAME"')", L"SetAutostart()", error);
      RegCloseKey(key);
      return;
    }
    SetStartupApproved(1);
  }
  else {
    error = RegDeleteValue(key, APP_NAME);
    if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND) {
      Error(L"RegDeleteValue('"APP_NAME"')", L"SetAutostart()", error);
      RegCloseKey(key);
      return;
    }
    SetStartupApproved(0);
  }
  RegCloseKey(key);
}

int CheckAutostart() {
  wchar_t txt[10];
  GetPrivateProfileString(L"General", L"Autostart", L"0", txt, ARRAY_SIZE(txt), inipath);
  if (!_wtoi(txt)) {
    return 0;
  }
  GetPrivateProfileString(L"General", L"AutostartElevate", L"0", txt, ARRAY_SIZE(txt), inipath);
  if (_wtoi(txt)) {
    return 2;
  }
  return 1;
}

void SetAutostart(int on, int elevate) {
  WriteAutostartIni(on, elevate);
  WriteAutostartRegistry(on, elevate);
}

void MigrateAutostartFromRegistry() {
  wchar_t txt[10];
  GetPrivateProfileString(L"General", L"Autostart", L"", txt, ARRAY_SIZE(txt), inipath);
  if (txt[0] != L'\0') {
    return;
  }
  HKEY key;
  wchar_t value[MAX_PATH+32] = L"";
  DWORD len = sizeof(value);
  if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return;
  }
  if (RegQueryValueEx(key, APP_NAME, NULL, NULL, (LPBYTE)value, &len) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return;
  }
  RegCloseKey(key);
  if (value[0] == L'\0') {
    return;
  }
  int elevate = (wcsstr(value, L" -elevate") != NULL) ? 1 : 0;
  WriteAutostartIni(1, elevate);
}

void SyncAutostartRegistry() {
  wchar_t txt[10];
  GetPrivateProfileString(L"General", L"Autostart", L"0", txt, ARRAY_SIZE(txt), inipath);
  int on = _wtoi(txt);
  GetPrivateProfileString(L"General", L"AutostartElevate", L"0", txt, ARRAY_SIZE(txt), inipath);
  int elevate = _wtoi(txt);
  WriteAutostartRegistry(on, elevate);
}
