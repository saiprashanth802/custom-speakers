using System.Windows;

namespace SpeakerDspDeck;

public partial class App : Application
{
    // Marshal BLE callbacks onto the UI thread.
    public static void OnUi(Action a)
    {
        var d = Current?.Dispatcher;
        if (d is null || d.CheckAccess()) a();
        else d.Invoke(a);
    }
}
