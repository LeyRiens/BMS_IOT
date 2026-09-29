$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$out = Join-Path ([IO.Path]::GetTempPath()) ('bms-gateway-tests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $out | Out-Null
Push-Location $repo
try {
    $flags = @('-std=c99', '-Wall', '-Wextra', '-Werror', '-I', 'tests/stubs', '-I', 'STM32_CAN_TO_USART/Core/Inc')
    & gcc @flags tests/buffer_test.c STM32_CAN_TO_USART/Core/Src/buffer.c -o (Join-Path $out 'buffer_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'buffer test compilation failed' }
    & (Join-Path $out 'buffer_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'buffer test failed' }
    & gcc @flags tests/gateway_test.c STM32_CAN_TO_USART/Core/Src/buffer.c STM32_CAN_TO_USART/Core/Src/mqtt_at.c STM32_CAN_TO_USART/Core/Src/state_user.c -o (Join-Path $out 'gateway_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'gateway test compilation failed' }
    $failed = @()
    foreach ($case in @('race', 'rx_error', 'latest', 'raw_abort', 'snapshot', 'early_result', 'prompt_echo', 'rx_wrap', 'raw_timeout', 'reject', 'reserved', 'rx_retry', 'raw_rx', 'raw_dma_timeout', 'esp_reset')) {
        & (Join-Path $out 'gateway_test.exe') $case
        if ($LASTEXITCODE -ne 0) { $failed += $case }
    }
    if ($failed.Count) { throw ('Failed: ' + ($failed -join ', ')) }
} finally {
    Pop-Location
}
