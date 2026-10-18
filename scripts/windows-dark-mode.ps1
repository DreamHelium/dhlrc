# Switch Windows between its light and dark application mode.
#
# Windows normally offers this under Settings > Personalisation > Colours, but
# that page is locked on an unactivated Windows. The setting itself is only two
# values under the current user's registry key, so this script writes them
# directly; Qt -- and therefore dhlrc -- reads them when it starts.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\windows-dark-mode.ps1 Status
#   powershell -ExecutionPolicy Bypass -File scripts\windows-dark-mode.ps1 Dark
#   powershell -ExecutionPolicy Bypass -File scripts\windows-dark-mode.ps1 Light
#
# Restart dhlrc after switching: it picks the palette up at start-up. The mode
# is remembered by Windows, so running it once is enough.
#
# This only changes the current user's settings (HKCU); no administrator rights
# are needed and nothing else is touched. To go back, run the script with Light.

param(
    [ValidateSet('Dark', 'Light', 'Status')]
    [string]$Mode = 'Status'
)

$key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'

switch ($Mode) {
    'Status' {
        $current = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
        if ($null -eq $current -or $null -eq $current.AppsUseLightTheme) {
            Write-Output 'Windows app mode: not set (the light theme is used).'
        } elseif ($current.AppsUseLightTheme -eq 0) {
            Write-Output 'Windows app mode: dark.'
        } else {
            Write-Output 'Windows app mode: light.'
        }
    }
    'Dark' {
        if (-not (Test-Path $key)) {
            New-Item -Path $key -Force | Out-Null
        }
        Set-ItemProperty -Path $key -Name 'AppsUseLightTheme' -Value 0 -Type DWord
        Set-ItemProperty -Path $key -Name 'SystemUsesLightTheme' -Value 0 -Type DWord
        Write-Output 'Windows app mode: dark. Restart dhlrc to pick it up.'
    }
    'Light' {
        if (-not (Test-Path $key)) {
            New-Item -Path $key -Force | Out-Null
        }
        Set-ItemProperty -Path $key -Name 'AppsUseLightTheme' -Value 1 -Type DWord
        Set-ItemProperty -Path $key -Name 'SystemUsesLightTheme' -Value 1 -Type DWord
        Write-Output 'Windows app mode: light. Restart dhlrc to pick it up.'
    }
}
