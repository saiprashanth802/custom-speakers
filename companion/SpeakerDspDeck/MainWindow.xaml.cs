using System.Windows;
using System.Windows.Input;
using SpeakerDspDeck.ViewModels;

namespace SpeakerDspDeck;

public partial class MainWindow : Window
{
    private readonly MainViewModel _vm = new();

    public MainWindow()
    {
        InitializeComponent();
        DataContext = _vm;
    }

    // Custom chrome: WindowChrome handles drag/resize; these are the two glyphs.
    private void Minimise_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
    private void Close_Click(object sender, RoutedEventArgs e) => Close();

    private void Log_Toggle(object sender, MouseButtonEventArgs e) => _vm.LogExpanded = !_vm.LogExpanded;
}
