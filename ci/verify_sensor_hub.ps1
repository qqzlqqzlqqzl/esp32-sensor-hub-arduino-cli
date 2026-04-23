param(
    [string]$CliPath = 'C:\Program Files\Arduino CLI\arduino-cli.exe',
    [string]$SketchPath = 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub',
    [string]$Port = 'COM7',
    [string]$Fqbn = 'esp32:esp32:esp32s3',
    [string]$WifiSsid,
    [string]$WifiPassword,
    [string]$ReportPath = 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub\ci\ci_report.md',
    [string]$CameraScriptPath = 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub\ci\capture_usb_camera.py'
)

$ErrorActionPreference = 'Stop'
$results = @()

function Add-Result {
    param(
        [string]$Step,
        [bool]$Passed,
        [string]$Details
    )
    $script:results += [pscustomobject]@{
        Step = $Step
        Passed = $Passed
        Details = $Details
    }
}

function Invoke-Curl {
    param(
        [string]$Url,
        [int]$TimeoutSeconds = 10
    )
    (& curl.exe --noproxy '*' --connect-timeout $TimeoutSeconds --max-time $TimeoutSeconds -sS $Url | Out-String)
}

function Get-PropertyValue {
    param(
        [object]$Object,
        [string]$Name,
        $Default = $null
    )

    if ($null -eq $Object) {
        return $Default
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $Default
    }

    return $property.Value
}

function Test-PropertyPresent {
    param(
        [object]$Object,
        [string]$Name
    )

    return $null -ne $Object -and $null -ne $Object.PSObject.Properties[$Name]
}

function Get-DashboardCandidates {
    param([string]$SerialLog)

    $matches = [regex]::Matches($SerialLog, '(?m)^\[URL\]\s+([0-9\.]+)/api/status\s*$')
    $latestFirst = New-Object System.Collections.Generic.List[string]
    for ($i = $matches.Count - 1; $i -ge 0; $i--) {
        $match = $matches[$i]
        $ip = $match.Groups[1].Value
        if ($latestFirst -notcontains $ip) {
            $latestFirst.Add($ip)
        }
    }

    $preferred = @($latestFirst | Where-Object { $_ -ne '192.168.4.1' })
    $fallback = @($latestFirst | Where-Object { $_ -eq '192.168.4.1' })
    return @($preferred + $fallback)
}

function Resolve-DashboardEndpoint {
    param([string]$SerialLog)

    $candidates = @(Get-DashboardCandidates -SerialLog $SerialLog)
    if ($candidates.Count -eq 0) {
        return [pscustomobject]@{
            SelectedIp = ''
            StatusText = ''
            StatusJson = $null
            Details = 'Did not find URL marker in serial log.'
        }
    }

    foreach ($candidate in $candidates) {
        try {
            $statusText = Invoke-Curl -Url ('http://' + $candidate + '/api/status') -TimeoutSeconds 6
            $statusJson = $statusText | ConvertFrom-Json
            return [pscustomobject]@{
                SelectedIp = $candidate
                StatusText = $statusText
                StatusJson = $statusJson
                Details = ('Detected dashboard IP {0} from serial URLs [{1}]' -f $candidate, ($candidates -join ', '))
            }
        }
        catch {
        }
    }

    return [pscustomobject]@{
        SelectedIp = $candidates[0]
        StatusText = ''
        StatusJson = $null
        Details = ('Detected serial URLs [{0}] but none responded to /api/status.' -f ($candidates -join ', '))
    }
}

function Test-SpeakerDiagnosticsPresent {
    param([object]$Speaker)

    if ($null -eq $Speaker) {
        return $false
    }

    $signalFields = @(
        'playback_passed',
        'playback_verified',
        'hardware_passed',
        'hardware_verified',
        'verification_passed',
        'verification_ok',
        'loopback_passed',
        'test_running',
        'baseline_dbfs',
        'observed_dbfs',
        'observed_peak',
        'pass_count',
        'fail_count',
        'last_test_ms'
    )

    return (Test-PropertyPresent -Object $Speaker -Name 'online') -and
        (($signalFields | Where-Object { Test-PropertyPresent -Object $Speaker -Name $_ }).Count -ge 1)
}

function Test-SpeakerVerificationPassed {
    param([object]$Speaker)

    if ($null -eq $Speaker) {
        return $false
    }

    foreach ($flag in @('playback_passed', 'playback_verified', 'hardware_passed', 'hardware_verified', 'verification_passed', 'verification_ok', 'speaker_passed', 'loopback_passed')) {
        $value = Get-PropertyValue -Object $Speaker -Name $flag
        if ($value -eq $true) {
            return $true
        }
        if ($value -is [string] -and $value -match '^(pass|passed|ok|success|verified)$') {
            return $true
        }
    }

    return ([int](Get-PropertyValue -Object $Speaker -Name 'pass_count' -Default 0) -gt 0)
}

