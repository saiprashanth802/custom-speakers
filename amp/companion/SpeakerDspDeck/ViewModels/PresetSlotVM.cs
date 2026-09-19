namespace SpeakerDspDeck.ViewModels;

// One preset slot row: Save captures the current tuning, Load pushes it back.
public sealed class PresetSlotVM : Bindable
{
    public string Slot { get; }
    public string Name => $"Preset {Slot}";
    public RelayCommand SaveCommand { get; }
    public RelayCommand LoadCommand { get; }

    private bool _hasData;
    public bool HasData
    {
        get => _hasData;
        set { if (Set(ref _hasData, value)) { Raise(nameof(Status)); LoadCommand.RaiseCanExecuteChanged(); } }
    }

    public string Status => _hasData ? "saved" : "empty";

    public PresetSlotVM(string slot, Action<string> save, Action<string> load, bool hasData)
    {
        Slot = slot;
        _hasData = hasData;
        SaveCommand = new RelayCommand(() => save(slot));
        LoadCommand = new RelayCommand(() => load(slot), () => _hasData);
    }
}
