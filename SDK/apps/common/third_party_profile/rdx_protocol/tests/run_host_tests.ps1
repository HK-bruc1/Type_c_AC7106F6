$ErrorActionPreference = 'Stop'

$testDir = $PSScriptRoot
$moduleDir = Split-Path -Parent $testDir
$compiler = Get-Command tcc -ErrorAction Stop
$profileRoot = Split-Path -Parent $moduleDir
$publicInclude = Join-Path $moduleDir 'include'
$applicationDir = Join-Path $moduleDir 'application'
$adapterDir = Join-Path $moduleDir 'adapters\jl_br56'
$protocolDir = Join-Path $moduleDir 'protocol\v23'
$integrationDir = Join-Path $moduleDir 'integration\type_c_ac7106f6'
$outputPaths = @()

try {
    $recordSessionOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_record_session_{0}.exe" -f [guid]::NewGuid().ToString('N'))
    $outputPaths += $recordSessionOutput
    & $compiler.Source -Wall -Werror `
        -I $publicInclude -I $profileRoot -o $recordSessionOutput `
        (Join-Path $testDir 'rdx_record_session_test.c') `
        (Join-Path $moduleDir 'domain\rdx_record_session.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX record session test compilation failed: $LASTEXITCODE"
    }

    & $recordSessionOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX record session tests failed: $LASTEXITCODE"
    }

    $rtcServiceOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_rtc_service_{0}.exe" -f [guid]::NewGuid().ToString('N'))
    $outputPaths += $rtcServiceOutput
    & $compiler.Source -Wall -Werror -DRDX_CFG_RTC_ENABLE=1 `
        -I (Join-Path $testDir 'include') `
        -I $publicInclude `
        -I $profileRoot `
        -o $rtcServiceOutput `
        (Join-Path $testDir 'rdx_rtc_service_test.c') `
        (Join-Path $testDir 'rdx_clock_fake.c') `
        (Join-Path $testDir 'rdx_rtc_backup_repository_fake.c') `
        (Join-Path $applicationDir 'rdx_rtc_service.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX RTC service test compilation failed: $LASTEXITCODE"
    }

    & $rtcServiceOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX RTC service tests failed: $LASTEXITCODE"
    }

    $clockOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_clock_jl_br56_{0}.exe" -f [guid]::NewGuid().ToString('N'))
    $outputPaths += $clockOutput
    & $compiler.Source -Wall -Werror -DRDX_CFG_RTC_ENABLE=1 `
        -I (Join-Path $testDir 'include') `
        -I $publicInclude `
        -I $profileRoot `
        -o $clockOutput `
        (Join-Path $testDir 'rdx_clock_jl_br56_test.c') `
        (Join-Path $adapterDir 'rdx_clock_jl_br56.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX BR56 clock adapter test compilation failed: $LASTEXITCODE"
    }

    & $clockOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX BR56 clock adapter tests failed: $LASTEXITCODE"
    }

    $rtcRepositoryOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_rtc_repository_br56_{0}.exe" -f `
            [guid]::NewGuid().ToString('N'))
    $outputPaths += $rtcRepositoryOutput
    & $compiler.Source -Wall -Werror -DRDX_CFG_RTC_ENABLE=1 `
        -I (Join-Path $testDir 'include') `
        -I $publicInclude `
        -I $profileRoot `
        -o $rtcRepositoryOutput `
        (Join-Path $testDir 'rdx_rtc_backup_repository_jl_br56_test.c') `
        (Join-Path $adapterDir 'rdx_rtc_backup_repository_jl_br56.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX BR56 RTC backup repository test compilation failed: $LASTEXITCODE"
    }

    & $rtcRepositoryOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX BR56 RTC backup repository tests failed: $LASTEXITCODE"
    }

    $stateServiceOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_device_state_service_{0}.exe" -f `
            [guid]::NewGuid().ToString('N'))
    $outputPaths += $stateServiceOutput
    & $compiler.Source -Wall -Werror `
        -I (Join-Path $testDir 'include') `
        -I $publicInclude `
        -I $profileRoot `
        -o $stateServiceOutput `
        (Join-Path $testDir 'rdx_device_state_service_test.c') `
        (Join-Path $testDir 'rdx_device_state_repository_fake.c') `
        (Join-Path $applicationDir 'rdx_device_state_service.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX device state service test compilation failed: $LASTEXITCODE"
    }

    & $stateServiceOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX device state service tests failed: $LASTEXITCODE"
    }

    $stateRepositoryOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_device_state_repository_jl_br56_{0}.exe" -f `
            [guid]::NewGuid().ToString('N'))
    $outputPaths += $stateRepositoryOutput
    & $compiler.Source -Wall -Werror `
        -I (Join-Path $testDir 'include') `
        -I $publicInclude `
        -I $profileRoot `
        -o $stateRepositoryOutput `
        (Join-Path $testDir 'rdx_device_state_repository_jl_br56_test.c') `
        (Join-Path $adapterDir 'rdx_device_state_repository_jl_br56.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX BR56 device state repository test compilation failed: $LASTEXITCODE"
    }

    & $stateRepositoryOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX BR56 device state repository tests failed: $LASTEXITCODE"
    }

    $audioOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_audio_capture_contract_{0}.exe" -f `
            [guid]::NewGuid().ToString('N'))
    $outputPaths += $audioOutput
    & $compiler.Source -Wall -Werror `
        -I $publicInclude -I $profileRoot -o $audioOutput `
        (Join-Path $testDir 'rdx_audio_capture_contract_test.c') `
        (Join-Path $testDir 'rdx_audio_capture_fake.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX audio capture contract test compilation failed: $LASTEXITCODE"
    }

    & $audioOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX audio capture contract tests failed: $LASTEXITCODE"
    }

    $contractOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_transport_contract_{0}.exe" -f [guid]::NewGuid().ToString('N'))
    $outputPaths += $contractOutput
    & $compiler.Source -Wall -Werror `
        -I $publicInclude -I $profileRoot -o $contractOutput `
        (Join-Path $testDir 'rdx_transport_contract_test.c') `
        (Join-Path $testDir 'rdx_transport_fake.c') `
        (Join-Path $protocolDir 'rdx_protocol_session.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX transport contract test compilation failed: $LASTEXITCODE"
    }

    & $contractOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX transport contract tests failed: $LASTEXITCODE"
    }

    $protocolOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_protocol_v23_{0}.exe" -f [guid]::NewGuid().ToString('N'))
    $outputPaths += $protocolOutput
    & $compiler.Source -Wall -Werror `
        -I (Join-Path $testDir 'include') `
        -I $publicInclude `
        -I $profileRoot `
        -o $protocolOutput `
        (Join-Path $testDir 'rdx_protocol_v23_host_test.c') `
        (Join-Path $testDir 'rdx_transport_fake.c') `
        (Join-Path $protocolDir 'rdx_protocol_v23.c') `
        (Join-Path $protocolDir 'rdx_protocol_v23_management.c') `
        (Join-Path $protocolDir 'rdx_protocol_session.c') `
        (Join-Path $integrationDir 'rdx_product_profile_type_c.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX V23 protocol test compilation failed: $LASTEXITCODE"
    }

    & $protocolOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX V23 protocol tests failed: $LASTEXITCODE"
    }
} finally {
    foreach ($outputPath in $outputPaths) {
        Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
    }
}
