using System.IO;
using System.Text.Json;

namespace SpeakerDspDeck.Model;

// A full app-side tuning snapshot. Loading a preset pushes these values to the
// device (and updates the UI), so the app stays the source of truth — no device
// read-back needed. Persisted to %USERPROFILE%\SpeakerDspDeck\presets.json.
public sealed class BandDto
{
    public bool Enabled { get; set; }
    public FilterType Type { get; set; } = FilterType.PK;
    public double F { get; set; } = 1000;
    public double Q { get; set; } = 0.707;
    public double GainDb { get; set; }
}

public sealed class PresetSnapshot
{
    public double MasterPct { get; set; } = 50;
    public bool Muted { get; set; }
    public bool IsHiRes { get; set; }
    public double CrossoverHz { get; set; } = 2500;
    public double VoicingPreampDb { get; set; }
    public BandDto[] Voicing { get; set; } = Array.Empty<BandDto>();
    public double[] DriverLevels { get; set; } = Array.Empty<double>();
    public double[] DriverDelaysMs { get; set; } = Array.Empty<double>();
    public BandDto[][] DriverEq { get; set; } = Array.Empty<BandDto[]>();   // [driver][band]
}

public sealed class PresetStore
{
    // Outside %APPDATA% on purpose — a packaged/MSIX launch context virtualizes
    // AppData and would hide the file (the MacroPadDeck launch-context trap).
    private static string Dir =>
        Path.Combine(Environment.GetEnvironmentVariable("USERPROFILE") ?? ".", "SpeakerDspDeck");
    private static string FilePath => Path.Combine(Dir, "presets.json");

    private static readonly JsonSerializerOptions Opts = new() { WriteIndented = true };

    public Dictionary<string, PresetSnapshot> Slots { get; private set; } = new();

    public void Load()
    {
        try
        {
            if (File.Exists(FilePath))
                Slots = JsonSerializer.Deserialize<Dictionary<string, PresetSnapshot>>(
                            File.ReadAllText(FilePath)) ?? new();
        }
        catch { Slots = new(); }   // corrupt file -> start empty, never throw into the UI
    }

    public void Save()
    {
        Directory.CreateDirectory(Dir);
        File.WriteAllText(FilePath, JsonSerializer.Serialize(Slots, Opts));
    }

    public bool Has(string slot) => Slots.ContainsKey(slot);
    public PresetSnapshot? Get(string slot) => Slots.TryGetValue(slot, out var s) ? s : null;
    public void Put(string slot, PresetSnapshot snap) { Slots[slot] = snap; Save(); }
}
