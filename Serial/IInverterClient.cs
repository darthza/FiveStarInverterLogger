using FiveStarInverterLogger.Domain;

namespace FiveStarInverterLogger.Serial;

public interface IInverterClient
{
    Task<InverterSnapshot> ReadSnapshotAsync(CancellationToken cancellationToken);

    Task<RawResponse> QueryAsync(string command, CancellationToken cancellationToken);
}
