using System.Net.Http.Json;
using FiveStarInverterLogger.Configuration;
using FiveStarInverterLogger.Domain;
using Microsoft.Extensions.Options;

namespace FiveStarInverterLogger.Publishing;

public sealed class SmartHomePublisher : ISmartHomePublisher
{
    private readonly HttpClient _httpClient;
    private readonly ILogger<SmartHomePublisher> _logger;
    private readonly SmartHomeOptions _options;

    public SmartHomePublisher(HttpClient httpClient, IOptions<SmartHomeOptions> options, ILogger<SmartHomePublisher> logger)
    {
        _httpClient = httpClient;
        _logger = logger;
        _options = options.Value;
        _httpClient.Timeout = TimeSpan.FromSeconds(_options.TimeoutSeconds);
    }

    public async Task PublishAsync(InverterSnapshot snapshot, CancellationToken cancellationToken)
    {
        if (!_options.Enabled)
        {
            _logger.LogDebug("SmartHome publishing is disabled");
            return;
        }

        if (string.IsNullOrWhiteSpace(_options.EndpointUrl))
        {
            _logger.LogWarning("SmartHome publishing is enabled but no endpoint URL is configured");
            return;
        }

        using var request = new HttpRequestMessage(HttpMethod.Post, _options.EndpointUrl)
        {
            Content = JsonContent.Create(ToPayload(snapshot))
        };

        if (!string.IsNullOrWhiteSpace(_options.ApiKey))
        {
            request.Headers.Add("X-Api-Key", _options.ApiKey);
        }

        using var response = await _httpClient.SendAsync(request, cancellationToken);
        if (!response.IsSuccessStatusCode)
        {
            var body = await response.Content.ReadAsStringAsync(cancellationToken);
            _logger.LogWarning("SmartHome publish failed with status {StatusCode}: {Body}", response.StatusCode, body);
            return;
        }

        _logger.LogInformation("Published inverter snapshot to SmartHome endpoint {Endpoint}", _options.EndpointUrl);
    }

    private object ToPayload(InverterSnapshot snapshot) => new
    {
        deviceId = _options.DeviceId,
        timestamp = snapshot.Timestamp,
        protocol = snapshot.Protocol,
        status = snapshot.Status,
        rating = snapshot.Rating,
        identity = snapshot.Identity,
        raw = snapshot.Raw
    };
}
