using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using SpeakerDspDeck.Model;
using SpeakerDspDeck.Protocol;

namespace SpeakerDspDeck.ViewModels;

public abstract class Bindable : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;
    protected void Raise([CallerMemberName] string? n = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
    protected bool Set<T>(ref T field, T value, [CallerMemberName] string? n = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value; Raise(n); return true;
    }
}

// One voicing EQ band row. On any edit it sends SET_VOICING_BAND for its index.
public sealed class BandRow : Bindable
{
    private readonly int _idx;
    private readonly Func<Frame, Task> _send;
    private bool _suppress;

    public BandRow(int idx, Func<Frame, Task> send) { _idx = idx; _send = send; }
    public int Index => _idx;
    public string Label => $"Band {_idx + 1}";

    public static IReadOnlyList<FilterType> Types { get; } =
        new[] { FilterType.PK, FilterType.LowShelf, FilterType.HighShelf };

    private bool _enabled;
    public bool Enabled { get => _enabled; set { if (Set(ref _enabled, value)) Push(); } }
    private FilterType _type = FilterType.PK;
    public FilterType Type { get => _type; set { if (Set(ref _type, value)) Push(); } }
    private double _f = 1000;
    public double F { get => _f; set { if (Set(ref _f, value)) Push(); } }
    private double _q = 0.707;
    public double Q { get => _q; set { if (Set(ref _q, value)) Push(); } }
    private double _gainDb;
    public double GainDb { get => _gainDb; set { if (Set(ref _gainDb, value)) Push(); } }

    // Load from an imported band without firing 5 BLE writes.
    public void LoadSilently(EqBand b)
    {
        _suppress = true;
        Enabled = b.Enabled; Type = b.Type; F = b.F; Q = b.Q; GainDb = b.GainDb;
        _suppress = false;
    }

    public Frame BuildFrame() => Frame.Cmd(CmdOp.SetVoicingBand)
        .U8(_idx).Bool(_enabled).U8((int)_type).F32((float)_f).F32((float)_q).F32((float)_gainDb);

    private async void Push() { if (!_suppress) await _send(BuildFrame()); }
}

// One per-driver EQ band. Sends SET_DRIVER_EQ_BAND for its (driver, band).
public sealed class DriverBandRow : Bindable
{
    private readonly int _drv, _idx;
    private readonly Func<Frame, Task> _send;
    private bool _suppress;

    public DriverBandRow(int drv, int idx, Func<Frame, Task> send) { _drv = drv; _idx = idx; _send = send; }
    public string Label => $"Band {_idx + 1}";
    public static IReadOnlyList<FilterType> Types => BandRow.Types;

    private bool _enabled;
    public bool Enabled { get => _enabled; set { if (Set(ref _enabled, value)) Push(); } }
    private FilterType _type = FilterType.PK;
    public FilterType Type { get => _type; set { if (Set(ref _type, value)) Push(); } }
    private double _f = 1000;
    public double F { get => _f; set { if (Set(ref _f, value)) Push(); } }
    private double _q = 0.707;
    public double Q { get => _q; set { if (Set(ref _q, value)) Push(); } }
    private double _gainDb;
    public double GainDb { get => _gainDb; set { if (Set(ref _gainDb, value)) Push(); } }

    public void LoadSilently(bool en, FilterType t, double f, double q, double g)
    { _suppress = true; Enabled = en; Type = t; F = f; Q = q; GainDb = g; _suppress = false; }

    public Frame BuildFrame() => Frame.Cmd(CmdOp.SetDriverEqBand)
        .U8(_drv).U8(_idx).Bool(_enabled).U8((int)_type).F32((float)_f).F32((float)_q).F32((float)_gainDb);

    private async void Push() { if (!_suppress) await _send(BuildFrame()); }
}

// One output driver: level trim + time-align delay + a small EQ bank.
public sealed class DriverRow : Bindable
{
    private readonly int _idx;
    private readonly Func<Frame, Task> _send;
    private readonly Func<int> _rateHz;   // current sample rate, for ms<->samples

    public DriverRow(int idx, Func<Frame, Task> send, Func<int> rateHz)
    {
        _idx = idx; _send = send; _rateHz = rateHz;
        for (int i = 0; i < Caps.MaxDriverBands; i++) Eq.Add(new DriverBandRow(idx, i, send));
    }

    public int Index => _idx;
    public string Name => Caps.DriverNames[_idx];
    public ObservableCollection<DriverBandRow> Eq { get; } = new();

    private bool _suppress;

    private double _levelDb;
    public double LevelDb
    {
        get => _levelDb;
        set { if (Set(ref _levelDb, value) && !_suppress) _ = _send(BuildLevelFrame()); }
    }

    // Time alignment. Shown in ms; sent as sample count for the current rate.
    private double _delayMs;
    public double DelayMs
    {
        get => _delayMs;
        set { if (Set(ref _delayMs, value) && !_suppress) _ = _send(BuildDelayFrame()); }
    }

    // Read-back from the device: update the UI without echoing a SET.
    public void LoadLevelSilently(double dB) { _suppress = true; LevelDb = dB; _suppress = false; }
    public void LoadDelaySilently(int samples, int rateHz)
    { _suppress = true; DelayMs = samples * 1000.0 / rateHz; _suppress = false; }

    public Frame BuildLevelFrame() =>
        Frame.Cmd(CmdOp.SetDriverLevel).U8(_idx).F32((float)_levelDb);

    public Frame BuildDelayFrame()
    {
        int samples = (int)Math.Round(_delayMs * _rateHz() / 1000.0);
        if (samples < 0) samples = 0; if (samples > 1000) samples = 1000;
        return Frame.Cmd(CmdOp.SetDriverDelay).U8(_idx).U16(samples);
    }
}
