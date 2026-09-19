using System.Globalization;
using System.Text.RegularExpressions;
using SpeakerDspDeck.Model;

namespace SpeakerDspDeck.Eq;

// Parses Equalizer APO / Peace / AutoEQ parametric config text into voicing bands.
// Supports: Preamp, and Filter lines of type PK, LS/LSC (low-shelf), HS/HSC
// (high-shelf). Unknown types are skipped with a warning. See PROTOCOL.md.
public static class EqualizerApoImporter
{
    public sealed record Result(float PreampDb, List<EqBand> Bands, List<string> Warnings);

    private static readonly Regex PreampRx =
        new(@"^\s*Preamp:\s*(-?\d+(?:\.\d+)?)\s*dB", RegexOptions.IgnoreCase);

    // Filter 1: ON PK Fc 105 Hz Gain 5.5 dB Q 0.70
    private static readonly Regex FilterRx = new(
        @"^\s*Filter\s+\d+:\s*(ON|OFF)\s+([A-Za-z]+)\s+Fc\s+(\d+(?:\.\d+)?)\s*Hz" +
        @"(?:\s+Gain\s+(-?\d+(?:\.\d+)?)\s*dB)?(?:\s+Q\s+(\d+(?:\.\d+)?))?",
        RegexOptions.IgnoreCase);

    public static Result Parse(string text)
    {
        float preamp = 0f;
        var bands = new List<EqBand>();
        var warnings = new List<string>();
        var ci = CultureInfo.InvariantCulture;

        // A graphic-EQ curve (Audacity / REW "FilterCurve" export) is not parametric:
        // fit it to the ten bands instead of importing nothing (2026-09-15: the user's
        // speaker measurement came in this form and produced "0 band(s)").
        if (FilterCurveFitter.IsFilterCurve(text))
        {
            var curve = FilterCurveFitter.ParseCurve(text);
            if (curve is null)
            {
                warnings.Add("FilterCurve has fewer than four points.");
                return new Result(0f, bands, warnings);
            }
            var fit = FilterCurveFitter.FitBands(curve.Value.f, curve.Value.v, Caps.MaxVoicingBands);
            // Keep the loudest point of the fitted curve at or below 0 dBFS.
            double peak = curve.Value.v.Zip(curve.Value.f).Where(p => p.Second >= 30).Max(p => p.First);
            preamp = (float)Math.Round(Math.Min(0, -peak), 1);
            warnings.Add($"Fitted {curve.Value.f.Length}-point curve with {fit.Bands.Count} band(s): " +
                         $"RMS {fit.RmsDb:0.00} dB, worst {fit.MaxAbsDb:0.0} dB at {fit.MaxAtHz:0} Hz (30 Hz–18 kHz).");
            return new Result(preamp, fit.Bands, warnings);
        }

        foreach (var raw in text.Split('\n'))
        {
            var line = raw.Trim();
            if (line.Length == 0 || line.StartsWith('#')) continue;

            var pm = PreampRx.Match(line);
            if (pm.Success)
            {
                preamp = float.Parse(pm.Groups[1].Value, ci);
                continue;
            }

            var fm = FilterRx.Match(line);
            if (!fm.Success) continue;

            bool on = fm.Groups[1].Value.Equals("ON", StringComparison.OrdinalIgnoreCase);
            string type = fm.Groups[2].Value.ToUpperInvariant();
            float fc = float.Parse(fm.Groups[3].Value, ci);
            float gain = fm.Groups[4].Success ? float.Parse(fm.Groups[4].Value, ci) : 0f;
            float q = fm.Groups[5].Success ? float.Parse(fm.Groups[5].Value, ci) : 0.707f;

            FilterType? ft = type switch
            {
                "PK" or "PEQ"          => FilterType.PK,
                "LS" or "LSC" or "LSQ" => FilterType.LowShelf,
                "HS" or "HSC" or "HSQ" => FilterType.HighShelf,
                _ => null
            };

            if (ft is null)
            {
                warnings.Add($"Skipped unsupported filter type '{type}' at {fc:0} Hz.");
                continue;
            }

            bands.Add(new EqBand
            {
                Enabled = on, Type = ft.Value, F = fc, Q = q, GainDb = gain
            });
        }

        if (bands.Count > Caps.MaxVoicingBands)
        {
            warnings.Add(
                $"{bands.Count} bands parsed but only {Caps.MaxVoicingBands} voicing " +
                $"slots exist — keeping the first {Caps.MaxVoicingBands}.");
            bands = bands.Take(Caps.MaxVoicingBands).ToList();
        }

        return new Result(preamp, bands, warnings);
    }
}
