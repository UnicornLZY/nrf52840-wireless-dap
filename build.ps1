param(
  [string]$Cli='arduino-cli',
  [string]$Python='python',
  [string]$Core=(Join-Path $env:LOCALAPPDATA 'Arduino15\packages\adafruit\hardware\nrf52\1.7.0')
)
$ErrorActionPreference='Stop'
if(!(Test-Path -LiteralPath (Join-Path $Core 'platform.txt'))){throw 'Install Adafruit nRF52 core 1.7.0 or pass -Core with its installed directory.'}
$build=Join-Path $PSScriptRoot 'build'
$out=Join-Path $PSScriptRoot 'firmware'
New-Item -ItemType Directory -Force -Path $out | Out-Null
# Same flags as the uart_latency DAP128 / Fast / 2 Mbps / channel 40 build.
$flags='-DWD_ESB=1 -DSERIAL_BUFFER_SIZE=1024 -DCFG_TUD_VENDOR_TX_BUFSIZE=128 -DWD_USB_COMPAT=1 -DWD_V3_FIX=1 -DWD_ONE_ROW_RESET_P113_TEST=1 -DWD_COMPACT_WIRE_TEST=1 -DWD_LINK_RECOVERY_TEST=1 -DWD_SESSION_RESTART_TEST=1 -DWD_SOFT_RESET_VERIFY=1 -DWD_DAP_TRACE_CAPACITY=240 -DWD_RADIO_MBPS=2 -DWD_RADIO_CHANNEL=40 -DWD_DAP128=1 -DWD_SPEED_TEST=1'
& $Cli compile --fqbn 'adafruit:nrf52:pca10056:softdevice=s140v6,debug=l0' --libraries "$PSScriptRoot\libraries" --build-path $build --build-property "build.variant.path=$PSScriptRoot\variant" --build-property "compiler.c.extra_flags=$flags" --build-property "compiler.cpp.extra_flags=$flags" --build-property 'compiler.c.elf.extra_flags=-Wl,--wrap=tud_cdc_line_state_cb' "$PSScriptRoot\unified_dap"
if($LASTEXITCODE -ne 0){throw 'Firmware compile failed'}
$firmware=Join-Path $out 'uart_latency_packet128_unified_v3_p113_2m_fast_ch40.uf2'
& $Python "$Core\tools\uf2conv\uf2conv.py" "$build\unified_dap.ino.hex" -c -f 0xADA52840 -o $firmware
if($LASTEXITCODE -ne 0){throw 'UF2 conversion failed'}
& $Python "$PSScriptRoot\tools\verify_uf2.py" $firmware
if($LASTEXITCODE -ne 0){throw 'UF2 verification failed'}
Get-FileHash -LiteralPath $firmware -Algorithm SHA256 | Format-List
