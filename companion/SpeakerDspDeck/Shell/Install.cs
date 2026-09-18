using System.IO;

namespace SpeakerDspDeck.Shell;

// What makes the exe an "application" after it has been launched once:
//   * one instance — a second launch just brings the first one's window up;
//   * a Start-menu entry, so it is found by name (Start search, taskbar pinning,
//     and UI automation, which cannot grant an unregistered exe by name).
// Both are per-user and idempotent; nothing here needs elevation.
public static class Install
{
    private const string MutexName = "SpeakerDspDeck.SingleInstance";
    private const string ShowEvent = "SpeakerDspDeck.Show";
    private const string ShortcutName = "Speaker DSP Deck.lnk";

    private static Mutex? _mutex;

    // True for the first instance. A later instance signals the first to show
    // its window and should exit.
    public static bool ClaimSingleInstance()
    {
        _mutex = new Mutex(true, MutexName, out bool first);
        if (first) return true;
        try { EventWaitHandle.OpenExisting(ShowEvent).Set(); } catch { }
        return false;
    }

    public static void ListenForShow(Action onShow)
    {
        var ev = new EventWaitHandle(false, EventResetMode.AutoReset, ShowEvent);
        var t = new Thread(() => { while (true) { ev.WaitOne(); onShow(); } }) { IsBackground = true, Name = "show-listener" };
        t.Start();
    }

    public static void ReleaseSingleInstance()
    {
        try { _mutex?.ReleaseMutex(); } catch { }
    }

    public static string ShortcutPath =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), ShortcutName);

    // Creates or repoints the Start-menu shortcut. Returns what it did, for the log.
    public static string EnsureStartMenuShortcut()
    {
        string exe = Environment.ProcessPath ?? "";
        if (exe.Length == 0) return "";
        try
        {
            var shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType is null) return "";
            dynamic shell = Activator.CreateInstance(shellType)!;
            dynamic lnk = shell.CreateShortcut(ShortcutPath);
            bool existed = File.Exists(ShortcutPath);
            if (existed && string.Equals((string)lnk.TargetPath, exe, StringComparison.OrdinalIgnoreCase)) return "";
            lnk.TargetPath = exe;
            lnk.WorkingDirectory = Path.GetDirectoryName(exe);
            lnk.IconLocation = exe + ",0";
            lnk.Description = "Speaker DSP crossover — control app";
            lnk.Save();
            return existed ? "Start-menu shortcut repointed to this build." : "Added to the Start menu as \"Speaker DSP Deck\".";
        }
        catch (Exception ex) { return $"Start-menu shortcut not written: {ex.Message}"; }
    }
}
