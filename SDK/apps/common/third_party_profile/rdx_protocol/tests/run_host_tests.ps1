[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$testDir = $PSScriptRoot
$moduleDir = Split-Path -Parent $testDir
$compiler = Get-Command tcc -ErrorAction Stop
$profileRoot = Split-Path -Parent $moduleDir
$publicInclude = Join-Path $moduleDir 'include'
$applicationDir = Join-Path $moduleDir 'application'
$protocolDir = Join-Path $moduleDir 'protocol\v23'
$integrationDir = Join-Path $moduleDir 'integration\type_c_ac7106f6'
$testInclude = Join-Path $testDir 'include'
$script:outputPaths = @()

function New-RdxTestSpec {
    param(
        [string]$Name,
        [string[]]$Sources,
        [string[]]$Includes = @(),
        [string[]]$Defines = @()
    )

    [pscustomobject]@{
        Name = $Name
        Sources = $Sources
        Includes = $Includes
        Defines = $Defines
    }
}

function Invoke-RdxTest {
    param(
        [pscustomobject]$Spec
    )

    $outputPath = Join-Path ([System.IO.Path]::GetTempPath()) `
        ("rdx_{0}_{1}.exe" -f $Spec.Name, [guid]::NewGuid().ToString('N'))
    $script:outputPaths += $outputPath

    $arguments = @('-Wall', '-Werror')
    foreach ($define in $Spec.Defines) {
        $arguments += "-D$define"
    }
    foreach ($include in $Spec.Includes) {
        $arguments += @('-I', $include)
    }
    $arguments += @('-o', $outputPath)
    $arguments += $Spec.Sources

    Write-Host ("[RDX][HOST] {0}" -f $Spec.Name)
    & $compiler.Source @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "RDX $($Spec.Name) test compilation failed: $LASTEXITCODE"
    }

    & $outputPath
    if ($LASTEXITCODE -ne 0) {
        throw "RDX $($Spec.Name) tests failed: $LASTEXITCODE"
    }
}

$specs = @(
    (New-RdxTestSpec -Name 'factory_identity' `
        -Includes @($publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_factory_identity_test.c'),
            (Join-Path $applicationDir 'rdx_factory_identity.c')
        )),
    (New-RdxTestSpec -Name 'factory_identity_provider' `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_factory_identity_provider_test.c'),
            (Join-Path $moduleDir 'adapters\jl_br56\rdx_factory_identity_jl_br56.c')
        )),
    (New-RdxTestSpec -Name 'identity_service' `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_identity_service_test.c'),
            (Join-Path $applicationDir 'rdx_identity_service.c'),
            (Join-Path $applicationDir 'rdx_factory_identity.c'),
            (Join-Path $integrationDir 'rdx_product_profile_type_c.c')
        )),
    (New-RdxTestSpec -Name 'ble_name_service' `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_ble_name_service_test.c'),
            (Join-Path $applicationDir 'rdx_ble_name_service.c'),
            (Join-Path $integrationDir 'rdx_product_profile_type_c.c')
        )),
    (New-RdxTestSpec -Name 'record_session' `
        -Includes @($publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_record_session_test.c'),
            (Join-Path $moduleDir 'domain\rdx_record_session.c')
        )),
    (New-RdxTestSpec -Name 'artifact' `
        -Includes @($publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_artifact_test.c'),
            (Join-Path $moduleDir 'domain\rdx_artifact.c')
        )),
    (New-RdxTestSpec -Name 'storage_command_slot' `
        -Includes @($profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_storage_command_slot_test.c'),
            (Join-Path $applicationDir 'rdx_storage_command_slot.c')
        )),
    (New-RdxTestSpec -Name 'composite_destination' `
        -Includes @($publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_record_composite_destination_test.c'),
            (Join-Path $applicationDir 'rdx_record_composite_destination.c')
        )),
    (New-RdxTestSpec -Name 'local_storage_service' `
        -Includes @($publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_local_storage_service_test.c'),
            (Join-Path $moduleDir 'domain\rdx_artifact.c'),
            (Join-Path $applicationDir 'rdx_storage_command_slot.c'),
            (Join-Path $applicationDir 'rdx_local_storage_service.c'),
            (Join-Path $applicationDir 'rdx_record_local_destination.c'),
            (Join-Path $applicationDir 'rdx_record_composite_destination.c'),
            (Join-Path $applicationDir 'rdx_record_coordinator.c')
        )),
    (New-RdxTestSpec -Name 'local_storage_checkpoint_enabled' `
        -Defines @('RDX_CFG_LOCAL_STORAGE_PERIODIC_CHECKPOINT_ENABLE=1') `
        -Includes @($publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_local_storage_service_test.c'),
            (Join-Path $moduleDir 'domain\rdx_artifact.c'),
            (Join-Path $applicationDir 'rdx_storage_command_slot.c'),
            (Join-Path $applicationDir 'rdx_local_storage_service.c'),
            (Join-Path $applicationDir 'rdx_record_local_destination.c'),
            (Join-Path $applicationDir 'rdx_record_composite_destination.c'),
            (Join-Path $applicationDir 'rdx_record_coordinator.c')
        )),
    (New-RdxTestSpec -Name 'protocol_v23_record_storage' `
        -Defines @(
            'RDX_CFG_ONLINE_RECORDING_ENABLE=1',
            'RDX_CFG_LOCAL_RECORDING_ENABLE=1',
            'RDX_STORAGE_BACKEND_NATIVE=1',
            'RDX_CFG_BLE_TX_CAN_SEND_ENABLE=1'
        ) `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_protocol_v23_record_storage_test.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23_record.c')
        )),
    (New-RdxTestSpec -Name 'protocol_v23_record_storage_diag' `
        -Defines @(
            'RDX_CFG_ONLINE_RECORDING_ENABLE=1',
            'RDX_CFG_LOCAL_RECORDING_ENABLE=1',
            'RDX_STORAGE_BACKEND_NATIVE=1',
            'RDX_CFG_BLE_TX_CAN_SEND_ENABLE=1',
            'RDX_CFG_BLE_TX_DIAG_ENABLE=1'
        ) `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_protocol_v23_record_storage_test.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23_record.c')
        )),
    (New-RdxTestSpec -Name 'protocol_v23_local_storage' `
        -Defines @(
            'RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE=1',
            'RDX_LOCAL_STORAGE_WIRE_DEV_TEST_ENABLE=1'
        ) `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_protocol_v23_local_storage_test.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23_local_storage.c')
        )),
    (New-RdxTestSpec -Name 'rtc_service' `
        -Defines @('RDX_CFG_RTC_ENABLE=1') `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_rtc_service_test.c'),
            (Join-Path $testDir 'rdx_clock_fake.c'),
            (Join-Path $testDir 'rdx_rtc_backup_repository_fake.c'),
            (Join-Path $applicationDir 'rdx_rtc_service.c')
        )),
    (New-RdxTestSpec -Name 'device_state_service' `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_device_state_service_test.c'),
            (Join-Path $testDir 'rdx_device_state_repository_fake.c'),
            (Join-Path $applicationDir 'rdx_device_state_service.c')
        )),
    (New-RdxTestSpec -Name 'audio_capture_contract' `
        -Includes @($publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_audio_capture_contract_test.c'),
            (Join-Path $testDir 'rdx_audio_capture_fake.c')
        )),
    (New-RdxTestSpec -Name 'protocol_v23' `
        -Defines @('RDX_CFG_OTA_ENABLE=1') `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_protocol_v23_host_test.c'),
            (Join-Path $testDir 'rdx_transport_fake.c'),
            (Join-Path $testDir 'rdx_firmware_update_fake.c'),
            (Join-Path $applicationDir 'rdx_ota_service.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23_ota.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23_management.c'),
            (Join-Path $protocolDir 'rdx_protocol_session.c'),
            (Join-Path $integrationDir 'rdx_product_profile_type_c.c')
        )),
    (New-RdxTestSpec -Name 'protocol_v23_sensitive_diag' `
        -Defines @(
            'RDX_CFG_OTA_ENABLE=1',
            'RDX_CFG_SENSITIVE_DATA_DIAG_ENABLE=1'
        ) `
        -Includes @($testInclude, $publicInclude, $profileRoot) `
        -Sources @(
            (Join-Path $testDir 'rdx_protocol_v23_host_test.c'),
            (Join-Path $testDir 'rdx_transport_fake.c'),
            (Join-Path $testDir 'rdx_firmware_update_fake.c'),
            (Join-Path $applicationDir 'rdx_ota_service.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23_ota.c'),
            (Join-Path $protocolDir 'rdx_protocol_v23_management.c'),
            (Join-Path $protocolDir 'rdx_protocol_session.c'),
            (Join-Path $integrationDir 'rdx_product_profile_type_c.c')
        ))
)

try {
    foreach ($spec in $specs) {
        Invoke-RdxTest -Spec $spec
    }
    Write-Host ("[RDX][HOST] {0} test groups passed" -f $specs.Count)
} finally {
    foreach ($outputPath in $script:outputPaths) {
        Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
    }
}
