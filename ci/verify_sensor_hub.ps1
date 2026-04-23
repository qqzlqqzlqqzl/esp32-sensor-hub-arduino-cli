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

function Invoke-CurlPost {
    param(
        [string]$Url,
        [int]$TimeoutSeconds = 10
    )
    (& curl.exe --noproxy '*' --connect-timeout $TimeoutSeconds --max-time $TimeoutSeconds -sS -X POST $Url | Out-String)
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

function ConvertFrom-EmbeddedJson {
    param([string]$Text)

    if ([string]::IsNullOrWhiteSpace($Text)) {
        throw 'No text available for JSON extraction.'
    }

    $lines = @($Text -split [Environment]::NewLine | Where-Object { $_.Trim().Length -gt 0 })
    for ($i = $lines.Count - 1; $i -ge 0; $i--) {
        $candidate = $lines[$i].Trim()
        if ($candidate.StartsWith('{') -and $candidate.EndsWith('}')) {
            return ($candidate | ConvertFrom-Json)
        }
    }

    $start = $Text.LastIndexOf('{')
    if ($start -lt 0) {
        throw 'No JSON object found in text.'
    }
    return ($Text.Substring($start) | ConvertFrom-Json)
}

function Get-FirstPresentPropertyValue {
    param(
        [object]$Object,
        [string[]]$Names,
        $Default = $null
    )

    foreach ($name in $Names) {
        if (Test-PropertyPresent -Object $Object -Name $name) {
            return Get-PropertyValue -Object $Object -Name $name -Default $Default
        }
    }

    return $Default
}

function Get-NullableIntValue {
    param($Value)

    if ($null -eq $Value) {
        return $null
    }

    try {
        return [int]$Value
    }
    catch {
        return $null
    }
}

function Get-NullableDoubleValue {
    param($Value)

    if ($null -eq $Value) {
        return $null
    }

    try {
        return [double]$Value
    }
    catch {
        return $null
    }
}

function Get-SpeakerSpeakCount {
    param([object]$Speaker)

    return Get-NullableIntValue (Get-FirstPresentPropertyValue -Object $Speaker -Names @(
        'speak_count',
        'spoken_count',
        'playback_count',
        'temperature_speak_count',
        'temperature_playback_count',
        'announce_count'
    ))
}

function Get-SpeakerHasSpokenTemperature {
    param([object]$Speaker)

    if ($null -eq $Speaker) {
        return $false
    }

    return [bool](Get-FirstPresentPropertyValue -Object $Speaker -Names @(
        'has_spoken_temp',
        'spoken_temperature',
        'temperature_announced',
        'has_announced_temperature'
    ) -Default $false)
}

function Get-SpeakerLastSpokenTemperature {
    param([object]$Speaker)

    return Get-NullableDoubleValue (Get-FirstPresentPropertyValue -Object $Speaker -Names @(
        'last_spoken_temp_c',
        'last_temperature_spoken_c',
        'spoken_temp_c',
        'last_announced_temp_c'
    ))
}

function Test-SpeakerPlaybackFieldsPresent {
    param([object]$Speaker)

    if ($null -eq $Speaker) {
        return $false
    }

    return (($null -ne (Get-SpeakerSpeakCount -Speaker $Speaker)) -or
        (Test-PropertyPresent -Object $Speaker -Name 'speak_running') -or
        (Test-PropertyPresent -Object $Speaker -Name 'has_spoken_temp') -or
        ($null -ne (Get-SpeakerLastSpokenTemperature -Speaker $Speaker)))
}

function Test-SpeakerPlaybackEvidenceChanged {
    param(
        [object]$BeforeSpeaker,
        [object]$AfterSpeaker,
        [double]$ExpectedTemperatureC
    )

    if ($null -eq $AfterSpeaker) {
        return $false
    }

    $beforeCount = Get-SpeakerSpeakCount -Speaker $BeforeSpeaker
    $afterCount = Get-SpeakerSpeakCount -Speaker $AfterSpeaker
    if ($null -ne $afterCount -and (($null -eq $beforeCount) -or ($afterCount -gt $beforeCount))) {
        return $true
    }

    $beforeHasSpoken = Get-SpeakerHasSpokenTemperature -Speaker $BeforeSpeaker
    $afterHasSpoken = Get-SpeakerHasSpokenTemperature -Speaker $AfterSpeaker
    $beforeTemp = Get-SpeakerLastSpokenTemperature -Speaker $BeforeSpeaker
    $afterTemp = Get-SpeakerLastSpokenTemperature -Speaker $AfterSpeaker
    if ($afterHasSpoken) {
        if (-not $beforeHasSpoken) {
            return $true
        }
        if ($null -ne $afterTemp -and ($null -eq $beforeTemp -or [math]::Abs($afterTemp - $beforeTemp) -gt 0.05)) {
            return $true
        }
        if (($null -eq $afterCount) -and $null -ne $afterTemp -and [math]::Abs($afterTemp - $ExpectedTemperatureC) -le 0.6) {
            return $true
        }
    }

    return $false
}

function Wait-ForSpeakTemperatureEvidence {
    param(
        [string]$Ip,
        [object]$BeforeStatus,
        [int]$TimeoutSeconds = 15
    )

    $beforeSpeaker = Get-PropertyValue -Object $BeforeStatus -Name 'speaker'
    $expectedTemperatureC = Get-NullableDoubleValue (Get-PropertyValue -Object (Get-PropertyValue -Object $BeforeStatus -Name 'dht11') -Name 'temp_c')
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $lastStatusText = ''
    $lastStatusJson = $null

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 900
        $lastStatusText = Invoke-Curl ('http://' + $Ip + '/api/status')
        $lastStatusJson = $lastStatusText | ConvertFrom-Json
        if (Test-SpeakerPlaybackEvidenceChanged -BeforeSpeaker $beforeSpeaker -AfterSpeaker $lastStatusJson.speaker -ExpectedTemperatureC $expectedTemperatureC) {
            return [pscustomobject]@{
                Passed = $true
                StatusText = $lastStatusText
                StatusJson = $lastStatusJson
                Details = ('Temperature playback evidence changed: ' + (Get-SpeakerVerificationSummary -Speaker $lastStatusJson.speaker))
            }
        }
    }

    return [pscustomobject]@{
        Passed = $false
        StatusText = $lastStatusText
        StatusJson = $lastStatusJson
        Details = ('No board-speaker temperature playback evidence changed within {0}s. Before={1} After={2}' -f $TimeoutSeconds, (Get-SpeakerVerificationSummary -Speaker $beforeSpeaker), (Get-SpeakerVerificationSummary -Speaker (Get-PropertyValue -Object $lastStatusJson -Name 'speaker')))
    }
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
        (Invoke-CurlPost ('http://' + $ip + '/api/speaker_test')) | Out-Null
        Start-Sleep -Seconds 2
        $statusText = Invoke-Curl ('http://' + $ip + '/api/status')
        $statusJson = $statusText | ConvertFrom-Json
    }
    $speakerDiagnosticsOk = Test-SpeakerDiagnosticsPresent -Speaker $statusJson.speaker
    $speakerVerificationOk = Test-SpeakerVerificationPassed -Speaker $statusJson.speaker
    $speakerPlaybackFieldsOk = Test-SpeakerPlaybackFieldsPresent -Speaker $statusJson.speaker
    $statusOk = $statusJson.dht11.online -and
        $statusJson.ap3216c.online -and
        $statusJson.qma6100p.online -and
        $statusJson.mic.online -and
        $statusJson.speaker.online -and
        $speakerDiagnosticsOk -and
        $speakerVerificationOk -and
        $speakerPlaybackFieldsOk -and
        $statusJson.chip_temp.online -and
        $statusJson.display.online -and
        $statusJson.storage.mount_ok -and
        ($statusJson.storage.write_failures -eq 0) -and
        ($statusJson.storage.dropped_samples -eq 0) -and
        $statusJson.system.cpu_freq_mhz -ge 1 -and
        ($statusJson.storage.persisted_samples -ge 1)
    Add-Result 'API Status' $statusOk $statusText
    Add-Result 'Speaker Verification' $speakerVerificationOk (Get-SpeakerVerificationSummary -Speaker $statusJson.speaker)
    Add-Result 'Speaker Playback Fields' $speakerPlaybackFieldsOk (Get-SpeakerVerificationSummary -Speaker $statusJson.speaker)

    $speakTriggerText = Invoke-CurlPost ('http://' + $ip + '/api/speak_temperature')
    $speakTriggerJson = $null
    try {
        $speakTriggerJson = $speakTriggerText | ConvertFrom-Json
    }
    catch {
    }
    $speakAccepted = ($null -eq $speakTriggerJson) -or ((Get-PropertyValue -Object $speakTriggerJson -Name 'accepted' -Default $true) -eq $true)
    Add-Result 'Speak Temperature Trigger' $speakAccepted $speakTriggerText.Trim()
    if (-not $speakAccepted) {
        throw 'Board rejected /api/speak_temperature request.'
    }

    $speakEvidence = Wait-ForSpeakTemperatureEvidence -Ip $ip -BeforeStatus $statusJson -TimeoutSeconds 15
    if ($speakEvidence.StatusJson) {
        $statusText = $speakEvidence.StatusText
        $statusJson = $speakEvidence.StatusJson
    }
    Add-Result 'Speak Temperature Playback' $speakEvidence.Passed $speakEvidence.Details

    $historyText = Invoke-Curl ('http://' + $ip + '/api/history')
    $historyJson = $historyText | ConvertFrom-Json
    $historyOk = ($historyJson.rows.Count -ge 1) -and ($null -ne $historyJson.rows[0].cpu_usage_pct)
    Add-Result 'API History' $historyOk ('rows=' + $historyJson.rows.Count)

    $htmlText = Invoke-Curl ('http://' + $ip + '/')
    $htmlOk = $htmlText.Contains('ESP32 多传感器看板') -and
        $htmlText.Contains('播报当前温度') -and
        $htmlText.Contains('板载喇叭自检') -and
        $htmlText.Contains('/api/speaker_test') -and
        $htmlText.Contains('/api/speak_temperature') -and
        $htmlText.Contains('查看 CSV 日志') -and
        $htmlText.Contains('CPU 使用率')
    Add-Result 'HTML Dashboard' $htmlOk ('HTML bytes=' + $htmlText.Length)

    $csvText = Invoke-Curl ('http://' + $ip + '/api/log.csv')
    $csvLines = @($csvText -split [Environment]::NewLine | Where-Object { $_.Trim().Length -gt 0 })
    $csvOk = ($csvLines.Count -ge 1) -and $csvLines[0].Contains(',')
    Add-Result 'CSV Log' $csvOk ('lines=' + $csvLines.Count)

    $cameraText = (& python $CameraScriptPath 2>&1 | Out-String)
    $cameraJson = ConvertFrom-EmbeddedJson -Text $cameraText
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
$reportLines += '- Live dashboard exposes sensor telemetry, CPU status, LCD state, board speaker verification state, and board speaker playback evidence'
$reportLines += '- HTML dashboard includes board speaker playback/self-test controls and CSV log availability'
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
