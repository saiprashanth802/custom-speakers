param($port='COM4')
# Drive the arduino-esp32 USBCDC reboot state machine: (!dtr,rts) -> (dtr,rts) -> (dtr,!rts) -> (!dtr,!rts)
$p = New-Object System.IO.Ports.SerialPort $port,115200,None,8,One
$p.DtrEnable = $false; $p.RtsEnable = $true; $p.Open(); Start-Sleep -Milliseconds 100
$p.DtrEnable = $true;  Start-Sleep -Milliseconds 100
$p.RtsEnable = $false; Start-Sleep -Milliseconds 100
$p.DtrEnable = $false; Start-Sleep -Milliseconds 100
try { $p.Close() } catch {}