function Get-SpeakerVerificationSummary {
    param([object]$Speaker)

    if ($null -eq $Speaker) {
        return '(speaker state missing)'
    }

    return ($Speaker | ConvertTo-Json -Depth 5 -Compress)
}

function Read-SerialLog {
    param(
        [string]$SerialPort,
        [int]$TimeoutSeconds = 180
    )

    $port = New-Object System.IO.Ports.SerialPort $SerialPort,115200,'None',8,'one'
    $port.ReadTimeout = 2500
    $port.DtrEnable = $false
    $port.RtsEnable = $false
    $port.Open()

    try {
        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        $lines = New-Object System.Collections.Generic.List[string]
        $firstUrlSeenAt = $null
        $preferredUrlSeen = $false
        while ((Get-Date) -lt $deadline) {
            try {
                $line = $port.ReadLine()
                if ($line) {
                    $trimmed = $line.Trim()
                    $lines.Add($trimmed)
                    if ($lines.Count -gt 400) {
                        $lines.RemoveAt(0)
                    }

                    $urlMatch = [regex]::Match($trimmed, '^\[URL\]\s+([0-9\.]+)/api/status$')
                    if ($urlMatch.Success) {
                        if ($null -eq $firstUrlSeenAt) {
                            $firstUrlSeenAt = Get-Date
                        }
                        if ($urlMatch.Groups[1].Value -ne '192.168.4.1') {
                            $preferredUrlSeen = $true
                        }
                    }
                }
            } catch {
            }

            if ($preferredUrlSeen -and $null -ne $firstUrlSeenAt -and (Get-Date) -ge $firstUrlSeenAt.AddSeconds(6)) {
                break
            }
            if ((-not $preferredUrlSeen) -and $null -ne $firstUrlSeenAt -and (Get-Date) -ge $firstUrlSeenAt.AddSeconds(35)) {
                break
            }
        }
        return ($lines -join [Environment]::NewLine)
    }
    finally {
        $port.Close()
    }
}

function Invoke-ArduinoCompile {
    param(
        [string]$Mode,
        [string]$Cli,
        [string]$Sketch,
        [string]$BoardFqbn,
        [string]$SerialPort,
        [string]$ExtraFlags
    )

    if ($Mode -eq 'build') {
        if ($ExtraFlags) {
            return (& $Cli compile --fqbn $BoardFqbn --build-property $ExtraFlags $Sketch 2>&1 | Out-String)
        }
        return (& $Cli compile --fqbn $BoardFqbn $Sketch 2>&1 | Out-String)
    }

    if ($ExtraFlags) {
        return (& $Cli compile -u -p $SerialPort --fqbn $BoardFqbn --build-property $ExtraFlags $Sketch 2>&1 | Out-String)
    }
    return (& $Cli compile -u -p $SerialPort --fqbn $BoardFqbn $Sketch 2>&1 | Out-String)
}

$extraFlags = $null
if ($WifiSsid) {
    $extraFlags = ('build.extra_flags=-DWIFI_STA_SSID="{0}" -DWIFI_STA_PASS="{1}"' -f $WifiSsid, $WifiPassword)
}

$serialLog = ''
$ip = ''

