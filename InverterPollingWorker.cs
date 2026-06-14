using FiveStarInverterLogger.Configuration;
using FiveStarInverterLogger.Publishing;
using FiveStarInverterLogger.Serial;
using Microsoft.Extensions.Options;

namespace FiveStarInverterLogger;

public sealed class InverterPollingWorker : BackgroundService
{
    private readonly IInverterClient _inverterClient;
    private readonly ILogger<InverterPollingWorker> _logger;
    private readonly InverterOptions _options;
    private readonly ISmartHomePublisher _publisher;

    public InverterPollingWorker(
        IInverterClient inverterClient,
        ISmartHomePublisher publisher,
        IOptions<InverterOptions> options,
        ILogger<InverterPollingWorker> logger)
    {
        _inverterClient = inverterClient;
        _publisher = publisher;
        _options = options.Value;
        _logger = logger;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation(
            "FiveStar inverter logger starting. Port={PortName}, Baud={BaudRate}, PollInterval={PollInterval}s",
            _options.PortName,
            _options.BaudRate,
            _options.PollIntervalSeconds);

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                var snapshot = await _inverterClient.ReadSnapshotAsync(stoppingToken);

                if (snapshot.Status is null)
                {
                    _logger.LogWarning("Q1 status could not be parsed. Raw Q1: {RawQ1}", snapshot.Raw.Q1);
                }
                else
                {
                    _logger.LogInformation(
                        "Inverter: input={InputVoltage}V output={OutputVoltage}V loadField={LoadField} freq={Frequency}Hz current={Current}A battery={Battery}V",
                        snapshot.Status.InputVoltage,
                        snapshot.Status.OutputVoltage,
                        snapshot.Status.LoadOrPowerField,
                        snapshot.Status.OutputFrequency,
                        snapshot.Status.OutputCurrent,
                        snapshot.Status.BatteryVoltage);
                }

                await _publisher.PublishAsync(snapshot, stoppingToken);
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                break;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Failed to read or publish inverter snapshot");
            }

            await Task.Delay(TimeSpan.FromSeconds(_options.PollIntervalSeconds), stoppingToken);
        }
    }
}
