/*
  Copyright (C) 2019  Stefan Sundin
  Copyright (C) 2025  borrageiros

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This is a modified version of SuperF4 by Stefan Sundin.
*/

#define AUTOSTART_VALUE_MAX (MAX_PATH + 64)

static int BuildAutostartValue(wchar_t *value, DWORD valueSize, int elevate) {
  wchar_t path[MAX_PATH];
  DWORD pathLen = GetModuleFileName(NULL, path, ARRAY_SIZE(path));
  DWORD needed;

  if (pathLen == 0 || pathLen >= ARRAY_SIZE(path)) {
    return 0;
  }
  path[ARRAY_SIZE(path) - 1] = L'\0';

  needed = 3 + pathLen + (elevate ? 9 : 0);
  if (needed >= valueSize) {
    return 0;
  }

  value[0] = L'"';
  wcscpy(value + 1, path);
  wcscat(value, L"\"");
  if (elevate) {
    wcscat(value, L" -elevate");
  }
  return 1;
}

int CheckAutostart() {
  HKEY key;
  wchar_t value[AUTOSTART_VALUE_MAX] = L"";
  wchar_t compare[AUTOSTART_VALUE_MAX];
  DWORD len = sizeof(value);

  if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return 0;
  }
  if (RegQueryValueEx(key, APP_NAME, NULL, NULL, (LPBYTE)value, &len) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return 0;
  }
  RegCloseKey(key);

  if (!BuildAutostartValue(compare, ARRAY_SIZE(compare), 0)) {
    return 0;
  }
  if (wcscmp(value, compare) != 0 && wcsstr(value, compare) != value) {
    return 0;
  }
  if (wcsstr(value, L" -elevate") != NULL) {
    return 2;
  }
  return 1;
}

void SetAutostart(int on, int elevate) {
  HKEY key;
  int error = RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
  if (error != ERROR_SUCCESS) {
    Error(L"RegCreateKeyEx(HKEY_CURRENT_USER,'Software\\Microsoft\\Windows\\CurrentVersion\\Run')", L"Error opening the registry.", error);
    return;
  }
  if (on) {
    wchar_t value[AUTOSTART_VALUE_MAX];
    if (!BuildAutostartValue(value, ARRAY_SIZE(value), elevate)) {
      RegCloseKey(key);
      return;
    }
    error = RegSetValueEx(key, APP_NAME, 0, REG_SZ, (LPBYTE)value, (DWORD)((wcslen(value) + 1) * sizeof(value[0])));
    if (error != ERROR_SUCCESS) {
      Error(L"RegSetValueEx('"APP_NAME"')", L"SetAutostart()", error);
      RegCloseKey(key);
      return;
    }
  }
  else {
    error = RegDeleteValue(key, APP_NAME);
    if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND) {
      Error(L"RegDeleteValue('"APP_NAME"')", L"SetAutostart()", error);
      RegCloseKey(key);
      return;
    }
  }
  RegCloseKey(key);
}
