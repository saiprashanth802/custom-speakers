using System.Windows;
using SpeakerDspDeck.ViewModels;

namespace SpeakerDspDeck;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
        DataContext = new MainViewModel();
    }
}
