using System.Collections.ObjectModel;
using System.IO;
using SpeakerDspDeck.Ble;
using SpeakerDspDeck.Eq;
using SpeakerDspDeck.Model;
using SpeakerDspDeck.Protocol;

namespace SpeakerDspDeck.ViewModels;

public sealed class MainViewModel : Bindable
{
    private readonly BleLink _ble = new();
    private readonly PresetStore _presets = new();

    public ObservableCollection<BandRow> Voicing { get; } = new();
    public ObservableCollection<DriverRow> Drivers { get; } = new();
    public ObservableCollection<PresetSlotVM> Presets { get; } = new();
    public ObservableCollection<string> LogLines { get; } = new();

    public RelayCommand ConnectCommand { get; }
    public RelayCommand ImportEqCommand { get; }
    public RelayCommand SaveNvsCommand { get; }

    public MainViewModel()
    {
        for (int i = 0; i < Caps.MaxVoicingBands; i++)
            Voicing.Add(new BandRow(i, f => _ble.Send(f)));
        for (int i = 0; i < Caps.NumDrivers; i++)
            Drivers.Add(new DriverRow(i, f => _ble.Send(f), () => IsHiRes ? 96000 : 48000));

        _presets.Load();
        foreach (var name in new[] { "A", "B", "C", "D" })
            Presets.Add(new PresetSlotVM(name, SavePreset, LoadPreset, _presets.Has(name)));

        ConnectCommand = new RelayCommand(() => _ble.StartScan(), () => !Connected);
        ImportEqCommand = new RelayCommand(ImportEq, () => Connected);
        SaveNvsCommand = new RelayCommand(async () =>
            await _ble.Send(Frame.Cmd(CmdOp.SaveToNvs)), () => Connected);

        _ble.Log += AddLog;
        _ble.ConnectionChanged += OnConnChanged;
        _ble.EventReceived += OnEvt;

        _ble.StartScan();   // auto-connect on launch
    }

    // ---- Presets: capture the whole UI state / apply it back --------------
    private PresetSnapshot CaptureSnapshot() => new()
    {
        MasterPct = MasterPct, Muted = Muted, IsHiRes = IsHiRes,
        CrossoverHz = CrossoverHz, VoicingPreampDb = VoicingPreampDb,
        Voicing = Voicing.Select(b => new BandDto
            { Enabled = b.Enabled, Type = b.Type, F = b.F, Q = b.Q, GainDb = b.GainDb }).ToArray(),
        DriverLevels = Drivers.Select(d => d.LevelDb).ToArray(),
        DriverDelaysMs = Drivers.Select(d => d.DelayMs).ToArray(),
        DriverEq = Drivers.Select(d => d.Eq.Select(b => new BandDto
            { Enabled = b.Enabled, Type = b.Type, F = b.F, Q = b.Q, GainDb = b.GainDb }).ToArray()).ToArray(),
    };

    private async void ApplySnapshot(PresetSnapshot s)
    {
        // Setting these properties sends the corresponding commands to the device.
        IsHiRes = s.IsHiRes;
        Muted = s.Muted;
        MasterPct = s.MasterPct;
        CrossoverHz = s.CrossoverHz;
        VoicingPreampDb = s.VoicingPreampDb;
        for (int i = 0; i < Voicing.Count && i < s.Voicing.Length; i++)
        {
            var d = s.Voicing[i];
            Voicing[i].LoadSilently(new Model.EqBand
                { Enabled = d.Enabled, Type = d.Type, F = (float)d.F, Q = (float)d.Q, GainDb = (float)d.GainDb });
            await _ble.Send(Voicing[i].BuildFrame());
        }
        for (int i = 0; i < Drivers.Count && i < s.DriverLevels.Length; i++)
            Drivers[i].LevelDb = s.DriverLevels[i];
        for (int i = 0; i < Drivers.Count && i < s.DriverDelaysMs.Length; i++)
            Drivers[i].DelayMs = s.DriverDelaysMs[i];
        for (int i = 0; i < Drivers.Count && i < s.DriverEq.Length; i++)
            for (int j = 0; j < Drivers[i].Eq.Count && j < s.DriverEq[i].Length; j++)
            {
                var d = s.DriverEq[i][j];
                Drivers[i].Eq[j].LoadSilently(d.Enabled, d.Type, d.F, d.Q, d.GainDb);
                await _ble.Send(Drivers[i].Eq[j].BuildFrame());
            }
    }

    private void SavePreset(string slot)
    {
        _presets.Put(slot, CaptureSnapshot());
        foreach (var p in Presets) if (p.Slot == slot) p.HasData = true;
        AddLog($"Saved preset {slot}.");
    }

    private void LoadPreset(string slot)
    {
        var s = _presets.Get(slot);
        if (s is null) return;
        ApplySnapshot(s);
        AddLog($"Loaded preset {slot}.");
    }

    private bool _connected;
    public bool Connected
    {
        get => _connected;
        private set { if (Set(ref _connected, value)) { Raise(nameof(StatusText)); RaiseCommands(); } }
    }

    public string StatusText => Connected ? "Connected" : "Not connected";

    // Which driver's level/delay/EQ the per-driver panel is editing.
    private int _selDriver;
    public int SelectedDriverIndex
    {
        get => _selDriver;
        set { if (Set(ref _selDriver, value < 0 ? 0 : value)) Raise(nameof(SelectedDriver)); }
    }
    public DriverRow SelectedDriver => Drivers[_selDriver < 0 ? 0 : _selDriver];

