param([string]$Cxx='g++')
$ErrorActionPreference='Stop'
$out=Join-Path $PSScriptRoot 'test-build'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$flags=@('-std=c++11','-Wall','-Wextra','-Werror','-DWD_SPEED_TEST=1','-DWD_DAP128=1','-DCFG_TUD_VENDOR_TX_BUFSIZE=128','-DWD_USB_COMPAT=1','-DWD_COMPACT_WIRE_TEST=1','-DWD_LINK_RECOVERY_TEST=1','-DWD_SESSION_RESTART_TEST=1','-DWD_ONE_ROW_RESET_P113_TEST=1','-DWD_SOFT_RESET_VERIFY=1')
foreach($test in @('free_role_test','packet128_test','recovery_test','uart_latency_test','uart_timing_probe')){
  $exe=Join-Path $out "$test.exe"
  & $Cxx @flags -I "$PSScriptRoot\tests\mocks" "$PSScriptRoot\tests\$test.cpp" -o $exe
  if($LASTEXITCODE -ne 0){throw "$test compilation failed"}
  & $exe
  if($LASTEXITCODE -ne 0){throw "$test failed"}
}
$exe=Join-Path $out 'radio_test.exe'
& $Cxx -std=c++11 -Wall -Wextra -Werror -DWD_RADIO_MBPS=2 -DWD_V3_FIX=1 -DWD_DAP128=1 -I "$PSScriptRoot\tests\radio_mocks" -I "$PSScriptRoot\tests\mocks" "$PSScriptRoot\tests\radio_test.cpp" -o $exe
if($LASTEXITCODE -ne 0){throw 'Radio test compilation failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Radio test failed'}
