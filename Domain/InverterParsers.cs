using System.Globalization;

namespace FiveStarInverterLogger.Domain;

public static class InverterParsers
{
    public static Q1Status? ParseQ1(string raw)
    {
        if (!raw.StartsWith('('))
        {
            return null;
        }

        var parts = raw[1..].Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 8)
        {
            return null;
        }

        return new Q1Status(
            ParseDecimal(parts[0]),
            ParseDecimal(parts[1]),
            ParseDecimal(parts[2]),
            int.Parse(parts[3], CultureInfo.InvariantCulture),
            ParseDecimal(parts[4]),
            ParseDecimal(parts[5]),
            ParseDecimal(parts[6]),
            parts[7]);
    }

    public static RatingInfo? ParseF(string raw)
    {
        if (!raw.StartsWith('#'))
        {
            return null;
        }

        var parts = raw[1..].Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 4)
        {
            return null;
        }

        return new RatingInfo(
            ParseDecimal(parts[0]),
            int.Parse(parts[1], CultureInfo.InvariantCulture),
            ParseDecimal(parts[2]),
            ParseDecimal(parts[3]));
    }

    public static IdentityInfo? ParseI(string raw)
    {
        if (!raw.StartsWith('#'))
        {
            return null;
        }

        var payload = raw[1..].Trim();
        var firmwareIndex = payload.IndexOf('R', StringComparison.Ordinal);
        if (firmwareIndex < 0)
        {
            return new IdentityInfo(payload, "");
        }

        return new IdentityInfo(
            payload[..firmwareIndex].Trim(),
            payload[firmwareIndex..].Trim());
    }

    private static decimal ParseDecimal(string value) =>
        decimal.Parse(value, CultureInfo.InvariantCulture);
}
