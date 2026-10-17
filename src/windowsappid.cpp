#include "windowsappid.h"

#if defined(_WIN32)

#include <string>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

/* Declared here rather than pulled in from a shell header, which differ
 * between SDK versions; the symbol lives in shell32. */
extern "C" HRESULT WINAPI
SetCurrentProcessExplicitAppUserModelID (PCWSTR app_id);

namespace
{
/* These have to match `DH_APP_ID` in CMakeLists.txt (the installer registers
 * the same id) and `default_app_id` in dhlrc-core/src/config.rs (the id the
 * core sends its toasts as). */
constexpr wchar_t APP_ID[] = L"cn.dh.dhlrc";
constexpr wchar_t DISPLAY_NAME[] = L"Minecraft Structure Modifier";

std::wstring
executablePath ()
{
  std::wstring path (MAX_PATH, L'\0');
  const DWORD length = GetModuleFileNameW (nullptr, path.data (),
                                           static_cast<DWORD> (path.size ()));
  path.resize (length);
  return path;
}

/* Windows takes the toast's name and icon from this key. The installer writes
 * it as well; writing it here too means a portable (unpacked) build is named
 * correctly. Harmless to repeat. */
void
registerAppUserModelId (const std::wstring &executable)
{
  const std::wstring key_path
      = std::wstring (L"Software\\Classes\\AppUserModelId\\") + APP_ID;
  HKEY key = nullptr;
  if (RegCreateKeyExW (HKEY_CURRENT_USER, key_path.c_str (), 0, nullptr, 0,
                       KEY_WRITE, nullptr, &key, nullptr)
      != ERROR_SUCCESS)
    return;

  RegSetValueExW (key, L"DisplayName", 0, REG_SZ,
                  reinterpret_cast<const BYTE *> (DISPLAY_NAME),
                  sizeof (DISPLAY_NAME));
  RegSetValueExW (
      key, L"IconUri", 0, REG_SZ,
      reinterpret_cast<const BYTE *> (executable.c_str ()),
      static_cast<DWORD> ((executable.size () + 1) * sizeof (wchar_t)));
  RegCloseKey (key);
}
}

void
applyWindowsAppUserModelID ()
{
  const std::wstring executable = executablePath ();
  registerAppUserModelId (executable);
  SetCurrentProcessExplicitAppUserModelID (APP_ID);
}

#else

void
applyWindowsAppUserModelID ()
{
}

#endif
