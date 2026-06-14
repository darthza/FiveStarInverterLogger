using FiveStarInverterLogger;
using FiveStarInverterLogger.Configuration;
using FiveStarInverterLogger.Publishing;
using FiveStarInverterLogger.Serial;

var builder = Host.CreateApplicationBuilder(args);

builder.Services.Configure<InverterOptions>(builder.Configuration.GetSection(InverterOptions.SectionName));
builder.Services.Configure<SmartHomeOptions>(builder.Configuration.GetSection(SmartHomeOptions.SectionName));

builder.Services.AddHttpClient<ISmartHomePublisher, SmartHomePublisher>();
builder.Services.AddSingleton<IInverterClient, FiveStarInverterClient>();
builder.Services.AddHostedService<InverterPollingWorker>();

var host = builder.Build();
host.Run();
