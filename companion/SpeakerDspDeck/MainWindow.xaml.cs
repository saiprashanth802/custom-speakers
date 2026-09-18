using System.ComponentModel;
using System.Windows;
using System.Windows.Input;
using SpeakerDspDeck.ViewModels;

namespace SpeakerDspDeck;

public partial class MainWindow : Window
{
    private readonly MainViewModel _vm = new();

    public MainViewModel Vm => _vm;

    public MainWindow()
    {
        InitializeComponent();
        DataContext = _vm;
    }

    // Custom chrome: WindowChrome handles drag/resize; these are the two glyphs.
    private void Minimise_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
    private void Close_Click(object sender, RoutedEventArgs e) => Close();

    private void Log_Toggle(object sender, MouseButtonEventArgs e) => _vm.LogExpanded = !_vm.LogExpanded;

    // Closing the window hides it to the tray; the BLE link and the device
    // state stay live. Only the tray's Exit really closes it.
    protected override void OnClosing(CancelEventArgs e)
    {
        if (!App.ExitRequested)
        {
            e.Cancel = true;
            Hide();
            return;
        }
        base.OnClosing(e);
    }

    public void ShowFromTray()
    {
        Show();
        if (WindowState == WindowState.Minimized) WindowState = WindowState.Normal;
        Activate();
        // Windows refuses foreground to a process that isn't already in front;
        // a Topmost flick is the reliable way to surface a hidden window.
        Topmost = true; Topmost = false;
        Focus();
    }
}
