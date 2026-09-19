using System.Windows;
using SpeakerDspDeck.Shell;

namespace SpeakerDspDeck;

// Tray application (MacroPadDeck convention): the process lives in the tray,
// the window is opened and closed at will, and only the tray's Exit ends it.
//   SpeakerDspDeck.exe          launch with the window shown
//   SpeakerDspDeck.exe --tray   launch straight to the tray (what autostart uses)
public partial class App : Application
{
    private MainWindow? _win;
    private TrayIcon? _tray;

    // Set by the tray's Exit so MainWindow.OnClosing lets the close through.
    public static bool ExitRequested { get; private set; }

    protected override void OnStartup(StartupEventArgs e)
    {
        if (!Install.ClaimSingleInstance()) { Shutdown(); return; }
        base.OnStartup(e);

        _win = new MainWindow();
        if (!e.Args.Contains("--tray")) _win.Show();
        _tray = new TrayIcon(_win);

        Install.ListenForShow(() => OnUi(() => _win.ShowFromTray()));
        string note = Install.EnsureStartMenuShortcut();
        if (note.Length > 0) _win.Vm.LogLine(note);
    }

    public static void ExitApp()
    {
        ExitRequested = true;
        Current.Shutdown();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _tray?.Dispose();
        Install.ReleaseSingleInstance();
        base.OnExit(e);
    }

    // Marshal BLE callbacks onto the UI thread.
    public static void OnUi(Action a)
    {
        var d = Current?.Dispatcher;
        if (d is null || d.CheckAccess()) a();
        else d.Invoke(a);
    }
}
