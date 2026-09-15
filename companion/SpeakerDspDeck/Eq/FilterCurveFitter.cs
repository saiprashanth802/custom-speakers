using System.Globalization;
using System.Text.RegularExpressions;
using SpeakerDspDeck.Model;

namespace SpeakerDspDeck.Eq;

// Fits a graphic-EQ target curve to at most Caps.MaxVoicingBands parametric bands.
//
// Input is the Audacity / REW "FilterCurve" export — f0="10" ... v0="-22.3" ... — a
// list of (Hz, dB) points. It is what a speaker measurement tends to produce, and it
// is not parametric, so it cannot be sent to the device as it is: the voicing EQ is
// ten RBJ biquads. This turns the curve into PK / low-shelf / high-shelf bands the
// same way AutoEQ does, only simpler: greedy placement on the largest residual, then
// coordinate-descent refinement of every band's F, Q and gain against the target,
// weighted to 30 Hz–18 kHz (below that the woofers do not go, above it the curve is
// noise). The response model is the app's own RBJ design (Model/Response.cs), so
// what the fit predicts is what the device will do.
//
// Typical result on a 50-point measured curve: RMS 0.3–0.6 dB, worst point ~1.5 dB,
// in a few hundred milliseconds. The residual numbers go into the import log.
public static class FilterCurveFitter
{
    public sealed record Fit(List<EqBand> Bands, double RmsDb, double MaxAbsDb, double MaxAtHz);

    private const double Fs = 48000;            // design rate; 96 k differs by < 0.1 dB below 10 kHz
    private const double WMin = 30, WMax = 18000;

    private static readonly Regex PairRx = new(@"\b([fv])(\d+)=""(-?\d+(?:\.\d+)?)""", RegexOptions.IgnoreCase);

    public static bool IsFilterCurve(string text) => text.TrimStart().StartsWith("FilterCurve:", StringComparison.OrdinalIgnoreCase);

    // Returns null if the text does not carry at least four (f, v) pairs.
    public static (double[] f, double[] v)? ParseCurve(string text)
    {
        var f = new SortedDictionary<int, double>();
        var v = new SortedDictionary<int, double>();
        foreach (Match m in PairRx.Matches(text))
        {
            int i = int.Parse(m.Groups[2].Value);
            double x = double.Parse(m.Groups[3].Value, CultureInfo.InvariantCulture);
            if (m.Groups[1].Value.Equals("f", StringComparison.OrdinalIgnoreCase)) f[i] = x; else v[i] = x;
        }
        var keys = f.Keys.Where(v.ContainsKey).ToArray();
        if (keys.Length < 4) return null;
        return (keys.Select(k => f[k]).ToArray(), keys.Select(k => v[k]).ToArray());
    }

    public static Fit FitBands(double[] fHz, double[] targetDb, int maxBands)
    {
        int n = fHz.Length;
        var w = new double[n];
        for (int i = 0; i < n; i++) w[i] = fHz[i] >= WMin && fHz[i] <= WMax ? 1.0 : 0.05;

        var bands = new List<EqBand>();
        var model = new double[n];

        for (int k = 0; k < maxBands; k++)
        {
            Evaluate(bands, fHz, model);
            var resid = new double[n];
            for (int i = 0; i < n; i++) resid[i] = targetDb[i] - model[i];
            if (Rms(resid, w) < 0.25) break;

            // Largest weighted residual decides where the next band goes.
            int peak = 0;
            for (int i = 1; i < n; i++) if (Math.Abs(resid[i]) * w[i] > Math.Abs(resid[peak]) * w[peak]) peak = i;
            double gain = resid[peak];
            if (Math.Abs(gain) < 0.2) break;

            // Try PK at a few Qs and a shelf when the residual sits at either end;
            // keep whichever lowers the error most.
            EqBand? best = null; double bestErr = double.MaxValue;
            foreach (var cand in Candidates(fHz[peak], gain, peak, n))
            {
                bands.Add(cand);
                Evaluate(bands, fHz, model);
                double e = Error(targetDb, model, w);
                bands.RemoveAt(bands.Count - 1);
                if (e < bestErr) { bestErr = e; best = cand; }
            }
            bands.Add(best!);
            Refine(bands, fHz, targetDb, w, rounds: 3);
        }
        Refine(bands, fHz, targetDb, w, rounds: 6);

        Evaluate(bands, fHz, model);
        double sum = 0, wsum = 0, maxAbs = 0, maxAt = 0;
        for (int i = 0; i < n; i++)
        {
            double d = targetDb[i] - model[i];
            if (w[i] < 1) continue;
            sum += d * d; wsum += 1;
            if (Math.Abs(d) > maxAbs) { maxAbs = Math.Abs(d); maxAt = fHz[i]; }
        }
        foreach (var b in bands) { b.Enabled = true; b.F = (float)Math.Round(b.F, 1); b.Q = (float)Math.Round(b.Q, 2); b.GainDb = (float)Math.Round(b.GainDb, 1); }
        return new Fit(bands.OrderBy(b => b.F).ToList(), Math.Sqrt(sum / Math.Max(wsum, 1)), maxAbs, maxAt);
    }

