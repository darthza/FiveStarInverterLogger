using FiveStarInverterLogger.Domain;

namespace FiveStarInverterLogger.Publishing;

public interface ISmartHomePublisher
{
    Task PublishAsync(InverterSnapshot snapshot, CancellationToken cancellationToken);
}
