using System.IO.Ports;
using FiveStarInverterLogger.Configuration;
using FiveStarInverterLogger.Domain;
using Microsoft.Extensions.Options;

namespace FiveStarInverterLogger.Serial;

public sealed class FiveStarInverterClient : IInverterClient, IDisposable
{
    private const string ProtocolName = "Megatec/Q1 legacy RS232 ASCII";
    private readonly ILogger<FiveStarInverterClient> _logger;
    private readonly InverterOptions _options;
    private readonly SemaphoreSlim _serialLock = new(1, 1);
    private SerialPort? _serialPort;

    public FiveStarInverterClient(IOptions<InverterOptions> options, ILogger<FiveStarInverterClient> logger)
    {
        _options = options.Value;
        _logger = logger;
    }

    public async Task<InverterSnapshot> ReadSnapshotAsync(CancellationToken cancellationToken)
    {
        var rawQ = await QueryAsync("Q", cancellationToken);
        var rawQ1 = await QueryAsync("Q1", cancellationToken);
        var rawF = await QueryAsync("F", cancellationToken);
        var rawI = await QueryAsync("I", cancellationToken);

        RawResponse? rawMd = null;
        RawResponse? rawQmd = null;
        RawResponse? rawCmsgInvHb = null;

        // These are known FiveStar/Sunmagic leads, but not guaranteed on every unit.
        // They are captured raw until the binary/text layout is confirmed.
        rawMd = await QueryAsync("MD", cancellationToken);
        rawQmd = await QueryAsync("QMD", cancellationToken);
        rawCmsgInvHb = await QueryAsync("CMSG.INV-HB", cancellationToken);

        var rawQ1Text = rawQ1.Ascii.Trim();
        var rawFText = rawF.Ascii.Trim();
        var rawIText = rawI.Ascii.Trim();

        return new InverterSnapshot(
            DateTimeOffset.UtcNow,
            _options.PortName,
            ProtocolName,
            InverterParsers.ParseQ1(rawQ1Text),
            InverterParsers.ParseF(rawFText),
            InverterParsers.ParseI(rawIText),
            new RawResponses(
                rawQ.Ascii.Trim(),
                rawQ1Text,
                rawFText,
                rawIText,
                rawMd.Ascii.Trim(),
                rawQmd.Ascii.Trim(),
                rawCmsgInvHb.Hex));
    }

    public async Task<RawResponse> QueryAsync(string command, CancellationToken cancellationToken)
    {
        await _serialLock.WaitAsync(cancellationToken);
        try
        {
            var port = GetOrOpenPort();
            port.DiscardInBuffer();
            port.DiscardOutBuffer();
            port.Write(command + "\r");

            using var buffer = new MemoryStream();
            var deadline = DateTimeOffset.UtcNow.AddMilliseconds(_options.ReadTimeoutMs);

            while (DateTimeOffset.UtcNow < deadline)
            {
                cancellationToken.ThrowIfCancellationRequested();

                try
                {
                    var value = port.ReadByte();
                    if (value < 0)
                    {
                        continue;
                    }

                    buffer.WriteByte((byte)value);
                    if (value == '\r')
                    {
                        break;
                    }
                }
                catch (TimeoutException)
                {
                    break;
                }
            }

            var response = new RawResponse(command, buffer.ToArray());
            _logger.LogDebug("Serial query {Command} returned ASCII '{Ascii}' HEX '{Hex}'", command, response.Ascii.Trim(), response.Hex);
            return response;
        }
        catch
        {
            ClosePort();
            throw;
        }
        finally
        {
            _serialLock.Release();
        }
    }

    private SerialPort GetOrOpenPort()
    {
        if (_serialPort?.IsOpen == true)
        {
            return _serialPort;
        }

        _serialPort = new SerialPort(_options.PortName, _options.BaudRate, Parity.None, 8, StopBits.One)
        {
            NewLine = "\r",
            ReadTimeout = _options.ReadTimeoutMs,
            WriteTimeout = _options.ReadTimeoutMs,
            DtrEnable = false,
            RtsEnable = false
        };

        _serialPort.Open();
        _logger.LogInformation("Opened inverter serial port {PortName} at {BaudRate} baud", _options.PortName, _options.BaudRate);
        return _serialPort;
    }

    private void ClosePort()
    {
        if (_serialPort is null)
        {
            return;
        }

        try
        {
            _serialPort.Dispose();
        }
        finally
        {
            _serialPort = null;
        }
    }

    public void Dispose()
    {
        ClosePort();
        _serialLock.Dispose();
    }
}
