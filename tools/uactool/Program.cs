// uactool — probe and drive the "Speaker DSP" USB audio device from Windows.
//   uactool probe                      list formats the driver accepts in exclusive mode
//   uactool play <rate> <bits> <secs>  play a 1 kHz sine at -30 dBFS in EXCLUSIVE mode
//   uactool shared <secs>              play through the shared-mode mix (Windows' format)
using NAudio.CoreAudioApi;
using NAudio.Wave;

var en = new MMDeviceEnumerator();
var all = en.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.All)
            .ToList();
foreach (var d in all) Console.WriteLine($"  endpoint: {d.FriendlyName} [{d.State}]");
var dev = all.FirstOrDefault(d => d.FriendlyName.Contains("Speaker DSP") && d.State == DeviceState.Active);
if (dev == null) { Console.WriteLine("Speaker DSP endpoint not found"); return 2; }
Console.WriteLine($"device: {dev.FriendlyName}  state={dev.State}");
using (var ac = dev.AudioClient) Console.WriteLine($"mix format (shared): {ac.MixFormat}");

string cmd = args.Length > 0 ? args[0] : "probe";
if (cmd == "probe") {
    foreach (var rate in new[] { 44100, 48000, 88200, 96000, 192000 })
    foreach (var bits in new[] { 16, 24, 32 }) {
        var wf = WaveFormat.CreateCustomFormat(WaveFormatEncoding.Extensible, rate, 2, rate * 2 * bits / 8, 2 * bits / 8, bits);
        var wfe = new WaveFormatExtensible(rate, bits, 2);
        using var ac = dev.AudioClient;
        bool ok = ac.IsFormatSupported(AudioClientShareMode.Exclusive, wfe, out var closest);
        Console.WriteLine($"  exclusive {rate,6} Hz {bits,2}-bit: {(ok ? "OK" : "no")}{(closest != null ? "  closest " + closest : "")}");
    }
    return 0;
}

int secs = int.Parse(args[cmd == "play" ? 3 : 1]);
IWaveProvider src;
WasapiOut wo;
if (cmd == "play") {
    int rate = int.Parse(args[1]), bits = int.Parse(args[2]);
    var fmt = new WaveFormatExtensible(rate, bits, 2);
    src = new Sine(fmt, 1000.0, dbfs: -30);
    wo = new WasapiOut(dev, AudioClientShareMode.Exclusive, true, 20);
} else {
    using var ac = dev.AudioClient;
    var mix = ac.MixFormat;
    src = new Sine(WaveFormat.CreateIeeeFloatWaveFormat(mix.SampleRate, mix.Channels), 1000.0, dbfs: -30);
    wo = new WasapiOut(dev, AudioClientShareMode.Shared, true, 20);
}
wo.Init(src);
Console.WriteLine($"playing {src.WaveFormat} for {secs} s, mode {(cmd == "play" ? "exclusive" : "shared")}");
wo.Play();
Thread.Sleep(secs * 1000);
wo.Stop();
wo.Dispose();
Console.WriteLine("stopped");
return 0;

// Raw sine generator that writes whatever PCM/float container the format asks for.
sealed class Sine : IWaveProvider {
    readonly WaveFormat _fmt; readonly double _f, _amp; double _ph;
    public Sine(WaveFormat fmt, double hz, double dbfs) { _fmt = fmt; _f = hz; _amp = Math.Pow(10, dbfs / 20); }
    public WaveFormat WaveFormat => _fmt;
    public int Read(byte[] buf, int off, int count) {
        int bps = _fmt.BitsPerSample / 8, ch = _fmt.Channels, frame = bps * ch, n = count / frame;
        for (int i = 0; i < n; i++) {
            double s = Math.Sin(_ph) * _amp; _ph += 2 * Math.PI * _f / _fmt.SampleRate; if (_ph > 2 * Math.PI) _ph -= 2 * Math.PI;
            for (int c = 0; c < ch; c++) {
                int o = off + (i * ch + c) * bps;
                if (_fmt.Encoding == WaveFormatEncoding.IeeeFloat) BitConverter.TryWriteBytes(buf.AsSpan(o), (float)s);
                else { long v = (long)(s * ((1L << (_fmt.BitsPerSample - 1)) - 1)); for (int b = 0; b < bps; b++) buf[o + b] = (byte)(v >> (8 * b)); }
            }
        }
        return n * frame;
    }
}