    private static IEnumerable<EqBand> Candidates(double f, double gain, int idx, int n)
    {
        foreach (double q in new[] { 0.5, 0.8, 1.2, 2.0, 3.5 })
            yield return new EqBand { Type = FilterType.PK, F = (float)f, Q = (float)q, GainDb = (float)gain };
        if (idx < n / 4) yield return new EqBand { Type = FilterType.LowShelf, F = (float)(f * 1.5), Q = 0.7f, GainDb = (float)gain };
        if (idx > 3 * n / 4) yield return new EqBand { Type = FilterType.HighShelf, F = (float)(f / 1.5), Q = 0.7f, GainDb = (float)gain };
    }

    // Coordinate descent: each parameter of each band is nudged both ways with a
    // shrinking step and the move is kept only if the weighted error drops.
    private static void Refine(List<EqBand> bands, double[] fHz, double[] target, double[] w, int rounds)
    {
        var model = new double[fHz.Length];
        Evaluate(bands, fHz, model);
        double err = Error(target, model, w);
        double fStep = Math.Pow(2, 1.0 / 4), qStep = 1.25, gStep = 0.5;
        for (int r = 0; r < rounds; r++)
        {
            foreach (var b in bands)
            {
                err = Try(b, v => b.F = (float)Math.Clamp(b.F * v, 20, 20000), fStep, 1 / fStep, bands, fHz, target, w, model, err);
                err = Try(b, v => b.Q = (float)Math.Clamp(b.Q * v, 0.2, 8), qStep, 1 / qStep, bands, fHz, target, w, model, err);
                err = Try(b, v => b.GainDb = (float)Math.Clamp(b.GainDb + v, -18, 18), gStep, -gStep, bands, fHz, target, w, model, err);
            }
            fStep = Math.Sqrt(fStep); qStep = Math.Sqrt(qStep); gStep /= 2;
        }
    }

    private static double Try(EqBand b, Action<double> apply, double up, double down,
                              List<EqBand> bands, double[] fHz, double[] target, double[] w, double[] model, double err)
    {
        float f0 = b.F, q0 = b.Q, g0 = b.GainDb;
        foreach (double v in new[] { up, down })
        {
            b.F = f0; b.Q = q0; b.GainDb = g0;
            apply(v);
            Evaluate(bands, fHz, model);
            double e = Error(target, model, w);
            if (e < err) return e;                  // keep the move
        }
        b.F = f0; b.Q = q0; b.GainDb = g0;          // neither helped
        return err;
    }

    private static void Evaluate(List<EqBand> bands, double[] fHz, double[] outDb)
    {
        Array.Clear(outDb);
        foreach (var b in bands)
        {
            var c = Response.Design(b.Type, Fs, b.F, b.Q, b.GainDb);
            for (int i = 0; i < fHz.Length; i++) outDb[i] += c.MagDb(2 * Math.PI * fHz[i] / Fs);
        }
    }

    private static double Error(double[] target, double[] model, double[] w)
    {
        double s = 0;
        for (int i = 0; i < target.Length; i++) { double d = target[i] - model[i]; s += w[i] * d * d; }
        return s;
    }

    private static double Rms(double[] resid, double[] w)
    {
        double s = 0, ws = 0;
        for (int i = 0; i < resid.Length; i++) { s += w[i] * resid[i] * resid[i]; ws += w[i]; }
        return Math.Sqrt(s / Math.Max(ws, 1e-9));
    }
}
