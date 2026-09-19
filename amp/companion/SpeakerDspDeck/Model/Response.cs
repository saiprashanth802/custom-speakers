namespace SpeakerDspDeck.Model;

// Frequency-response maths for the curve display. A straight port of the RBJ
// designs in firmware/dsp_engine/biquad.h so what the app draws is what the
// device computes (same formulas, same Q convention, same LR4 = 2x Butterworth).
public static class Response
{
    public const double FMin = 20, FMax = 20000;
    public const double ButterQ = 0.70710678;

    // Log-spaced frequency axis, shared by every curve.
    public static double[] LogAxis(int n)
    {
        var f = new double[n];
        double la = Math.Log10(FMin), lb = Math.Log10(FMax);
        for (int i = 0; i < n; i++) f[i] = Math.Pow(10, la + (lb - la) * i / (n - 1));
        return f;
    }

    // Normalised biquad coefficients (a0 == 1), RBJ cookbook. gainDb ignored for LP/HP.
    public readonly record struct Coef(double B0, double B1, double B2, double A1, double A2)
    {
        // |H(e^jw)|^2 = |B(e^jw)|^2 / |A(e^jw)|^2 evaluated directly.
        public double MagDb(double w)
        {
            double c1 = Math.Cos(w), s1 = Math.Sin(w), c2 = Math.Cos(2 * w), s2 = Math.Sin(2 * w);
            double nr = B0 + B1 * c1 + B2 * c2, ni = -(B1 * s1 + B2 * s2);
            double dr = 1 + A1 * c1 + A2 * c2, di = -(A1 * s1 + A2 * s2);
            double m2 = (nr * nr + ni * ni) / Math.Max(dr * dr + di * di, 1e-30);
            return 10 * Math.Log10(Math.Max(m2, 1e-30));
        }
    }

    public static Coef Design(FilterType type, double fs, double f0, double Q, double gainDb)
    {
        double A = Math.Pow(10, gainDb / 40.0);
        double w0 = 2 * Math.PI * f0 / fs;
        double cw = Math.Cos(w0), sw = Math.Sin(w0);
        double alpha = sw / (2 * Math.Max(Q, 1e-3));
        double b0, b1, b2, a0, a1, a2;
        switch (type)
        {
            case FilterType.LowPass:
                b0 = (1 - cw) * 0.5; b1 = 1 - cw; b2 = (1 - cw) * 0.5;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case FilterType.HighPass:
                b0 = (1 + cw) * 0.5; b1 = -(1 + cw); b2 = (1 + cw) * 0.5;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case FilterType.PK:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
            case FilterType.LowShelf:
            {
                double s = 2 * Math.Sqrt(A) * alpha;
                b0 =     A * ((A + 1) - (A - 1) * cw + s);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 =     A * ((A + 1) - (A - 1) * cw - s);
                a0 =         (A + 1) + (A - 1) * cw + s;
                a1 =    -2 * ((A - 1) + (A + 1) * cw);
                a2 =         (A + 1) + (A - 1) * cw - s; break;
            }
            case FilterType.HighShelf:
            {
                double s = 2 * Math.Sqrt(A) * alpha;
                b0 =      A * ((A + 1) + (A - 1) * cw + s);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 =      A * ((A + 1) + (A - 1) * cw - s);
                a0 =          (A + 1) - (A - 1) * cw + s;
                a1 =      2 * ((A - 1) - (A + 1) * cw);
                a2 =          (A + 1) - (A - 1) * cw - s; break;
            }
            default:
                b0 = 1; b1 = 0; b2 = 0; a0 = 1; a1 = 0; a2 = 0; break;
        }
        return new Coef(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    // Sum of a chain's responses in dB at each axis frequency.
    public static double[] ChainDb(IReadOnlyList<Coef> chain, double fs, double[] axis, double offsetDb = 0)
    {
        var y = new double[axis.Length];
        for (int i = 0; i < axis.Length; i++)
        {
            double w = 2 * Math.PI * axis[i] / fs, db = offsetDb;
            foreach (var c in chain) db += c.MagDb(w);
            y[i] = db;
        }
        return y;
    }

    // LR4 = two cascaded Butterworth (Q = 1/sqrt2) sections, same as the firmware.
    public static double[] Lr4Db(FilterType lpOrHp, double fs, double fc, double[] axis)
    {
        var c = Design(lpOrHp, fs, fc, ButterQ, 0);
        return ChainDb(new[] { c, c }, fs, axis);
    }
}

// What the curve control draws. Immutable snapshot; the VM replaces it whole.
public sealed record ResponseData(double[] Axis, double[] VoicingDb, double[] LowDb, double[] HighDb, double CrossoverHz);
