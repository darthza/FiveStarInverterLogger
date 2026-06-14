namespace FiveStarInverterLogger.Domain;

public sealed record InverterSnapshot(
    DateTimeOffset Timestamp,
    string DeviceId,
    string Protocol,
    Q1Status? Status,
    RatingInfo? Rating,
    IdentityInfo? Identity,
    RawResponses Raw);

public sealed record Q1Status(
    decimal InputVoltage,
    decimal InputField,
    decimal OutputVoltage,
    int LoadOrPowerField,
    decimal OutputFrequency,
    decimal OutputCurrent,
    decimal BatteryVoltage,
    string StatusBits)
{
    public decimal ApproxOutputApparentPowerVa => OutputVoltage * OutputCurrent;
}

public sealed record RatingInfo(
    decimal NominalAcVoltage,
    int RatingField,
    decimal NominalBatteryVoltage,
    decimal NominalFrequency);

public sealed record IdentityInfo(string DeviceId, string Firmware);

public sealed record RawResponses(string? Q, string? Q1, string? F, string? I, string? Md, string? Qmd, string? CmsgInvHb);

public sealed record RawResponse(string Command, byte[] Bytes)
{
    public string Ascii => System.Text.Encoding.ASCII.GetString(Bytes);

    public string Hex => BitConverter.ToString(Bytes).Replace("-", " ");

    public bool IsEmpty => Bytes.Length == 0;
}
