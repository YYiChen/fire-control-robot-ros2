# ============================================================
# Install WSL2 + Ubuntu 22.04 with direct connection (no proxy)
# Target stack: Ubuntu 22.04 + ROS2 Humble (LTS)
#
# 1. If your proxy app uses TUN / global mode (virtual NIC),
#    closing system proxy is NOT enough -- quit the proxy app first.
# 2. Run PowerShell as Administrator.
# 3. Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
# 4. & "this_script.ps1"
# 5. Reboot when prompted, then open 'Ubuntu 22.04' to set user/password.
# ============================================================

$ErrorActionPreference = 'Stop'

$k = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Internet Settings'
$prevEnable = (Get-ItemProperty $k).ProxyEnable
$prevServer = (Get-ItemProperty $k).ProxyServer

# Temporarily disable system proxy so wsl.exe download goes direct
Set-ItemProperty $k -Name ProxyEnable -Value 0
$env:HTTP_PROXY = $null; $env:HTTPS_PROXY = $null; $env:ALL_PROXY = $null
$env:http_proxy = $null;  $env:https_proxy = $null;  $env:all_proxy = $null
Write-Host "[OK] System proxy disabled temporarily. Using direct connection." -ForegroundColor Green

# Ensure default is WSL2
try {
    wsl --set-default-version 2 | Out-Null
} catch {
    Write-Host "[WARN] set-default-version failed (WSL may not be installed yet). Continuing." -ForegroundColor Yellow
}

# Install WSL2 + Ubuntu 22.04 (matches ROS2 Humble requirement)
Write-Host "[*] Installing WSL2 + Ubuntu 22.04 (direct download, please wait)..." -ForegroundColor Cyan
wsl --install -d Ubuntu-22.04

# Restore system proxy after install command returns (before reboot)
Set-ItemProperty $k -Name ProxyEnable -Value $prevEnable
if ($prevServer -ne $null) { Set-ItemProperty $k -Name ProxyServer -Value $prevServer }
Write-Host "[OK] System proxy restored (ProxyEnable=$prevEnable, ProxyServer=$prevServer)" -ForegroundColor Green
Write-Host "[*] Reboot when prompted. After reboot open 'Ubuntu 22.04' and set username/password." -ForegroundColor Yellow
