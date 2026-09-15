param($port='COM4', $secs=4)
$p = New-Object System.IO.Ports.SerialPort $port,115200,None,8,One
$p.ReadTimeout = 500; $p.DtrEnable = $true; $p.RtsEnable = $true
$p.Open(); $p.DiscardInBuffer()
$end = (Get-Date).AddSeconds($secs); $buf = ''
while ((Get-Date) -lt $end) { Start-Sleep -Milliseconds 200; $buf += $p.ReadExisting() }
$p.Close(); $buf
