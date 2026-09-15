using SpeakerDspDeck.Ble;
using SpeakerDspDeck.Protocol;

// Step-6 driver: exercise CMD_SAVE_PRESET / CMD_LOAD_PRESET across a rate change.
var ble = new BleLink();
var connected = new TaskCompletionSource();
ble.Log += s => Console.WriteLine("  ble: " + s);
ble.ConnectionChanged += up => { if (up) connected.TrySetResult(); };
ble.EventReceived += d => Console.WriteLine($"  evt: {BitConverter.ToString(d)}");
ble.StartScan();
await connected.Task.WaitAsync(TimeSpan.FromSeconds(20));
await Task.Delay(500);

async Task Send(string label, Frame f) { Console.WriteLine(label); await ble.Send(f); await Task.Delay(700); }

await Send("HELLO", Frame.Cmd(CmdOp.Hello));
await Send("SET_PROFILE HiRes", Frame.Cmd(CmdOp.SetProfile).U8(1));
await Task.Delay(1500);
await Send("SAVE_PRESET 0  (saved while running 96k)", Frame.Cmd(CmdOp.SavePreset).U8(0));
await Send("SET_PROFILE Normal", Frame.Cmd(CmdOp.SetProfile).U8(0));
await Task.Delay(1500);
await Send("LOAD_PRESET 0  (must design for the running 48k, not the saved 96k)", Frame.Cmd(CmdOp.LoadPreset).U8(0));
await Send("GET_STATUS", Frame.Cmd(CmdOp.GetStatus));
await Send("SET_PROFILE HiRes", Frame.Cmd(CmdOp.SetProfile).U8(1));
await Task.Delay(1500);
await Send("LOAD_PRESET 0  (now running 96k)", Frame.Cmd(CmdOp.LoadPreset).U8(0));
await Send("GET_STATUS", Frame.Cmd(CmdOp.GetStatus));
ble.Disconnect();
Console.WriteLine("done");
