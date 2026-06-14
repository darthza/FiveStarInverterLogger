namespace FiveStarInverterLogger.Configuration;

public sealed class SmartHomeOptions
{
    public const string SectionName = "SmartHome";

    public bool Enabled { get; set; }

    public string EndpointUrl { get; set; } = "";

    public string? ApiKey { get; set; }

    public string DeviceId { get; set; } = "fivestar-gf-8048mbw-fs";

    public int TimeoutSeconds { get; set; } = 10;
}
