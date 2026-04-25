param(
    [string]$CliPath = 'C:\Program Files\Arduino CLI\arduino-cli.exe',
    [string]$SketchPath = 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub',
    [string]$Port = '',
    [string]$Fqbn = 'esp32:esp32:esp32s3',
    [string]$WifiSsid,
    [string]$WifiPassword,
    [string]$ReportPath = 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub\ci\ci_report.md',
    [string]$CameraScriptPath = 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub\ci\capture_usb_camera.py',
    [int]$SoakSeconds = 70,
    [switch]$IncludeCamera
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

function Resolve-UploadPort {
    param([string]$RequestedPort)

    $available = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
    if ($RequestedPort -and ($available -contains $RequestedPort)) {
        return [pscustomobject]@{
            Passed = $true
            Port = $RequestedPort
            Details = ('using requested port {0}; available={1}' -f $RequestedPort, (($available -join ', ')))
        }
    }

    $candidates = @($available | Where-Object { $_ -ne 'COM1' })
    if ($candidates.Count -gt 0) {
        return [pscustomobject]@{
            Passed = $true
            Port = $candidates[0]
            Details = ('using detected port {0}; requested={1}; available={2}' -f $candidates[0], $(if ($RequestedPort) { $RequestedPort } else { '(auto)' }), (($available -join ', ')))
        }
    }

    return [pscustomobject]@{
        Passed = $false
        Port = $null
        Details = ('no ESP32 upload port detected; requested={0}; available={1}' -f $(if ($RequestedPort) { $RequestedPort } else { '(auto)' }), $(if ($available.Count) { ($available -join ', ') } else { '(none)' }))
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

function Wait-DashboardStatus {
    param(
        [string]$Ip,
        [int]$TimeoutSeconds = 90
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $lastError = ''
    while ((Get-Date) -lt $deadline) {
        try {
            $statusText = Invoke-Curl ('http://' + $Ip + '/api/status') -TimeoutSeconds 5
            $statusJson = $statusText | ConvertFrom-Json
            return [pscustomobject]@{
                Passed = $true
                StatusText = $statusText
                StatusJson = $statusJson
                Details = 'Dashboard responded.'
            }
        }
        catch {
            $lastError = $_.Exception.Message
            Start-Sleep -Seconds 2
        }
    }

    return [pscustomobject]@{
        Passed = $false
        StatusText = ''
        StatusJson = $null
        Details = ('Dashboard did not respond within {0}s. Last error: {1}' -f $TimeoutSeconds, $lastError)
    }
}

function Invoke-FlushAndVerify {
    param(
        [string]$Ip,
        [int]$Attempts = 3
    )

    $lastText = ''
    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        try {
            $lastText = Invoke-CurlPost ('http://' + $Ip + '/api/flush') -TimeoutSeconds 8
            $flushJson = $lastText | ConvertFrom-Json
            if ($flushJson.flushed -eq $true) {
                return [pscustomobject]@{
                    Passed = $true
                    Text = $lastText
                    Details = $lastText.Trim()
                }
            }
        }
        catch {
            $lastText = $_.Exception.Message
        }
        Start-Sleep -Seconds 2
    }

    return [pscustomobject]@{
        Passed = $false
        Text = $lastText
        Details = $lastText
    }
}

function Test-CsvLog {
    param([string]$Ip)

    $csvText = Invoke-Curl ('http://' + $Ip + '/api/log.csv') -TimeoutSeconds 35
    $csvLines = @($csvText -split [Environment]::NewLine | Where-Object { $_.Trim().Length -gt 0 })
    $csvOk = ($csvLines.Count -ge 1) -and $csvLines[0].Contains(',')
    return [pscustomobject]@{
        Passed = $csvOk
        Details = ('lines=' + $csvLines.Count)
    }
}

function Test-HealthEndpoint {
    param(
        [string]$Ip,
        [object]$StatusJson
    )

    $healthText = Invoke-Curl ('http://' + $Ip + '/api/health')
    $healthJson = $healthText | ConvertFrom-Json
    $ok = ($healthJson.ok -eq $true) -and
        ($healthJson.status -eq 'OK') -and
        ($healthJson.storage_ok -eq $true) -and
        ($healthJson.sensors_ok -eq $true) -and
        ($healthJson.speaker_ok -eq $true) -and
        ($healthJson.camera_ok -eq $true) -and
        ($healthJson.alerts_active -eq $false) -and
        ([int]$healthJson.persisted_samples -ge 1) -and
        ([int]$healthJson.free_heap_bytes -gt 0)

    return [pscustomobject]@{
        Passed = $ok
        HealthText = $healthText
        HealthJson = $healthJson
        Details = $healthText.Trim()
    }
}

function Test-LiveEndpoint {
    param([string]$Ip)

    $liveText = Invoke-Curl ('http://' + $Ip + '/api/live')
    $liveJson = $liveText | ConvertFrom-Json
    $ok = ($null -ne $liveJson.dht11) -and
        ($null -ne $liveJson.ap3216c) -and
        ($null -ne $liveJson.qma6100p) -and
        ($null -ne $liveJson.mic) -and
        ($null -ne $liveJson.camera) -and
        ($null -ne $liveJson.system) -and
        ($null -ne $liveJson.storage) -and
        ($null -ne $liveJson.cadence) -and
        ([int]$liveJson.cadence.live_poll_ms -eq 500) -and
        ([int]$liveJson.cadence.snapshot_poll_ms -eq 10000) -and
        ([int]$liveJson.cadence.live_cache_ttl_ms -eq 1000) -and
        ([int]$liveJson.cadence.sample_interval_ms -eq 10000) -and
        ([int]$liveJson.cadence.flush_interval_ms -eq 10000) -and
        ([int]$liveJson.storage.live_build_count -ge 1) -and
        ($liveJson.camera.online -eq $true) -and
        ([int]$liveJson.system.free_heap_bytes -gt 0) -and
        ([int]$liveJson.storage.persisted_samples -ge 1)

    return [pscustomobject]@{
        Passed = $ok
        LiveText = $liveText
        LiveJson = $liveJson
        Details = $liveText.Trim()
    }
}

function Test-CameraEndpoint {
    param([string]$Ip)

    $cameraText = Invoke-Curl ('http://' + $Ip + '/api/camera') -TimeoutSeconds 8
    $cameraJson = $cameraText | ConvertFrom-Json
    $beforeCaptureCount = [int](Get-PropertyValue -Object $cameraJson.camera -Name 'capture_count' -Default 0)
    $beforeFailures = [int](Get-PropertyValue -Object $cameraJson.camera -Name 'capture_failures' -Default 0)
    $jpgPath = Join-Path $env:TEMP ('esp32_camera_' + [guid]::NewGuid().ToString('N') + '.jpg')
    $captureLatencyMs = New-Object System.Collections.Generic.List[int]
    try {
        $captureWatch = [System.Diagnostics.Stopwatch]::StartNew()
        & curl.exe --noproxy '*' --connect-timeout 8 --max-time 12 -sS -o $jpgPath ('http://' + $Ip + '/api/camera.jpg') | Out-Null
        $captureWatch.Stop()
        $captureLatencyMs.Add([int]$captureWatch.ElapsedMilliseconds)
        $jpgLength = if (Test-Path $jpgPath) { (Get-Item $jpgPath).Length } else { 0 }
    }
    finally {
        if (Test-Path $jpgPath) {
            Remove-Item -LiteralPath $jpgPath -Force
        }
    }
    for ($i = 0; $i -lt 4; $i++) {
        $loopJpgPath = Join-Path $env:TEMP ('esp32_camera_latency_' + [guid]::NewGuid().ToString('N') + '.jpg')
        try {
            $loopWatch = [System.Diagnostics.Stopwatch]::StartNew()
            & curl.exe --noproxy '*' --connect-timeout 8 --max-time 12 -sS -o $loopJpgPath ('http://' + $Ip + '/api/camera.jpg') | Out-Null
            $loopWatch.Stop()
            $loopLength = if (Test-Path $loopJpgPath) { (Get-Item $loopJpgPath).Length } else { 0 }
            if ($loopLength -gt 1024) {
                $captureLatencyMs.Add([int]$loopWatch.ElapsedMilliseconds)
            }
            else {
                $captureLatencyMs.Add(9999)
            }
        }
        finally {
            if (Test-Path $loopJpgPath) {
                Remove-Item -LiteralPath $loopJpgPath -Force
            }
        }
    }
    $latencyStats = $captureLatencyMs | Measure-Object -Average -Maximum
    $avgCaptureMs = [math]::Round($latencyStats.Average)
    $maxCaptureMs = [int]$latencyStats.Maximum
    $streamPath = Join-Path $env:TEMP ('esp32_camera_stream_' + [guid]::NewGuid().ToString('N') + '.mjpg')
    try {
        $streamWatch = [System.Diagnostics.Stopwatch]::StartNew()
        & curl.exe --noproxy '*' --connect-timeout 8 --max-time 3 -sS -o $streamPath ('http://' + $Ip + ':81/stream.mjpg') 2>$null | Out-Null
        $streamWatch.Stop()
        $streamBytes = if (Test-Path $streamPath) { (Get-Item $streamPath).Length } else { 0 }
        $streamText = if (Test-Path $streamPath) { [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($streamPath)) } else { '' }
        $streamFrames = [regex]::Matches($streamText, '--frame').Count
        $streamSeconds = [math]::Max(0.1, $streamWatch.Elapsed.TotalSeconds)
        $streamFps = [math]::Round(($streamFrames / $streamSeconds), 1)
    }
    finally {
        if (Test-Path $streamPath) {
            Remove-Item -LiteralPath $streamPath -Force
        }
    }

    $regText = Invoke-Curl ('http://' + $Ip + '/api/register?device=ap3216c&reg=0&mask=255') -TimeoutSeconds 6
    $regJson = $regText | ConvertFrom-Json
    $controlText = Invoke-CurlPost ('http://' + $Ip + '/api/camera/control?name=quality&value=18') -TimeoutSeconds 6
    $controlJson = $controlText | ConvertFrom-Json
    $brightnessText = Invoke-CurlPost ('http://' + $Ip + '/api/camera/control?name=brightness&value=1') -TimeoutSeconds 6
    $brightnessJson = $brightnessText | ConvertFrom-Json
    $invalidQualityText = Invoke-CurlPost ('http://' + $Ip + '/api/camera/control?name=quality&value=99') -TimeoutSeconds 6
    $invalidQualityJson = $invalidQualityText | ConvertFrom-Json
    $hexRegText = Invoke-Curl ('http://' + $Ip + '/api/register?device=ap3216c&reg=0x00&mask=255') -TimeoutSeconds 6
    $hexRegJson = $hexRegText | ConvertFrom-Json
    $es8388BeforeText = Invoke-Curl ('http://' + $Ip + '/api/register?device=es8388&reg=48&mask=255') -TimeoutSeconds 6
    $es8388BeforeJson = $es8388BeforeText | ConvertFrom-Json
    $es8388WriteText = Invoke-CurlPost ('http://' + $Ip + '/api/register?device=es8388&reg=48&mask=255&value=18') -TimeoutSeconds 6
    $es8388WriteJson = $es8388WriteText | ConvertFrom-Json
    if ($es8388BeforeJson.ok -eq $true) {
        Invoke-CurlPost ('http://' + $Ip + '/api/register?device=es8388&reg=48&mask=255&value=' + [int]$es8388BeforeJson.value) -TimeoutSeconds 6 | Out-Null
    }
    Invoke-CurlPost ('http://' + $Ip + '/api/camera/control?name=brightness&value=0') -TimeoutSeconds 6 | Out-Null
    $unsafeWriteText = Invoke-CurlPost ('http://' + $Ip + '/api/register?device=xl9555&reg=2&mask=255&value=255') -TimeoutSeconds 6
    $unsafeWriteJson = $unsafeWriteText | ConvertFrom-Json
    $badFrameText = Invoke-CurlPost ('http://' + $Ip + '/api/camera/control?name=framesize_name&value=BAD') -TimeoutSeconds 6
    $badFrameJson = $badFrameText | ConvertFrom-Json
    $badValueText = Invoke-CurlPost ('http://' + $Ip + '/api/camera/control?name=quality&value=BAD') -TimeoutSeconds 6
    $badValueJson = $badValueText | ConvertFrom-Json
    $badDeviceText = Invoke-Curl ('http://' + $Ip + '/api/register?device=bad%22device&reg=0') -TimeoutSeconds 6
    $badDeviceJson = $badDeviceText | ConvertFrom-Json
    $wrappedRegText = Invoke-Curl ('http://' + $Ip + '/api/register?device=ap3216c&reg=256') -TimeoutSeconds 6
    $wrappedRegJson = $wrappedRegText | ConvertFrom-Json
    Start-Sleep -Milliseconds 500
    $afterText = Invoke-Curl ('http://' + $Ip + '/api/camera') -TimeoutSeconds 8
    $afterJson = $afterText | ConvertFrom-Json
    $afterCaptureCount = [int](Get-PropertyValue -Object $afterJson.camera -Name 'capture_count' -Default 0)
    $afterFailures = [int](Get-PropertyValue -Object $afterJson.camera -Name 'capture_failures' -Default 0)
    $quality = [int](Get-PropertyValue -Object $afterJson.camera -Name 'quality' -Default -1)

    $ok = ($cameraJson.camera.online -eq $true) -and
        ([int](Get-PropertyValue -Object $cameraJson.camera -Name 'pid' -Default 0) -gt 0) -and
        ($jpgLength -gt 1024) -and
        ($avgCaptureMs -le 1200) -and
        ($maxCaptureMs -le 2500) -and
        ($streamBytes -gt 4096) -and
        ($streamFrames -ge 40) -and
        ($streamFps -ge 18.0) -and
        ($afterCaptureCount -gt $beforeCaptureCount) -and
        ($afterFailures -eq $beforeFailures) -and
        ($regJson.ok -eq $true) -and
        ($controlJson.applied -eq $true) -and
        ($controlJson.verified -eq $true) -and
        ([int]$controlJson.effective -eq 18) -and
        ($brightnessJson.applied -eq $true) -and
        ($brightnessJson.verified -eq $true) -and
        ([int]$brightnessJson.effective -eq 1) -and
        ($invalidQualityJson.applied -eq $false) -and
        ($invalidQualityJson.error -eq 'out_of_range') -and
        ($hexRegJson.ok -eq $false) -and
        ($hexRegJson.error -eq 'invalid_request') -and
        ($es8388WriteJson.ok -eq $true) -and
        ([int]$es8388WriteJson.value -eq 18) -and
        ($unsafeWriteJson.ok -eq $false) -and
        ($unsafeWriteJson.error -eq 'write_blocked') -and
        ($badFrameJson.applied -eq $false) -and
        ($badFrameJson.error -eq 'unsupported_framesize') -and
        ($badValueJson.applied -eq $false) -and
        ($badValueJson.error -eq 'invalid_value') -and
        ($badDeviceJson.ok -eq $false) -and
        ($badDeviceJson.error -eq 'invalid_request') -and
        ($wrappedRegJson.ok -eq $false) -and
        ($wrappedRegJson.error -eq 'reg_out_of_range') -and
        ($quality -eq 18)

    return [pscustomobject]@{
        Passed = $ok
        Details = ('online={0} name={1} pid={2} jpg_bytes={3} avg_jpg_ms={4} max_jpg_ms={5} stream_bytes={6} stream_frames={7} stream_fps={8} capture_before={9} capture_after={10} failures_before={11} failures_after={12} reg_value={13} quality={14} quality_verified={15} brightness_verified={16} invalid_quality={17} hex_reg={18} es8388_volume={19} unsafe_write={20} bad_frame={21} bad_value={22} bad_device={23} wrapped_reg={24}' -f $cameraJson.camera.online, $cameraJson.camera.name, $cameraJson.camera.pid, $jpgLength, $avgCaptureMs, $maxCaptureMs, $streamBytes, $streamFrames, $streamFps, $beforeCaptureCount, $afterCaptureCount, $beforeFailures, $afterFailures, $regJson.value, $quality, $controlJson.verified, $brightnessJson.verified, $invalidQualityJson.error, $hexRegJson.error, $es8388WriteJson.value, $unsafeWriteJson.error, $badFrameJson.error, $badValueJson.error, $badDeviceJson.error, $wrappedRegJson.error)
    }
}

function Test-BootTelemetry {
    param([object]$StatusJson)

    $boot = Get-PropertyValue -Object $StatusJson -Name 'boot' -Default $null
    if ($null -eq $boot) {
        return [pscustomobject]@{
            Passed = $false
            Details = 'Missing boot telemetry object.'
        }
    }

    $setupMs = [int](Get-PropertyValue -Object $boot -Name 'setup_complete_ms' -Default 0)
    $historyMs = [int](Get-PropertyValue -Object $boot -Name 'history_load_ms' -Default 0)
    $rowsCounted = [int](Get-PropertyValue -Object $boot -Name 'history_rows_counted' -Default 0)
    $rowsLoaded = [int](Get-PropertyValue -Object $boot -Name 'history_rows_loaded' -Default 0)
    $firstUrlMs = [int](Get-PropertyValue -Object $boot -Name 'first_url_report_ms' -Default 0)
    $serverMs = [int](Get-PropertyValue -Object $boot -Name 'server_start_ms' -Default -1)
    $displayMs = [int](Get-PropertyValue -Object $boot -Name 'display_init_ms' -Default -1)
    $sensorsMs = [int](Get-PropertyValue -Object $boot -Name 'sensors_init_ms' -Default -1)
    $maxLoadedRows = [math]::Min(120, $rowsCounted)

    $ok = ($setupMs -gt 0) -and
        ($setupMs -lt 8000) -and
        ($historyMs -lt 3000) -and
        ($rowsLoaded -le $maxLoadedRows) -and
        ($firstUrlMs -gt 0) -and
        ($firstUrlMs -ge $setupMs) -and
        ($serverMs -ge 0) -and
        ($displayMs -ge 0) -and
        ($sensorsMs -ge 0)

    return [pscustomobject]@{
        Passed = $ok
        Details = ('setup_ms={0} history_ms={1} rows_counted={2} rows_loaded={3} first_url_ms={4} server_ms={5} display_ms={6} sensors_ms={7}' -f $setupMs, $historyMs, $rowsCounted, $rowsLoaded, $firstUrlMs, $serverMs, $displayMs, $sensorsMs)
    }
}

function Wait-UptimeAtLeast {
    param(
        [string]$Ip,
        [int]$MinUptimeSeconds = 90,
        [int]$TimeoutSeconds = 130
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $lastText = ''
    $lastJson = $null
    while ((Get-Date) -lt $deadline) {
        $lastText = Invoke-Curl ('http://' + $Ip + '/api/status') -TimeoutSeconds 8
        $lastJson = $lastText | ConvertFrom-Json
        $uptime = [int](Get-PropertyValue -Object $lastJson.system -Name 'uptime_sec' -Default 0)
        if ($uptime -ge $MinUptimeSeconds) {
            return [pscustomobject]@{
                Passed = $true
                StatusText = $lastText
                StatusJson = $lastJson
                Details = ('uptime_sec={0}' -f $uptime)
            }
        }
        Start-Sleep -Seconds 5
    }

    return [pscustomobject]@{
        Passed = $false
        StatusText = $lastText
        StatusJson = $lastJson
        Details = ('Uptime did not reach {0}s before timeout.' -f $MinUptimeSeconds)
    }
}

function Test-HtmlCadenceMarkers {
    param([string]$HtmlText)

    return $HtmlText.Contains('/api/live') -and
        $HtmlText.Contains('const LIVE_POLL_MS = 500') -and
        $HtmlText.Contains('const SNAPSHOT_POLL_MS = 10000') -and
        $HtmlText.Contains('setTimeout(liveLoop, LIVE_POLL_MS)') -and
        $HtmlText.Contains('setTimeout(snapshotLoop, SNAPSHOT_POLL_MS)') -and
        (-not $HtmlText.Contains('setInterval(liveLoop')) -and
        (-not $HtmlText.Contains('setInterval(snapshotLoop'))
}

function Test-LiveCadenceSoak {
    param(
        [string]$Ip,
        [int]$DurationSeconds = 20
    )

    $beforeText = Invoke-Curl ('http://' + $Ip + '/api/live') -TimeoutSeconds 5
    $beforeJson = $beforeText | ConvertFrom-Json
    $beforeHeap = [int](Get-PropertyValue -Object $beforeJson.system -Name 'free_heap_bytes' -Default 0)
    $beforeBuildCount = [int](Get-PropertyValue -Object $beforeJson.storage -Name 'live_build_count' -Default 0)
    $minHeap = $beforeHeap
    $maxCpu = 0.0
    $okResponses = 0
    $failedResponses = 0
    $deadline = (Get-Date).AddSeconds($DurationSeconds)

    while ((Get-Date) -lt $deadline) {
        $startedAt = Get-Date
        try {
            $liveText = Invoke-Curl ('http://' + $Ip + '/api/live') -TimeoutSeconds 4
            $liveJson = $liveText | ConvertFrom-Json
            $cadenceOk = ([int]$liveJson.cadence.live_poll_ms -eq 500) -and
                ([int]$liveJson.cadence.snapshot_poll_ms -eq 10000) -and
                ([int]$liveJson.cadence.live_cache_ttl_ms -eq 1000) -and
                ([int]$liveJson.cadence.sample_interval_ms -eq 10000) -and
                ([int]$liveJson.cadence.flush_interval_ms -eq 10000)
            if ($cadenceOk) {
                $okResponses++
            } else {
                $failedResponses++
            }
            $freeHeap = [int](Get-PropertyValue -Object $liveJson.system -Name 'free_heap_bytes' -Default 0)
            if ($freeHeap -gt 0 -and ($minHeap -eq 0 -or $freeHeap -lt $minHeap)) {
                $minHeap = $freeHeap
            }
            $cpu = [double](Get-PropertyValue -Object $liveJson.system -Name 'cpu_usage_pct' -Default 0)
            if ($cpu -gt $maxCpu) {
                $maxCpu = $cpu
            }
        }
        catch {
            $failedResponses++
        }

        $elapsedMs = [int]((Get-Date) - $startedAt).TotalMilliseconds
        $sleepMs = 500 - $elapsedMs
        if ($sleepMs -gt 0) {
            Start-Sleep -Milliseconds $sleepMs
        }
    }

    $afterText = Invoke-Curl ('http://' + $Ip + '/api/live') -TimeoutSeconds 5
    $afterJson = $afterText | ConvertFrom-Json
    $afterHeap = [int](Get-PropertyValue -Object $afterJson.system -Name 'free_heap_bytes' -Default 0)
    $afterBuildCount = [int](Get-PropertyValue -Object $afterJson.storage -Name 'live_build_count' -Default 0)
    $liveBuilds = $afterBuildCount - $beforeBuildCount
    $heapDrop = $beforeHeap - $afterHeap
    $ok = ($okResponses -ge ([math]::Floor($DurationSeconds * 1.5))) -and
        ($failedResponses -eq 0) -and
        ($afterHeap -gt 0) -and
        ($heapDrop -lt 25000) -and
        ($liveBuilds -le ([math]::Floor($okResponses * 0.8) + 2))

    return [pscustomobject]@{
        Passed = $ok
        Details = ('duration_s={0} ok_responses={1} failed_responses={2} live_builds={3} heap_before={4} heap_after={5} min_heap={6} heap_drop={7} max_cpu_pct={8:n2}' -f $DurationSeconds, $okResponses, $failedResponses, $liveBuilds, $beforeHeap, $afterHeap, $minHeap, $heapDrop, $maxCpu)
    }
}

function Test-ConfigPersistence {
    param([string]$Ip)

    $configUrl = 'http://' + $Ip + '/api/config?temp_high_c=61.5&humidity_high_pct=88.0&sound_high_dbfs=-22.5&light_high_als=3456&cpu_high_pct=92.5&heap_low_bytes=54321&speaker_alerts=0'
    $saveText = Invoke-CurlPost $configUrl
    $saveJson = $saveText | ConvertFrom-Json
    $badAccepted = $false
    try {
        $badText = Invoke-CurlPost ('http://' + $Ip + '/api/config?light_high_als=-1&heap_low_bytes=abc') -TimeoutSeconds 5
        $badJson = $badText | ConvertFrom-Json
        $badAccepted = ($badJson.saved -eq $true)
    }
    catch {
        $badAccepted = $false
    }
    $rangeBadAccepted = $false
    try {
        $rangeBadText = Invoke-CurlPost ('http://' + $Ip + '/api/config?light_high_als=-1&heap_low_bytes=1') -TimeoutSeconds 5
        $rangeBadJson = $rangeBadText | ConvertFrom-Json
        $rangeBadAccepted = ($rangeBadJson.saved -eq $true)
    }
    catch {
        $rangeBadAccepted = $false
    }
    $statusText = Invoke-Curl ('http://' + $Ip + '/api/status')
    $statusJson = $statusText | ConvertFrom-Json
    $config = $statusJson.config
    $ok = $saveJson.saved -and
        (-not $badAccepted) -and
        (-not $rangeBadAccepted) -and
        $config.persisted -and
        ([math]::Abs([double]$config.temp_high_c - 61.5) -lt 0.01) -and
        ([math]::Abs([double]$config.humidity_high_pct - 88.0) -lt 0.01) -and
        ([math]::Abs([double]$config.sound_high_dbfs - (-22.5)) -lt 0.01) -and
        ([int]$config.light_high_als -eq 3456) -and
        ([math]::Abs([double]$config.cpu_high_pct - 92.5) -lt 0.01) -and
        ([int]$config.heap_low_bytes -eq 54321)

    return [pscustomobject]@{
        Passed = $ok
        StatusText = $statusText
        StatusJson = $statusJson
        Details = ('save=' + $saveText.Trim() + ' invalid_rejected=' + [string](-not $badAccepted) + ' range_invalid_rejected=' + [string](-not $rangeBadAccepted) + ' config=' + ($config | ConvertTo-Json -Depth 5 -Compress))
    }
}

function Test-RebootPersistence {
    param(
        [string]$Ip,
        [object]$BeforeStatus
    )

    $flushBefore = Invoke-FlushAndVerify -Ip $Ip
    if (-not $flushBefore.Passed) {
        return [pscustomobject]@{
            Passed = $false
            StatusText = ''
            StatusJson = $null
            Details = ('Pre-reboot flush failed: ' + $flushBefore.Details)
        }
    }
    Start-Sleep -Seconds 1
    $readyBefore = Wait-UptimeAtLeast -Ip $Ip -MinUptimeSeconds 90 -TimeoutSeconds 140
    if (-not $readyBefore.Passed) {
        return [pscustomobject]@{
            Passed = $false
            StatusText = $readyBefore.StatusText
            StatusJson = $readyBefore.StatusJson
            Details = ('Board was not old enough for strict reboot evidence: ' + $readyBefore.Details)
        }
    }
    $beforeText = $readyBefore.StatusText
    $beforeJson = $readyBefore.StatusJson
    $beforePersisted = [int](Get-PropertyValue -Object $beforeJson.storage -Name 'persisted_samples' -Default 0)
    $beforeTempHigh = [double](Get-PropertyValue -Object $beforeJson.config -Name 'temp_high_c' -Default 0)
    $beforeUptime = [int](Get-PropertyValue -Object $beforeJson.system -Name 'uptime_sec' -Default 0)

    $rebootAccepted = $false
    try {
        $rebootText = Invoke-CurlPost ('http://' + $Ip + '/api/reboot') -TimeoutSeconds 3
        $rebootJson = $rebootText | ConvertFrom-Json
        $rebootAccepted = ($rebootJson.rebooting -eq $true)
    }
    catch {
    }
    if (-not $rebootAccepted) {
        return [pscustomobject]@{
            Passed = $false
            StatusText = $beforeText
            StatusJson = $beforeJson
            Details = 'Board did not accept /api/reboot.'
        }
    }

    Start-Sleep -Seconds 3
    $deadline = (Get-Date).AddSeconds(130)
    $after = $null
    while ((Get-Date) -lt $deadline) {
        $candidate = Wait-DashboardStatus -Ip $Ip -TimeoutSeconds 8
        if ($candidate.Passed) {
            $candidateUptime = [int](Get-PropertyValue -Object $candidate.StatusJson.system -Name 'uptime_sec' -Default 999999)
            $candidateTempHigh = [double](Get-PropertyValue -Object $candidate.StatusJson.config -Name 'temp_high_c' -Default 0)
            $candidatePersistedConfig = [bool](Get-PropertyValue -Object $candidate.StatusJson.config -Name 'persisted' -Default $false)
            if (($candidateUptime -lt $beforeUptime) -and
                $candidatePersistedConfig -and
                ([math]::Abs($candidateTempHigh - $beforeTempHigh) -lt 0.01)) {
                $after = $candidate
                break
            }
        }
        Start-Sleep -Seconds 2
    }

    if ($null -eq $after) {
        return [pscustomobject]@{
            Passed = $false
            StatusText = ''
            StatusJson = $null
            Details = ('No reboot evidence with persisted config within timeout. before_uptime={0} before_temp_high={1}' -f $beforeUptime, $beforeTempHigh)
        }
    }

    $afterJson = $after.StatusJson
    $afterPersisted = [int](Get-PropertyValue -Object $afterJson.storage -Name 'persisted_samples' -Default 0)
    $afterTempHigh = [double](Get-PropertyValue -Object $afterJson.config -Name 'temp_high_c' -Default 0)
    $afterUptime = [int](Get-PropertyValue -Object $afterJson.system -Name 'uptime_sec' -Default 999999)
    $historyText = Invoke-Curl ('http://' + $Ip + '/api/history')
    $historyJson = $historyText | ConvertFrom-Json
    $ok = $afterJson.storage.mount_ok -and
        $afterJson.config.persisted -and
        ($afterPersisted -ge $beforePersisted) -and
        ([math]::Abs($afterTempHigh - $beforeTempHigh) -lt 0.01) -and
        ($afterUptime -lt $beforeUptime) -and
        ($historyJson.rows.Count -ge 1)

    return [pscustomobject]@{
        Passed = $ok
        StatusText = $after.StatusText
        StatusJson = $afterJson
        Details = ('before_persisted={0} after_persisted={1} before_uptime={2} after_uptime={3} before_temp_high={4} after_temp_high={5} history_rows={6}' -f $beforePersisted, $afterPersisted, $beforeUptime, $afterUptime, $beforeTempHigh, $afterTempHigh, $historyJson.rows.Count)
    }
}

function Test-ShortSoak {
    param(
        [string]$Ip,
        [int]$DurationSeconds
    )

    $beforeText = Invoke-Curl ('http://' + $Ip + '/api/status')
    $beforeJson = $beforeText | ConvertFrom-Json
    $beforePersisted = [int](Get-PropertyValue -Object $beforeJson.storage -Name 'persisted_samples' -Default 0)
    $beforeHeap = [int](Get-PropertyValue -Object $beforeJson.system -Name 'free_heap_bytes' -Default 0)
    $deadline = (Get-Date).AddSeconds($DurationSeconds)
    $polls = 0
    $minFreeHeap = $beforeHeap
    $lastJson = $beforeJson

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        $statusText = Invoke-Curl ('http://' + $Ip + '/api/status') -TimeoutSeconds 8
        $lastJson = $statusText | ConvertFrom-Json
        $freeHeap = [int](Get-PropertyValue -Object $lastJson.system -Name 'free_heap_bytes' -Default 0)
        if ($freeHeap -gt 0 -and ($minFreeHeap -eq 0 -or $freeHeap -lt $minFreeHeap)) {
            $minFreeHeap = $freeHeap
        }
        $polls++
    }

    $flush = Invoke-FlushAndVerify -Ip $Ip
    if (-not $flush.Passed) {
        return [pscustomobject]@{
            Passed = $false
            StatusText = ''
            StatusJson = $null
            Details = ('Soak flush failed: ' + $flush.Details)
        }
    }
    Start-Sleep -Seconds 1
    $afterText = Invoke-Curl ('http://' + $Ip + '/api/status')
    $afterJson = $afterText | ConvertFrom-Json
    $afterPersisted = [int](Get-PropertyValue -Object $afterJson.storage -Name 'persisted_samples' -Default 0)
    $afterHeap = [int](Get-PropertyValue -Object $afterJson.system -Name 'free_heap_bytes' -Default 0)
    $heapDrop = $beforeHeap - $afterHeap
    $ok = ($polls -ge 2) -and
        ($afterPersisted -gt $beforePersisted) -and
        ($afterJson.storage.write_failures -eq 0) -and
        ($afterJson.storage.dropped_samples -eq 0) -and
        ($afterHeap -gt 0) -and
        ($heapDrop -lt 40000)

    return [pscustomobject]@{
        Passed = $ok
        StatusText = $afterText
        StatusJson = $afterJson
        Details = ('duration_s={0} polls={1} persisted_before={2} persisted_after={3} heap_before={4} heap_after={5} heap_min_seen={6} heap_drop={7}' -f $DurationSeconds, $polls, $beforePersisted, $afterPersisted, $beforeHeap, $afterHeap, $minFreeHeap, $heapDrop)
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

    $portResolution = Resolve-UploadPort -RequestedPort $Port
    Add-Result 'Serial Port' $portResolution.Passed $portResolution.Details
    if (-not $portResolution.Passed) {
        throw $portResolution.Details
    }
    $Port = $portResolution.Port

    $uploadOutput = Invoke-ArduinoCompile -Mode 'upload' -Cli $CliPath -Sketch $SketchPath -BoardFqbn $Fqbn -SerialPort $Port -ExtraFlags $extraFlags
    Add-Result 'Upload' ($LASTEXITCODE -eq 0) (($uploadOutput -split [Environment]::NewLine | Select-Object -Last 8) -join [Environment]::NewLine)

    Start-Sleep -Seconds 1
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
        $statusJson.camera.online -and
        $statusJson.speaker.online -and
        $speakerDiagnosticsOk -and
        $speakerVerificationOk -and
        $speakerPlaybackFieldsOk -and
        $statusJson.chip_temp.online -and
        $statusJson.display.online -and
        $statusJson.storage.mount_ok -and
        ($null -ne $statusJson.config) -and
        ($null -ne $statusJson.alerts) -and
        ($statusJson.storage.write_failures -eq 0) -and
        ($statusJson.storage.dropped_samples -eq 0) -and
        $statusJson.system.cpu_freq_mhz -ge 1 -and
        ($statusJson.storage.persisted_samples -ge 1)
    Add-Result 'API Status' $statusOk $statusText
    Add-Result 'Speaker Verification' $speakerVerificationOk (Get-SpeakerVerificationSummary -Speaker $statusJson.speaker)
    Add-Result 'Speaker Playback Fields' $speakerPlaybackFieldsOk (Get-SpeakerVerificationSummary -Speaker $statusJson.speaker)

    $bootTelemetry = Test-BootTelemetry -StatusJson $statusJson
    Add-Result 'Boot Telemetry' $bootTelemetry.Passed $bootTelemetry.Details

    $health = Test-HealthEndpoint -Ip $ip -StatusJson $statusJson
    Add-Result 'API Health' $health.Passed $health.Details

    $live = Test-LiveEndpoint -Ip $ip
    Add-Result 'API Live Cadence' $live.Passed $live.Details

    $camera = Test-CameraEndpoint -Ip $ip
    Add-Result 'Camera Capture And Controls' $camera.Passed $camera.Details

    $liveSoak = Test-LiveCadenceSoak -Ip $ip -DurationSeconds 20
    Add-Result 'API Live 2Hz Soak' $liveSoak.Passed $liveSoak.Details

    $configPersistence = Test-ConfigPersistence -Ip $ip
    if ($configPersistence.StatusJson) {
        $statusText = $configPersistence.StatusText
        $statusJson = $configPersistence.StatusJson
    }
    Add-Result 'Persistent Config' $configPersistence.Passed $configPersistence.Details

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
        $htmlText.Contains('保存阈值') -and
        $htmlText.Contains('/api/health') -and
        $htmlText.Contains('/api/live') -and
        $htmlText.Contains('stream.mjpg') -and
        $htmlText.Contains('const CAMERA_STREAM_PORT = 81') -and
        $htmlText.Contains('const CAMERA_STREAM_TARGET_FPS = 20') -and
        $htmlText.Contains('/api/camera/control') -and
        $htmlText.Contains('/api/register') -and
        $htmlText.Contains('/api/system/control') -and
        $htmlText.Contains('/api/speaker_test') -and
        $htmlText.Contains('/api/speak_temperature') -and
        $htmlText.Contains('/api/config') -and
        $htmlText.Contains('查看 CSV 日志') -and
        $htmlText.Contains('CPU 使用率') -and
        $htmlText.Contains('只接受十进制') -and
        $htmlText.Contains('ES8388 音频') -and
        $htmlText.Contains('寄存器 46 到 49') -and
        $htmlText.Contains('写入值 0 到 33') -and
        $htmlText.Contains('写入被禁止') -and
        $htmlText.Contains('XL9555 IO 扩展') -and
        $htmlText.Contains('已生效') -and
        (Test-HtmlCadenceMarkers -HtmlText $htmlText)
    Add-Result 'HTML Dashboard' $htmlOk ('HTML bytes=' + $htmlText.Length)

    $rebootPersistence = Test-RebootPersistence -Ip $ip -BeforeStatus $statusJson
    if ($rebootPersistence.StatusJson) {
        $statusText = $rebootPersistence.StatusText
        $statusJson = $rebootPersistence.StatusJson
    }
    Add-Result 'Reboot Persistence' $rebootPersistence.Passed $rebootPersistence.Details

    $soak = Test-ShortSoak -Ip $ip -DurationSeconds $SoakSeconds
    if ($soak.StatusJson) {
        $statusText = $soak.StatusText
        $statusJson = $soak.StatusJson
    }
    Add-Result 'Short Soak' $soak.Passed $soak.Details

    $csv = Test-CsvLog -Ip $ip
    Add-Result 'CSV Log' $csv.Passed $csv.Details

    if ($IncludeCamera) {
        $cameraText = (& python $CameraScriptPath 2>&1 | Out-String)
        $cameraJson = ConvertFrom-EmbeddedJson -Text $cameraText
        $cameraOk = $cameraJson.ok -and (Test-Path $cameraJson.snapshot_path)
        Add-Result 'USB Camera' $cameraOk $cameraText.Trim()
    } else {
        Add-Result 'USB Camera' $true 'Skipped by default; pass -IncludeCamera to verify host USB camera artifacts.'
    }
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
$reportLines += '- Live dashboard exposes sensor telemetry, MC5640 camera status, CPU status, LCD state, health, alerts, persistent config, board speaker verification state, and board speaker playback evidence'
$reportLines += '- Boot-to-dashboard readiness exposes setup timing telemetry and limits boot history parsing to the latest dashboard rows'
$reportLines += '- Camera JPEG capture, effective camera controls and safe decimal hardware register access are verified through HTTP APIs'
$reportLines += '- Peripheral register controls expose Chinese decimal guidance, safe writable ranges, and blocked unsafe writes'
$reportLines += '- Camera dashboard uses a dedicated MJPEG stream on port 81 with a measured 20 FPS target'
$reportLines += '- HTML dashboard uses completion-based /api/live polling for 0.5s visible telemetry refresh and keeps full status/history snapshots at 10s'
$reportLines += '- Hardware CI runs a 2Hz /api/live soak to check live polling stability'
$reportLines += '- HTML dashboard includes board speaker playback/self-test controls, alert thresholds and CSV log availability'
$reportLines += '- LittleFS data and config survive a board reboot'
$reportLines += '- Short soak verifies continued sampling, storage writes and heap stability'
$reportLines += '- Host USB camera capture is optional and skipped unless requested'
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
    $serialExcerpt = @(($serialLog -split [Environment]::NewLine) | Where-Object { $_ -match '^\[(NET|URL|LIVE|BOOT)\]' } | Select-Object -Last 8)
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
