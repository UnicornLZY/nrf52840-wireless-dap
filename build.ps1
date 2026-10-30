param(
  [ValidateSet(64,128,240)][int]$PacketBytes=240,
  [ValidateSet(1,2)][int]$RadioMbps=2,
  [ValidateRange(0,80)][int]$Channel=80,
  [switch]$Fast,
  [string]$Cli='',
  [string]$Python='',
  [string]$Core="$env:LOCALAPPDATA\Arduino15\packages\adafruit\hardware\nrf52\1.7.0"
)
$ErrorActionPreference='Stop'
$env:PYTHONWARNINGS = 'ignore'

if (-not $Cli) {
  if (Get-Command arduino-cli -ErrorAction SilentlyContinue) { $Cli = 'arduino-cli' }
  elseif (Test-Path 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe') { $Cli = 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' }
  else { throw 'arduino-cli not found. Please specify -Cli path.' }
}

if (-not $Python) {
  if (Test-Path 'C:\Users\LZY\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe') {
    $Python = 'C:\Users\LZY\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
  }
  elseif (Get-Command python -ErrorAction SilentlyContinue) { $Python = 'python' }
  else { throw 'python not found. Please specify -Python path.' }
}

if(!(Test-Path -LiteralPath (Join-Path $Core 'platform.txt'))){throw "Install Adafruit nRF52 core 1.7.0 or pass -Core with its installed directory: $Core"}

$build=Join-Path $PSScriptRoot 'build'
$out=Join-Path $PSScriptRoot 'firmware'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$txBuf = if($PacketBytes -eq 240){ 256 } else { $PacketBytes }
$flags="-DWD_ESB=1 -DSERIAL_BUFFER_SIZE=1024 -DCFG_TUD_VENDOR_TX_BUFSIZE=$txBuf -DWD_USB_COMPAT=1 -DWD_V3_FIX=1 -DWD_ONE_ROW_RESET_P113_TEST=1 -DWD_COMPACT_WIRE_TEST=1 -DWD_LINK_RECOVERY_TEST=1 -DWD_SESSION_RESTART_TEST=1 -DWD_SOFT_RESET_VERIFY=1 -DWD_DAP_TRACE_CAPACITY=240 -DWD_RADIO_MBPS=$RadioMbps -DWD_RADIO_CHANNEL=$Channel -DWD_DAP128=$([int]($PacketBytes -eq 128)) -DWD_DAP240=$([int]($PacketBytes -eq 240))"
if($Fast -or ($PacketBytes -eq 240)){ $flags += " -DWD_SPEED_TEST=1" }

& $Cli compile --fqbn 'adafruit:nrf52:pca10056:softdevice=s140v6,debug=l0' --libraries "$PSScriptRoot\libraries" --build-path $build --build-property "build.variant.path=$PSScriptRoot\variant" --build-property "compiler.c.extra_flags=$flags" --build-property "compiler.cpp.extra_flags=$flags" --build-property 'compiler.c.elf.extra_flags=-Wl,--wrap=tud_cdc_line_state_cb' "$PSScriptRoot\unified_dap"
if($LASTEXITCODE -ne 0){throw 'Firmware compile failed'}

$firmwareName="uart_latency_packet${PacketBytes}_unified_v3_p113_${RadioMbps}m_fast_ch${Channel}_afh_v2.uf2"
$firmware=Join-Path $out $firmwareName

& $Python -W ignore "$Core\tools\uf2conv\uf2conv.py" "$build\unified_dap.ino.hex" -c -f 0xADA52840 -o $firmware
if($LASTEXITCODE -ne 0){throw 'UF2 conversion failed'}
& $Python "$PSScriptRoot\tools\verify_uf2.py" $firmware
if($LASTEXITCODE -ne 0){throw 'UF2 verification failed'}
Get-FileHash -LiteralPath $firmware -Algorithm SHA256 | Format-List
