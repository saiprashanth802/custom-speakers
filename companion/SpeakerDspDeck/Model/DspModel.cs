namespace SpeakerDspDeck.Model;

// Mirrors firmware/dsp_engine/dsp_params.h (FilterType, Params). See PROTOCOL.md.
public enum FilterType : byte { PK = 0, LowShelf = 1, HighShelf = 2, LowPass = 3, HighPass = 4 }

public enum Profile : byte { Normal = 0, HiRes = 1 }   // Normal=USB/48k, HiRes=WiFi/96k

public static class Caps
{
    public const int MaxVoicingBands = 10;
    public const int MaxDriverBands  = 4;
    public const int NumDrivers      = 4;   // 0=L-woofer 1=R-woofer 2=L-tweeter 3=R-tweeter
    public static readonly string[] DriverNames =
        { "L-Woofer", "R-Woofer", "L-Tweeter", "R-Tweeter" };
}

public sealed class EqBand
{
    public bool Enabled { get; set; }
    public FilterType Type { get; set; } = FilterType.PK;
    public float F { get; set; } = 1000f;
    public float Q { get; set; } = 0.707f;
    public float GainDb { get; set; }
}

public sealed class DriverCfg
{
    public float LevelDb { get; set; }
    public int DelaySamples { get; set; }
    public EqBand[] Eq { get; } =
        Enumerable.Range(0, Caps.MaxDriverBands).Select(_ => new EqBand()).ToArray();
}