    private Profile _profile = Profile.Normal;
    public Profile SelectedProfile
    {
        get => _profile;
        set { if (Set(ref _profile, value)) { _ = _ble.Send(Frame.Cmd(CmdOp.SetProfile).U8((int)value)); Raise(nameof(IsHiRes)); } }
    }

    // Bound to the High-Res toggle in the UI (avoids an enum<->bool converter).
    public bool IsHiRes
    {
        get => _profile == Profile.HiRes;
        set => SelectedProfile = value ? Profile.HiRes : Profile.Normal;
    }

    // 0..100 for the slider; sent as 0..1 linear.
    private double _master = 50;
    public double MasterPct
    {
        get => _master;
        set { if (Set(ref _master, value)) _ = _ble.Send(Frame.Cmd(CmdOp.SetMasterGain).F32((float)(value / 100.0))); }
    }

    private bool _muted;
    public bool Muted
    {
        get => _muted;
        set { if (Set(ref _muted, value)) _ = _ble.Send(Frame.Cmd(CmdOp.SetMute).Bool(value)); }
    }

    private double _crossoverHz = 2500;
    public double CrossoverHz
    {
        get => _crossoverHz;
        set { if (Set(ref _crossoverHz, value)) _ = _ble.Send(Frame.Cmd(CmdOp.SetCrossoverHz).F32((float)value)); }
    }

    private double _preampDb;
    public double VoicingPreampDb
    {
        get => _preampDb;
        set { if (Set(ref _preampDb, value)) _ = _ble.Send(Frame.Cmd(CmdOp.SetVoicingPreamp).F32((float)value)); }
    }

    private async void ImportEq()
    {
        var dlg = new Microsoft.Win32.OpenFileDialog
        {
            Title = "Import Peace / AutoEQ / Equalizer APO preset",
            Filter = "EQ preset (*.txt)|*.txt|All files|*.*"
        };
        if (dlg.ShowDialog() != true) return;

        var res = EqualizerApoImporter.Parse(File.ReadAllText(dlg.FileName));
        foreach (var w in res.Warnings) AddLog("Import: " + w);

        await _ble.Send(Frame.Cmd(CmdOp.ClearVoicing));
        VoicingPreampDb = res.PreampDb;

        for (int i = 0; i < Voicing.Count; i++)
        {
            if (i < res.Bands.Count)
            {
                Voicing[i].LoadSilently(res.Bands[i]);
                await _ble.Send(Voicing[i].BuildFrame());
            }
            else
            {
                Voicing[i].LoadSilently(new EqBand { Enabled = false });
                await _ble.Send(Voicing[i].BuildFrame());
            }
        }
        AddLog($"Imported {res.Bands.Count} band(s), preamp {res.PreampDb:0.0} dB from {Path.GetFileName(dlg.FileName)}.");
    }

    // Re-push the app's full state to the device on connect (MacroPadDeck
    // convention) so the device always matches the UI after any (re)connect.
    private async void PushAll()
    {
        await _ble.Send(Frame.Cmd(CmdOp.SetMasterGain).F32((float)(MasterPct / 100.0)));
        await _ble.Send(Frame.Cmd(CmdOp.SetMute).Bool(Muted));
        await _ble.Send(Frame.Cmd(CmdOp.SetCrossoverHz).F32((float)CrossoverHz));
        await _ble.Send(Frame.Cmd(CmdOp.SetVoicingPreamp).F32((float)VoicingPreampDb));
        foreach (var b in Voicing) await _ble.Send(b.BuildFrame());
        foreach (var d in Drivers)
        {
            await _ble.Send(d.BuildLevelFrame());
            await _ble.Send(d.BuildDelayFrame());
            foreach (var eb in d.Eq) await _ble.Send(eb.BuildFrame());
        }
        AddLog("Pushed current state to device.");
    }

    private void OnConnChanged(bool up) => App.OnUi(() =>
    {
        Connected = up;
        if (up)
            // Subscribed now; pull HELLO (the device's connect-time HELLO races
            // ahead of Windows' CCCD subscribe and is usually missed).
            _ = _ble.Send(Frame.Cmd(CmdOp.Hello));
        else
            _ble.StartScan();   // device dropped (e.g. rebooted) — look for it again
    });

    private void OnEvt(byte[] data) => App.OnUi(() =>
    {
        var r = new EvtReader(data);
        switch (r.Op)
        {
            case EvtOp.Hello:
                ushort proto = r.U16(2);
                if (proto != Proto.ProtocolVersion)
                    AddLog($"WARNING: protocol mismatch (device {proto}, app {Proto.ProtocolVersion}).");
                else
                { AddLog("Handshake OK."); PushAll(); }
                break;
            case EvtOp.Status:
                AddLog($"Status: profile={(Profile)r.U8(0)} muted={r.U8(1)} rate={r.U32(2)}Hz");
                break;
            case EvtOp.Ack:
                if (r.U8(1) != 0) AddLog($"Device NAK on op 0x{r.U8(0):X2} (err {r.U8(1)}).");
                break;
        }
    });

    private void AddLog(string s) => App.OnUi(() =>
    {
        LogLines.Insert(0, $"{DateTime.Now:HH:mm:ss}  {s}");
        while (LogLines.Count > 200) LogLines.RemoveAt(LogLines.Count - 1);
    });

    private void RaiseCommands()
    {
        ConnectCommand.RaiseCanExecuteChanged();
        ImportEqCommand.RaiseCanExecuteChanged();
        SaveNvsCommand.RaiseCanExecuteChanged();
    }
}
