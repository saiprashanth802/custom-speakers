using System.Collections.ObjectModel;
using System.IO;
using System.Windows.Threading;
using SpeakerDspDeck.Ble;
using SpeakerDspDeck.Eq;
using SpeakerDspDeck.Model;
using SpeakerDspDeck.Protocol;

namespace SpeakerDspDeck.ViewModels;

public sealed class MainViewModel : Bindable
{
    private readonly BleLink _ble = new();
    private readonly PresetStore _presets = new();

    // Read-back guard: while true, property setters update the UI but do not
    // echo a SET back to the device (the value just came FROM the device).
    private bool _silent;
    private readonly DispatcherTimer _readbackTimer = new() { Interval = TimeSpan.FromSeconds(3) };
    private int _readbackCount;

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

        // Old firmware (no GET_PARAMS) never sends PARAMS_DONE: fall back to the
        // pre-read-back behaviour of pushing the app's state over the device.
        _readbackTimer.Tick += (_, _) =>
        {
            _readbackTimer.Stop();
            AddLog("No read-back from device (old firmware?) — pushing app state instead.");
            PushAll();
        };

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

    // The rate the device reports it is actually running at (EVT_STATUS). It
    // lags the High-Res toggle by the switch time; the firmware pushes a second
    // STATUS once the audio task has completed the switch.
    private uint _deviceRateHz;
    public uint DeviceRateHz
    {
        get => _deviceRateHz;
        private set { if (Set(ref _deviceRateHz, value)) Raise(nameof(DeviceRateText)); }
    }
    public string DeviceRateText => _deviceRateHz == 0 ? "—" : $"{_deviceRateHz / 1000.0:0.#} kHz";

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
        set
        {
            if (Set(ref _profile, value))
            {
                if (!_silent) _ = _ble.Send(Frame.Cmd(CmdOp.SetProfile).U8((int)value));
                Raise(nameof(IsHiRes));
            }
        }
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
        set { if (Set(ref _master, value) && !_silent) _ = _ble.Send(Frame.Cmd(CmdOp.SetMasterGain).F32((float)(value / 100.0))); }
    }

    private bool _muted;
    public bool Muted
    {
        get => _muted;
        set { if (Set(ref _muted, value) && !_silent) _ = _ble.Send(Frame.Cmd(CmdOp.SetMute).Bool(value)); }
    }

    private double _crossoverHz = 2500;
    public double CrossoverHz
    {
        get => _crossoverHz;
        set { if (Set(ref _crossoverHz, value) && !_silent) _ = _ble.Send(Frame.Cmd(CmdOp.SetCrossoverHz).F32((float)value)); }
    }

    private double _preampDb;
    public double VoicingPreampDb
    {
        get => _preampDb;
        set { if (Set(ref _preampDb, value) && !_silent) _ = _ble.Send(Frame.Cmd(CmdOp.SetVoicingPreamp).F32((float)value)); }
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

    // Push the app's full state to the device. Since the read-back landed this
    // is only the fallback for firmware without GET_PARAMS; presets and imports
    // push their own deltas.
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
        {
            _readbackTimer.Stop();
            DeviceRateHz = 0;
            _ble.StartScan();   // device dropped (e.g. rebooted) — look for it again
        }
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
                {
                    AddLog($"Handshake OK (fw {r.U16(0)}). Reading device state…");
                    // Reflect what the device restored from NVS instead of
                    // overwriting it with the app's defaults.
                    _readbackCount = 0;
                    _readbackTimer.Start();
                    _ = _ble.Send(Frame.Cmd(CmdOp.GetParams));
                }
                break;

            case EvtOp.Status:
            {
                var profile = (Profile)r.U8(0);
                bool muted = r.U8(1) != 0;
                uint rate = r.U32(2);
                _silent = true;
                SelectedProfile = profile;
                Muted = muted;
                _silent = false;
                DeviceRateHz = rate;
                AddLog($"Status: profile={profile} muted={(muted ? 1 : 0)} rate={rate} Hz");
                break;
            }

            case EvtOp.Param:
                ApplyParam(r.Inner());
                _readbackCount++;
                break;

            case EvtOp.ParamsDone:
                _readbackTimer.Stop();
                AddLog($"Loaded device state ({_readbackCount} of {r.U8(0)} params).");
                if (_readbackCount != r.U8(0))
                    AddLog("WARNING: some read-back frames were lost; UI may not match the device.");
                break;

            case EvtOp.Ack:
                if (r.U8(1) != 0)
                {
                    if ((CmdOp)r.U8(0) == CmdOp.GetParams && r.U8(1) == 0xFF)
                    {
                        // Firmware predates the read-back: behave as before.
                        _readbackTimer.Stop();
                        AddLog("Firmware has no read-back — pushing app state instead.");
                        PushAll();
                    }
                    else AddLog($"Device NAK on op 0x{r.U8(0):X2} (err {r.U8(1)}).");
                }
                break;
        }
    });

    // One EVT_PARAM = one SET opcode + that opcode's exact payload. Apply it to
    // the UI without echoing it back.
    private void ApplyParam(EvtReader p)
    {
        _silent = true;
        try
        {
            switch ((CmdOp)(byte)p.Op)
            {
                case CmdOp.SetProfile:       SelectedProfile = (Profile)p.U8(0); break;
                case CmdOp.SetMute:          Muted = p.U8(0) != 0; break;
                case CmdOp.SetMasterGain:    MasterPct = Math.Round(p.F32(0) * 100.0, 1); break;
                case CmdOp.SetCrossoverHz:   CrossoverHz = p.F32(0); break;
                case CmdOp.SetVoicingPreamp: VoicingPreampDb = p.F32(0); break;
                case CmdOp.SetVoicingBand:
                {
                    int idx = p.U8(0);
                    if (idx < Voicing.Count)
                        Voicing[idx].LoadSilently(new EqBand
                        {
                            Enabled = p.U8(1) != 0, Type = (FilterType)p.U8(2),
                            F = p.F32(3), Q = p.F32(7), GainDb = p.F32(11)
                        });
                    break;
                }
                case CmdOp.SetDriverLevel:
                {
                    int d = p.U8(0);
                    if (d < Drivers.Count) Drivers[d].LoadLevelSilently(p.F32(1));
                    break;
                }
                case CmdOp.SetDriverDelay:
                {
                    int d = p.U8(0);
                    if (d < Drivers.Count) Drivers[d].LoadDelaySilently(p.U16(1), IsHiRes ? 96000 : 48000);
                    break;
                }
                case CmdOp.SetDriverEqBand:
                {
                    int d = p.U8(0), idx = p.U8(1);
                    if (d < Drivers.Count && idx < Drivers[d].Eq.Count)
                        Drivers[d].Eq[idx].LoadSilently(p.U8(2) != 0, (FilterType)p.U8(3),
                                                        p.F32(4), p.F32(8), p.F32(12));
                    break;
                }
                default:
                    AddLog($"Read-back: unknown param op 0x{(byte)p.Op:X2} ignored.");
                    break;
            }
        }
        finally { _silent = false; }
    }

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
