namespace FiveStarInverterLogger.Configuration;

public sealed class InverterOptions
{
    public const string SectionName = "Inverter";

    public string PortName { get; set; } = OperatingSystem.IsWindows() ? "COM1" : "/dev/ttyUSB0";

    public int BaudRate { get; set; } = 2400;

    public int ReadTimeoutMs { get; set; } = 1200;

    public int PollIntervalSeconds { get; set; } = 10;

    public string[] StartupRawCommands { get; set; } = ["Q", "F", "I"];
}
