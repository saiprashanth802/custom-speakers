using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Security.Cryptography;
using Windows.Storage.Streams;
using SpeakerDspDeck.Protocol;

namespace SpeakerDspDeck.Ble;

// The ONLY class that touches WinRT. Everything else sees plain C# events.
// Scans for a device advertising the DSP service, connects, wires CMD (write)
// and EVT (notify). Mirrors the MacroPadDeck BleLink convention:
//   * Drop() before every attempt — a BluetoothLEDevice left over from a
//     previous connection blocks the next one (the advert handler skipped every
//     advert while _device was non-null, so a rebooted board was never re-found).
//   * one attempt at a time (_gate) — adverts arrive faster than a connect.
//   * uncached enumeration, and dispose every service we are NOT keeping —
//     WinRT holds a session per undisposed GattDeviceService and leaked
//     sessions eventually lock the service.
//   * every failure path drops and rescans; only success stays put.
public sealed class BleLink
{
    private BluetoothLEAdvertisementWatcher? _watcher;
    private BluetoothLEDevice? _device;
    private GattDeviceService? _svc;
    private GattCharacteristic? _cmd;
    private GattCharacteristic? _evt;
    private readonly SemaphoreSlim _gate = new(1, 1);
    private bool _up;

    public event Action<bool>? ConnectionChanged;
    public event Action<byte[]>? EventReceived;
    public event Action<string>? Log;

    public bool IsConnected => _up;

    public const string DeviceName = "SpeakerDSP";

    public void StartScan()
    {
        Stop();
        Drop();
        // Match by name (not a UUID advert filter): a 128-bit service UUID + the
        // name overflow the 31-byte advert, so the UUID may not be in the packet.
        _watcher = new BluetoothLEAdvertisementWatcher { ScanningMode = BluetoothLEScanningMode.Active };
        _watcher.Received += OnAdvReceived;
        _watcher.Start();
        Log?.Invoke("Scanning for DSP device…");
    }

    private async void OnAdvReceived(BluetoothLEAdvertisementWatcher sender,
                                     BluetoothLEAdvertisementReceivedEventArgs args)
    {
        bool match = string.Equals(args.Advertisement.LocalName, DeviceName, StringComparison.Ordinal)
                     || args.Advertisement.ServiceUuids.Contains(Proto.Service);
        if (!match) return;
        if (!await _gate.WaitAsync(0)) return;          // an attempt is already running
        try
        {
            Stop();
            Log?.Invoke($"Found {DeviceName}, connecting…");
            string? fail;
            try { fail = await ConnectTo(args.BluetoothAddress); }
            catch (Exception ex) { fail = $"Connect failed: {ex.Message} (0x{ex.HResult:X8})"; }
            if (fail is null) return;
            Log?.Invoke(fail);
            Drop();
            await Task.Delay(500);                      // let the stack settle; no hot loop
            StartScan();
        }
        finally { _gate.Release(); }
    }

    // Returns null on success, otherwise the reason (caller drops + rescans).
    private async Task<string?> ConnectTo(ulong address)
    {
        Drop();
        _device = await BluetoothLEDevice.FromBluetoothAddressAsync(address);
        if (_device is null) return "Device handle null";
        _device.ConnectionStatusChanged += OnConnStatus;

        // Full uncached enumeration: the by-UUID query can answer from Windows'
        // cache of the previous session (stale after a reboot) or throw
        // ERROR_BAD_COMMAND on this stack.
        var svc = await _device.GetGattServicesAsync(BluetoothCacheMode.Uncached);
        if (svc.Status != GattCommunicationStatus.Success) return $"Services: {svc.Status}";
        foreach (var x in svc.Services)
        {
            if (_svc is null && x.Uuid == Proto.Service) _svc = x;
            else x.Dispose();
        }
        if (_svc is null) return "DSP service not found";

        var chars = await _svc.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
        if (chars.Status != GattCommunicationStatus.Success) return $"Characteristics: {chars.Status}";
        _cmd = chars.Characteristics.FirstOrDefault(c => c.Uuid == Proto.CmdChar);
        _evt = chars.Characteristics.FirstOrDefault(c => c.Uuid == Proto.EvtChar);
        if (_cmd is null || _evt is null) return "CMD/EVT characteristic missing";

        // Handler BEFORE subscribe, and cycle the CCCD off→on: it persists per
        // bond, and rewriting the same value never fires the device's onSubscribe.
        _evt.ValueChanged += OnEvt;
        await _evt.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue.None);
        var cfg = await _evt.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue.Notify);
        if (cfg != GattCommunicationStatus.Success) return $"Subscribe: {cfg}";

        Log?.Invoke("Connected.");
        _up = true;
        ConnectionChanged?.Invoke(true);
        return null;
    }

    private void OnConnStatus(BluetoothLEDevice sender, object args)
    {
        // Only flag it here; the teardown happens in StartScan()/Drop(), off the
        // device's own event callback (the view-model rescans on false).
        if (sender.ConnectionStatus == BluetoothConnectionStatus.Disconnected && _up)
        {
            _up = false;
            Log?.Invoke("Disconnected.");
            ConnectionChanged?.Invoke(false);
        }
    }

    private void OnEvt(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        var data = new byte[args.CharacteristicValue.Length];
        using var r = DataReader.FromBuffer(args.CharacteristicValue);
        r.ReadBytes(data);
        EventReceived?.Invoke(data);
    }

    public async Task SendAsync(byte[] frame)
    {
        var ch = _cmd;
        if (!_up || ch is null) return;
        var buf = CryptographicBuffer.CreateFromByteArray(frame);
        var mode = ch.CharacteristicProperties.HasFlag(GattCharacteristicProperties.WriteWithoutResponse)
            ? GattWriteOption.WriteWithoutResponse : GattWriteOption.WriteWithResponse;
        try { await ch.WriteValueAsync(buf, mode); }
        catch (Exception ex)
        {
            // A write that throws means the link is gone even if the status
            // event has not fired yet; report it so the rescan starts.
            Log?.Invoke($"Write failed: {ex.Message}");
            if (_up) { _up = false; ConnectionChanged?.Invoke(false); }
        }
    }

    public Task Send(Frame f) => SendAsync(f.ToArray());

    public void Stop()
    {
        try { _watcher?.Stop(); } catch { }
        _watcher = null;
    }

    private void Drop()
    {
        _up = false;
        if (_evt is not null) { try { _evt.ValueChanged -= OnEvt; } catch { } }
        _evt = null; _cmd = null;
        if (_svc is not null) { try { _svc.Dispose(); } catch { } }
        _svc = null;
        if (_device is not null)
        {
            try { _device.ConnectionStatusChanged -= OnConnStatus; _device.Dispose(); } catch { }
        }
        _device = null;
    }

    public void Disconnect()
    {
        Stop();
        bool wasUp = _up;
        Drop();
        if (wasUp) ConnectionChanged?.Invoke(false);
    }
}
