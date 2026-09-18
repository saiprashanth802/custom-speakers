using System.ComponentModel;
using SpeakerDspDeck.ViewModels;
using WF = System.Windows.Forms;

namespace SpeakerDspDeck.Shell;

// The tray presence, MacroPadDeck convention: the window is a view you open and
// close; the app lives here. Created on the WPF UI thread — WPF's dispatcher
// pumps Win32 messages, which is all NotifyIcon and its menu need.
public sealed class TrayIcon : IDisposable
{
    private const string RunKey  = @"Software\Microsoft\Windows\CurrentVersion\Run";
    private const string RunName = "SpeakerDspDeck";

    private readonly WF.NotifyIcon _icon;
    private readonly WF.ToolStripMenuItem _mute;
    private readonly MainWindow _win;
    private readonly MainViewModel _vm;

    public TrayIcon(MainWindow win)
    {
        _win = win;
        _vm  = win.Vm;

        var menu = new WF.ContextMenuStrip();
        var open = new WF.ToolStripMenuItem("Open Speaker DSP") { Font = new System.Drawing.Font(menu.Font, System.Drawing.FontStyle.Bold) };
        open.Click += (_, _) => _win.ShowFromTray();
        menu.Items.Add(open);

        _mute = new WF.ToolStripMenuItem("Mute") { CheckOnClick = true, Checked = _vm.Muted };
        _mute.CheckedChanged += (_, _) => { if (_vm.Muted != _mute.Checked) _vm.Muted = _mute.Checked; };
        menu.Items.Add(_mute);

        menu.Items.Add(new WF.ToolStripSeparator());

        var autostart = new WF.ToolStripMenuItem("Start with Windows (in the tray)") { CheckOnClick = true, Checked = IsAutostart() };
        autostart.CheckedChanged += (_, _) => SetAutostart(autostart.Checked);
        menu.Items.Add(autostart);

        menu.Items.Add(new WF.ToolStripSeparator());
        menu.Items.Add("Exit", null, (_, _) => App.ExitApp());

        // The exe carries dsp.ico (csproj ApplicationIcon) — reuse it so the
        // tray, taskbar and Explorer all show the same identity.
        System.Drawing.Icon icon;
        try { icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!)!; }
        catch { icon = System.Drawing.SystemIcons.Application; }

        _icon = new WF.NotifyIcon
        {
            Icon = icon,
            Text = "Speaker DSP",
            Visible = true,
            ContextMenuStrip = menu,
        };
        _icon.DoubleClick += (_, _) => _win.ShowFromTray();

        _vm.PropertyChanged += OnVm;
        UpdateText();
    }

    private void OnVm(object? s, PropertyChangedEventArgs e)
    {
        switch (e.PropertyName)
        {
            case nameof(MainViewModel.Muted):
                if (_mute.Checked != _vm.Muted) _mute.Checked = _vm.Muted;
                UpdateText();
                break;
            case nameof(MainViewModel.StatusText):
            case nameof(MainViewModel.DeviceRateHz):
            case nameof(MainViewModel.Connected):
                UpdateText();
                break;
        }
    }

    private void UpdateText()
    {
        string t = _vm.Connected
            ? $"Speaker DSP — {(_vm.DeviceRateHz > 0 ? $"{_vm.DeviceRateHz / 1000} kHz" : "connected")}{(_vm.Muted ? " · muted" : "")}"
            : "Speaker DSP — not connected";
        _icon.Text = t.Length > 63 ? t[..63] : t;   // NotifyIcon hard limit
    }

    private static bool IsAutostart()
    {
        using var k = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(RunKey);
        return k?.GetValue(RunName) is not null;
    }

    private static void SetAutostart(bool on)
    {
        using var k = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(RunKey);
        if (on) k.SetValue(RunName, $"\"{Environment.ProcessPath}\" --tray");
        else k.DeleteValue(RunName, throwOnMissingValue: false);
    }

    public void Dispose()
    {
        _vm.PropertyChanged -= OnVm;
        _icon.Visible = false;
        _icon.Dispose();
    }
}
