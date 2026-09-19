using System.Windows.Input;

namespace SpeakerDspDeck.ViewModels;

public sealed class RelayCommand : ICommand
{
    private readonly Action _exec;
    private readonly Func<bool>? _can;
    public RelayCommand(Action exec, Func<bool>? can = null) { _exec = exec; _can = can; }
    public bool CanExecute(object? p) => _can?.Invoke() ?? true;
    public void Execute(object? p) => _exec();
    public event EventHandler? CanExecuteChanged;
    public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}
