using System.Globalization;
using System.Windows;
using System.Windows.Media;
using SpeakerDspDeck.Model;

namespace SpeakerDspDeck.Controls;

// The hero: the tuning drawn as one bright object on the void. Voicing EQ is
// the iridescent line (the only cool-coloured thing in the app); the LR4
// low/high branches are faint gold; everything else is hairline grey.
// Pure OnRender — no per-point elements, so it redraws on every slider tick
// without allocation pressure.
//
// Curves are filled ribbons (the polyline offset ± half-width along the
// per-vertex normal) rather than pens, so the glow passes stay perfectly
// concentric with the crisp line. Thin translucent lines are pre-blended
// against the void rather than drawn through PushOpacity.
//
// SCREENSHOT GOTCHA (2026-09-15): Graphics.CopyFromScreen of this window shows
// white "sparkle" pixels on every low-alpha anti-aliased edge (thin translucent
// curves, effect rims, gradient-halo rims) and desaturates the ember. It is a
// capture artefact — PrintWindow(PW_RENDERFULLCONTENT) of the same frame is
// clean, hardware and software rendering both show it under CopyFromScreen,
// and it does not depend on geometry type. Almost certainly the desktop's HDR
// composition being read back through GDI. Verify visuals with PrintWindow.
public sealed class ResponseCurve : FrameworkElement
{
    public static readonly DependencyProperty DataProperty = DependencyProperty.Register(
        nameof(Data), typeof(ResponseData), typeof(ResponseCurve),
        new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender));
    public ResponseData? Data { get => (ResponseData?)GetValue(DataProperty); set => SetValue(DataProperty, value); }

    // dB window: EQ detail lives near 0; the crossover branches dive off the bottom.
    private const double DbTop = 15, DbBottom = -33;
    private const double PadL = 34, PadR = 10, PadT = 10, PadB = 18;

    private static readonly Brush Hair   = Frozen(new SolidColorBrush(Color.FromRgb(0x1b, 0x1e, 0x2a)));
    private static readonly Brush Zero   = Frozen(new SolidColorBrush(Color.FromRgb(0x3a, 0x3a, 0x40)));
    private static readonly Brush Label  = Frozen(new SolidColorBrush(Color.FromRgb(0x8d, 0x8a, 0x80)));
    private static readonly Brush Gold   = Frozen(new SolidColorBrush(Color.FromRgb(0x82, 0x5c, 0x2c)));   // ember at 50% over the void, pre-blended
    private static readonly Brush GoldHi = Frozen(new SolidColorBrush(Color.FromRgb(0x44, 0x23, 0x0d)));   // deep ember at 25% over the void, pre-blended
    private static readonly Brush Iris   = Frozen(new LinearGradientBrush(new GradientStopCollection
    {
        new(Color.FromRgb(0x8a, 0x6b, 0xff), 0.0),
        new(Color.FromRgb(0x4f, 0xc3, 0xff), 0.5),
        new(Color.FromRgb(0xff, 0x6a, 0xd5), 1.0),
    }, 0));

    private static readonly Typeface Mono = new(new FontFamily("Cascadia Mono, Consolas"), FontStyles.Normal, FontWeights.Normal, FontStretches.Normal);

    private static Brush Frozen(Brush b) { b.Freeze(); return b; }

    private double X(double hz, double w) =>
        PadL + (Math.Log10(hz) - Math.Log10(Response.FMin)) / (Math.Log10(Response.FMax) - Math.Log10(Response.FMin)) * (w - PadL - PadR);
    private double Y(double db, double h) =>
        PadT + (DbTop - Math.Clamp(db, DbBottom - 5, DbTop + 5)) / (DbTop - DbBottom) * (h - PadT - PadB);

    protected override void OnRender(DrawingContext dc)
    {
        double w = ActualWidth, h = ActualHeight;
        if (w < 40 || h < 40) return;
        double ppd = VisualTreeHelper.GetDpi(this).PixelsPerDip;

        // Void background is the parent's; draw only the grid.
        var hair = new Pen(Hair, 1); hair.Freeze();
        foreach (var hz in new[] { 20.0, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 })
        {
            double x = Math.Round(X(hz, w)) + 0.5;
            dc.DrawLine(hair, new Point(x, PadT), new Point(x, h - PadB));
            var t = Text(hz >= 1000 ? $"{hz / 1000:0}k" : $"{hz:0}", ppd);
            dc.DrawText(t, new Point(x - t.Width / 2, h - PadB + 3));
        }
        var zero = new Pen(Zero, 1); zero.Freeze();
        foreach (var db in new[] { 12.0, 0, -12, -24 })
        {
            double y = Math.Round(Y(db, h)) + 0.5;
            dc.DrawLine(db == 0 ? zero : hair, new Point(PadL, y), new Point(w - PadR, y));
            var t = Text(db == 0 ? "0" : $"{db:+0;-0}", ppd);
            dc.DrawText(t, new Point(PadL - t.Width - 6, y - t.Height / 2));
        }

        var d = Data;
        if (d is null || d.Axis.Length < 2) return;

        dc.PushClip(new RectangleGeometry(new Rect(PadL, PadT, w - PadL - PadR, h - PadT - PadB)));

        // Crossover branches: faint gold, plus an ember tick at fc.
        var lo = Points(d.Axis, d.LowDb, w, h);
        var hi = Points(d.Axis, d.HighDb, w, h);
        dc.DrawGeometry(Gold, null, Ribbon(lo, 0.6));
        dc.DrawGeometry(Gold, null, Ribbon(hi, 0.6));
        double xc = Math.Round(X(d.CrossoverHz, w)) + 0.5;
        var tick = new Pen(GoldHi, 1) { DashStyle = DashStyles.Dot }; tick.Freeze();
        dc.DrawLine(tick, new Point(xc, PadT), new Point(xc, h - PadB));

        // Voicing: glow ribbons underneath, then the crisp iridescent line.
        var v = Points(d.Axis, d.VoicingDb, w, h);
        foreach (var (half, alpha) in new[] { (7.0, 0.07), (3.5, 0.16), (1.75, 0.35) })
        {
            dc.PushOpacity(alpha);
            dc.DrawGeometry(Iris, null, Ribbon(v, half));
            dc.Pop();
        }
        dc.DrawGeometry(Iris, null, Ribbon(v, 0.9));
        dc.Pop();   // clip
    }

    private Point[] Points(double[] axis, double[] db, double w, double h)
    {
        var p = new Point[axis.Length];
        for (int i = 0; i < axis.Length; i++) p[i] = new Point(X(axis[i], w), Y(db[i], h));
        return p;
    }

    // Closed polygon: the polyline offset by ±half along the per-vertex normal.
    private static StreamGeometry Ribbon(Point[] p, double half)
    {
        int n = p.Length;
        var top = new Point[n];
        var bot = new Point[n];
        for (int i = 0; i < n; i++)
        {
            var a = p[Math.Max(i - 1, 0)];
            var b = p[Math.Min(i + 1, n - 1)];
            double dx = b.X - a.X, dy = b.Y - a.Y;
            double len = Math.Sqrt(dx * dx + dy * dy);
            if (len < 1e-9) { dx = 1; dy = 0; len = 1; }
            double nx = -dy / len * half, ny = dx / len * half;
            top[i] = new Point(p[i].X + nx, p[i].Y + ny);
            bot[i] = new Point(p[i].X - nx, p[i].Y - ny);
        }
        var g = new StreamGeometry { FillRule = FillRule.Nonzero };
        using (var c = g.Open())
        {
            c.BeginFigure(top[0], true, true);
            for (int i = 1; i < n; i++) c.LineTo(top[i], false, false);
            for (int i = n - 1; i >= 0; i--) c.LineTo(bot[i], false, false);
        }
        g.Freeze();
        return g;
    }

    private static FormattedText Text(string s, double ppd) =>
        new(s, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, Mono, 10, Label, ppd);
}
