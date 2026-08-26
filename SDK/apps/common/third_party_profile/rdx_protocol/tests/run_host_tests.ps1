$ErrorActionPreference = 'Stop'

$testDir = $PSScriptRoot
$moduleDir = Split-Path -Parent $testDir
$compiler = Get-Command tcc -ErrorAction Stop
$profileRoot = Split-Path -Parent $moduleDir
$earphoneInclude = Resolve-Path `
    (Join-Path $moduleDir '..\..\..\earphone\include')
$outputPaths = @()

try {
    $stateServiceOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_device_state_service_{0}.exe" -f `
            [guid]::NewGuid().ToString('N'))
    $outputPaths += $stateServiceOutput
    & $compiler.Source -Wall -Werror `
        -I (Join-Path $testDir 'include') `
        -I $moduleDir `
        -o $stateServiceOutput `
        (Join-Path $testDir 'rdx_device_state_service_test.c') `
        (Join-Path $testDir 'rdx_device_state_repository_fake.c') `
        (Join-Path $moduleDir 'rdx_device_state.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX device state service test compilation failed: $LASTEXITCODE"
    }

    & $stateServiceOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX device state service tests failed: $LASTEXITCODE"
    }

    $stateRepositoryOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_device_state_repository_br56_{0}.exe" -f `
            [guid]::NewGuid().ToString('N'))
    $outputPaths += $stateRepositoryOutput
    & $compiler.Source -Wall -Werror `
        -I (Join-Path $testDir 'include') `
        -I $moduleDir `
        -o $stateRepositoryOutput `
        (Join-Path $testDir 'rdx_device_state_repository_br56_test.c') `
        (Join-Path $moduleDir 'rdx_device_state_repository_br56.c')
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
    & $compiler.Source -Wall -Werror -I $moduleDir -o $audioOutput `
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
    & $compiler.Source -Wall -Werror -I $moduleDir -o $contractOutput `
        (Join-Path $testDir 'rdx_transport_contract_test.c') `
        (Join-Path $testDir 'rdx_transport_fake.c') `
        (Join-Path $moduleDir 'rdx_protocol_session.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX transport contract test compilation failed: $LASTEXITCODE"
    }

    & $contractOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX transport contract tests failed: $LASTEXITCODE"
    }

    $protocolOutput = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_mvp0_protocol_{0}.exe" -f [guid]::NewGuid().ToString('N'))
    $outputPaths += $protocolOutput
    & $compiler.Source -Wall -Werror `
        -I (Join-Path $testDir 'include') `
        -I $moduleDir `
        -I $profileRoot `
        -I $earphoneInclude `
        -o $protocolOutput `
        (Join-Path $testDir 'rdx_mvp0_protocol_host_test.c') `
        (Join-Path $testDir 'rdx_transport_fake.c') `
        (Join-Path $moduleDir 'rdx_mvp0_protocol.c') `
        (Join-Path $moduleDir 'rdx_protocol_session.c')
    if ($LASTEXITCODE -ne 0) {
        throw "RDX MVP0 protocol test compilation failed: $LASTEXITCODE"
    }

    & $protocolOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RDX MVP0 protocol tests failed: $LASTEXITCODE"
    }
} finally {
    foreach ($outputPath in $outputPaths) {
        Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
    }
}
