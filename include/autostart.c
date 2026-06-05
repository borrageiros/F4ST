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

static int GetAutostartExePath(wchar_t *path, DWORD size) {
  DWORD len = GetModuleFileName(NULL, path, size);
  if (len == 0 || len >= size) {
    return 0;
  }
  wchar_t longPath[MAX_PATH];
  DWORD longLen = GetLongPathNameW(path, longPath, ARRAY_SIZE(longPath));
  if (longLen > 0 && longLen < ARRAY_SIZE(longPath)) {
    wcscpy(path, longPath);
  }
  return 1;
}

static int ParseAutostartCommandPath(const wchar_t *value, wchar_t *path, DWORD size) {
  const wchar_t *start = value;
  const wchar_t *end;
  if (value[0] == L'"') {
    start = value + 1;
    end = wcschr(start, L'"');
    if (end == NULL) {
      return 0;
    }
  }
  else {
    end = wcschr(start, L' ');
    if (end == NULL) {
      end = start + wcslen(start);
    }
  }
  DWORD len = (DWORD)(end - start);
  if (len == 0 || len >= size) {
    return 0;
  }
  wcsncpy(path, start, len);
  path[len] = L'\0';
  wchar_t longPath[MAX_PATH];
  DWORD longLen = GetLongPathNameW(path, longPath, ARRAY_SIZE(longPath));
  if (longLen > 0 && longLen < ARRAY_SIZE(longPath)) {
    wcscpy(path, longPath);
  }
  return 1;
}

static int IsStartupApprovedEnabled() {
  HKEY key;
  BYTE approved[12];
  DWORD len = sizeof(approved);
  if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTOSTART_APPROVED_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return 1;
  }
  if (RegQueryValueEx(key, APP_NAME, NULL, NULL, approved, &len) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return 1;
  }
  RegCloseKey(key);
  if (len == 0) {
    return 1;
  }
  return (approved[0] == 0x02 || approved[0] == 0x06);
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

int CheckAutostart() {
  HKEY key;
  wchar_t value[MAX_PATH+32] = L"";
  DWORD len = sizeof(value);
  if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return 0;
  }
  if (RegQueryValueEx(key, APP_NAME, NULL, NULL, (LPBYTE)value, &len) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return 0;
  }
  RegCloseKey(key);
  wchar_t entryPath[MAX_PATH];
  wchar_t currentPath[MAX_PATH];
  if (!ParseAutostartCommandPath(value, entryPath, ARRAY_SIZE(entryPath))) {
    return 0;
  }
  if (!GetAutostartExePath(currentPath, ARRAY_SIZE(currentPath))) {
    return 0;
  }
  if (_wcsicmp(entryPath, currentPath) != 0) {
    return 0;
  }
  if (!IsStartupApprovedEnabled()) {
    return 0;
  }
  if (wcsstr(value, L" -elevate") != NULL) {
    return 2;
  }
  return 1;
}

void SetAutostart(int on, int elevate) {
  HKEY key;
  int error = RegCreateKeyEx(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);
  if (error != ERROR_SUCCESS) {
    Error(L"RegCreateKeyEx(HKEY_CURRENT_USER,'Software\\Microsoft\\Windows\\CurrentVersion\\Run')", L"Error opening the registry.", error);
    return;
  }
  if (on) {
    wchar_t path[MAX_PATH], value[MAX_PATH+32];
    if (!GetAutostartExePath(path, ARRAY_SIZE(path))) {
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

void RepairAutostartPath() {
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
  wchar_t entryPath[MAX_PATH];
  wchar_t currentPath[MAX_PATH];
  if (!ParseAutostartCommandPath(value, entryPath, ARRAY_SIZE(entryPath))) {
    return;
  }
  if (!GetAutostartExePath(currentPath, ARRAY_SIZE(currentPath))) {
    return;
  }
  int elevate = (wcsstr(value, L" -elevate") != NULL) ? 1 : 0;
  if (_wcsicmp(entryPath, currentPath) != 0) {
    SetAutostart(1, elevate);
    return;
  }
  HKEY approvedKey;
  if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTOSTART_APPROVED_KEY, 0, KEY_QUERY_VALUE, &approvedKey) == ERROR_SUCCESS) {
    BYTE approved[12];
    DWORD approvedLen = sizeof(approved);
    if (RegQueryValueEx(approvedKey, APP_NAME, NULL, NULL, approved, &approvedLen) == ERROR_SUCCESS) {
      RegCloseKey(approvedKey);
      return;
    }
    RegCloseKey(approvedKey);
  }
  SetStartupApproved(1);
}