try {
    $buildOutput = Invoke-ArduinoCompile -Mode 'build' -Cli $CliPath -Sketch $SketchPath -BoardFqbn $Fqbn -SerialPort $Port -ExtraFlags $extraFlags
    Add-Result 'Build' ($LASTEXITCODE -eq 0) $buildOutput.Trim()

    $uploadOutput = Invoke-ArduinoCompile -Mode 'upload' -Cli $CliPath -Sketch $SketchPath -BoardFqbn $Fqbn -SerialPort $Port -ExtraFlags $extraFlags
    Add-Result 'Upload' ($LASTEXITCODE -eq 0) (($uploadOutput -split [Environment]::NewLine | Select-Object -Last 8) -join [Environment]::NewLine)

    Start-Sleep -Seconds 4
    $serialLog = Read-SerialLog -SerialPort $Port -TimeoutSeconds 180
    $dashboardEndpoint = Resolve-DashboardEndpoint -SerialLog $serialLog
    if ($dashboardEndpoint.SelectedIp) {
        $ip = $dashboardEndpoint.SelectedIp
        Add-Result 'Serial URL' ($null -ne $dashboardEndpoint.StatusJson) $dashboardEndpoint.Details
    } else {
        Add-Result 'Serial URL' $false ($dashboardEndpoint.Details + [Environment]::NewLine + $serialLog)
        throw 'No dashboard IP detected from serial log.'
    }

    $statusText = $dashboardEndpoint.StatusText
    $statusJson = $dashboardEndpoint.StatusJson
    if ($null -eq $statusJson) {
        throw ('Dashboard API did not respond at ' + $ip)
    }

    if (-not (Test-SpeakerVerificationPassed -Speaker $statusJson.speaker)) {
        (& curl.exe --noproxy '*' -sS -X POST ('http://' + $ip + '/api/speaker_test') | Out-String) | Out-Null
        Start-Sleep -Seconds 2
        $statusText = Invoke-Curl ('http://' + $ip + '/api/status')
        $statusJson = $statusText | ConvertFrom-Json
    }
    $speakerDiagnosticsOk = Test-SpeakerDiagnosticsPresent -Speaker $statusJson.speaker
    $speakerVerificationOk = Test-SpeakerVerificationPassed -Speaker $statusJson.speaker
    $statusOk = $statusJson.dht11.online -and
        $statusJson.ap3216c.online -and
        $statusJson.qma6100p.online -and
        $statusJson.mic.online -and
        $statusJson.speaker.online -and
        $speakerDiagnosticsOk -and
        $statusJson.chip_temp.online -and
        $statusJson.display.online -and
        $statusJson.storage.mount_ok -and
        ($statusJson.storage.write_failures -eq 0) -and
        ($statusJson.storage.dropped_samples -eq 0) -and
        $statusJson.system.cpu_freq_mhz -ge 1 -and
        ($statusJson.storage.persisted_samples -ge 1)
    Add-Result 'API Status' $statusOk $statusText
    Add-Result 'Speaker Verification' $speakerVerificationOk (Get-SpeakerVerificationSummary -Speaker $statusJson.speaker)

    $historyText = Invoke-Curl ('http://' + $ip + '/api/history')
    $historyJson = $historyText | ConvertFrom-Json
    $historyOk = ($historyJson.rows.Count -ge 1) -and ($null -ne $historyJson.rows[0].cpu_usage_pct)
    Add-Result 'API History' $historyOk ('rows=' + $historyJson.rows.Count)

    $htmlText = Invoke-Curl ('http://' + $ip + '/')
    $htmlOk = $htmlText.Contains('ESP32 多传感器看板') -and
        $htmlText.Contains('板载喇叭自检') -and
        $htmlText.Contains('/api/speaker_test') -and
        $htmlText.Contains('查看 CSV 日志') -and
        $htmlText.Contains('CPU 使用率')
    Add-Result 'HTML Dashboard' $htmlOk ('HTML bytes=' + $htmlText.Length)

    $csvText = Invoke-Curl ('http://' + $ip + '/api/log.csv')
    $csvLines = @($csvText -split [Environment]::NewLine | Where-Object { $_.Trim().Length -gt 0 })
    $csvOk = ($csvLines.Count -ge 1) -and $csvLines[0].Contains(',')
    Add-Result 'CSV Log' $csvOk ('lines=' + $csvLines.Count)

    $cameraText = (& python $CameraScriptPath 2>&1 | Out-String)
    $cameraJson = $cameraText | ConvertFrom-Json
    $cameraOk = $cameraJson.ok -and (Test-Path $cameraJson.snapshot_path)
    Add-Result 'USB Camera' $cameraOk $cameraText.Trim()
}
catch {
    Add-Result 'Unhandled Failure' $false $_.Exception.Message
}

$overallPassed = ($results | Where-Object { -not $_.Passed }).Count -eq 0
$overallText = if ($overallPassed) { 'PASS' } else { 'FAIL' }

$reportLines = @()
$reportLines += '# ESP32 Sensor Hub CI Report'
$reportLines += ''
$reportLines += ('Generated: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
$reportLines += ''
$reportLines += ('Overall: **' + $overallText + '**')
$reportLines += ''
$reportLines += '## BDD Scenarios'
$reportLines += '- Firmware builds and uploads with Arduino CLI'
$reportLines += '- Live dashboard exposes sensor telemetry, CPU status, LCD state and board speaker verification state'
$reportLines += '- HTML dashboard includes board speaker self-test control and CSV log availability'
$reportLines += '- Host USB camera capture writes an image artifact'
$reportLines += ''
$reportLines += '## Step Results'
foreach ($result in $results) {
    $icon = if ($result.Passed) { '[PASS]' } else { '[FAIL]' }
    $reportLines += ($icon + ' ' + $result.Step + ': ' + $result.Details)
}
$reportLines += ''
$reportLines += '## Serial Excerpt'
$reportLines += '```text'
if ($serialLog) {
    $serialExcerpt = @(($serialLog -split [Environment]::NewLine) | Where-Object { $_ -match '^\[(NET|URL|LIVE)\]' } | Select-Object -Last 6)
    if ($serialExcerpt.Count -eq 0) {
        $serialExcerpt = @(($serialLog -split [Environment]::NewLine) | Select-Object -First 3)
    }
    $reportLines += $serialExcerpt
} else {
    $reportLines += '(no serial output captured)'
}
$reportLines += '```'

$reportDir = Split-Path -Parent $ReportPath
if (-not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
}

Set-Content -Path $ReportPath -Value ($reportLines -join [Environment]::NewLine) -Encoding UTF8
Get-Content $ReportPath

if (-not $overallPassed) {
    exit 1
}
