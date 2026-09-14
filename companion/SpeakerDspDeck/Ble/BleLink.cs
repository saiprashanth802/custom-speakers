using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Security.Cryptography;
using Windows.Storage.Streams;
using SpeakerDspDeck.Protocol;

namespace SpeakerDspDeck.Ble;

// The ONLY class that touches WinRT. Everything else sees plain C# events.
// Scans for a device advertising the DSP service, connects, wires CMD (write)
// and EVT (notify). Mirrors the MacroPadDeck BleLink convention.
public sealed class BleLink
{
    private BluetoothLEAdvertisementWatcher? _watcher;
    private BluetoothLEDevice? _device;
    private GattCharacteristic? _cmd;
    private GattCharacteristic? _evt;

    public event Action<bool>? ConnectionChanged;
    public event Action<byte[]>? EventReceived;
    public event Action<string>? Log;

    public bool IsConnected => _cmd is not null;

    public const string DeviceName = "SpeakerDSP";

    public void StartScan()
    {
        Stop();
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
        if (_device is not null) return;            // already connecting/connected
        bool match = string.Equals(args.Advertisement.LocalName, DeviceName, StringComparison.Ordinal)
                     || args.Advertisement.ServiceUuids.Contains(Proto.Service);
        if (!match) return;

        _watcher?.Stop();
        Log?.Invoke($"Found {DeviceName}, connecting…");
        try { await ConnectTo(args.BluetoothAddress); }
        catch (Exception ex) { Log?.Invoke($"Connect failed: {ex.Message}"); StartScan(); }
    }

    private async Task ConnectTo(ulong address)
    {
        _device = await BluetoothLEDevice.FromBluetoothAddressAsync(address);
        if (_device is null) { Log?.Invoke("Device handle null"); return; }
        _device.ConnectionStatusChanged += OnConnStatus;

        var svc = await _device.GetGattServicesForUuidAsync(Proto.Service);
        if (svc.Status != GattCommunicationStatus.Success || svc.Services.Count == 0)
        { Log?.Invoke("DSP service not found"); return; }

        var s = svc.Services[0];
        _cmd = (await s.GetCharacteristicsForUuidAsync(Proto.CmdChar)).Characteristics.FirstOrDefault();
        _evt = (await s.GetCharacteristicsForUuidAsync(Proto.EvtChar)).Characteristics.FirstOrDefault();
        if (_cmd is null || _evt is null) { Log?.Invoke("CMD/EVT characteristic missing"); return; }

        _evt.ValueChanged += OnEvt;
        var cfg = await _evt.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue.Notify);
        if (cfg != GattCommunicationStatus.Success)
            Log?.Invoke("Warning: could not subscribe to notifications (may need re-pair)");

        Log?.Invoke("Connected.");
        ConnectionChanged?.Invoke(true);
    }

    private void OnConnStatus(BluetoothLEDevice sender, object args)
    {
        if (sender.ConnectionStatus == BluetoothConnectionStatus.Disconnected)
        {
            Log?.Invoke("Disconnected.");
            _cmd = null; _evt = null;
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
        if (_cmd is null) return;
        var buf = CryptographicBuffer.CreateFromByteArray(frame);
        var mode = _cmd.CharacteristicProperties.HasFlag(GattCharacteristicProperties.WriteWithoutResponse)
            ? GattWriteOption.WriteWithoutResponse : GattWriteOption.WriteWithResponse;
        try { await _cmd.WriteValueAsync(buf, mode); }
        catch (Exception ex) { Log?.Invoke($"Write failed: {ex.Message}"); }
    }

    public Task Send(Frame f) => SendAsync(f.ToArray());

    public void Stop()
    {
        try { _watcher?.Stop(); } catch { }
        _watcher = null;
    }

    public void Disconnect()
    {
        Stop();
        if (_evt is not null) _evt.ValueChanged -= OnEvt;
        _device?.Dispose();
        _device = null; _cmd = null; _evt = null;
        ConnectionChanged?.Invoke(false);
    }
}
