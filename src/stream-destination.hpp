#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUuid>
#include <cstdint>

namespace vsp {

enum class StreamPlatform {
	YouTube = 0,
	Twitch = 1,
	TikTok = 2,
	Instagram = 3,
	CustomRtmp = 4,
};

enum class VerticalLiveStatus {
	Offline = 0,
	Connecting,
	Live,
	Reconnecting,
	Stopping,
	Error,
};

struct StreamDestination {
	QString id;              /* stable UUID */
	StreamPlatform platform = StreamPlatform::CustomRtmp;
	QString name;            /* user-facing destination name */
	QString server;          /* ingest URL — non-secret hostname ok in config */
	QString streamKey;       /* secret — memory only / secure store */
	QString username;        /* optional */
	QString password;        /* secret — memory only / secure store */
	QString twitchIngestId;  /* Twitch ingest selector id */
	bool useRecommendedTwitchIngest = true;
};

inline QString PlatformDisplayName(StreamPlatform p)
{
	switch (p) {
	case StreamPlatform::YouTube:
		return QStringLiteral("YouTube");
	case StreamPlatform::Twitch:
		return QStringLiteral("Twitch");
	case StreamPlatform::TikTok:
		return QStringLiteral("TikTok");
	case StreamPlatform::Instagram:
		return QStringLiteral("Instagram");
	case StreamPlatform::CustomRtmp:
		return QStringLiteral("Custom RTMP Server");
	}
	return QStringLiteral("Unknown");
}

inline QString PlatformIconResource(StreamPlatform p)
{
	/* Prefer bundled official-style SVG marks under /vsp/platforms. */
	switch (p) {
	case StreamPlatform::YouTube:
		return QStringLiteral(":/vsp/platforms/youtube.svg");
	case StreamPlatform::Twitch:
		return QStringLiteral(":/vsp/platforms/twitch.svg");
	case StreamPlatform::TikTok:
		return QStringLiteral(":/vsp/platforms/tiktok.svg");
	case StreamPlatform::Instagram:
		return QStringLiteral(":/vsp/platforms/instagram.svg");
	case StreamPlatform::CustomRtmp:
		return QStringLiteral(":/vsp/platforms/custom-rtmp.svg");
	}
	return QStringLiteral(":/vsp/platforms/placeholder.svg");
}

inline QString PlatformHelpUrl(StreamPlatform p)
{
	switch (p) {
	case StreamPlatform::YouTube:
		return QStringLiteral("https://support.google.com/youtube/answer/2907883");
	case StreamPlatform::Twitch:
		return QStringLiteral("https://help.twitch.tv/s/article/twitch-stream-key-guide");
	case StreamPlatform::TikTok:
		return QStringLiteral("https://www.tiktok.com/creators/creator-portal/");
	case StreamPlatform::Instagram:
		return QStringLiteral("https://help.instagram.com/");
	case StreamPlatform::CustomRtmp:
		return QString();
	}
	return QString();
}

/* Confirmed public default YouTube RTMPS ingest (user must still supply their own key). */
inline QString SuggestedYouTubeServer()
{
	return QStringLiteral("rtmps://a.rtmp.youtube.com/live2");
}

struct TwitchIngest {
	QString id;
	QString name;
	QString url;
};

inline QList<TwitchIngest> TwitchIngestOptions()
{
	/* Publicly documented Twitch RTMPS ingest endpoints (user supplies their own key). */
	return {
		{QStringLiteral("auto"), QStringLiteral("Recommended (auto)"),
		 QStringLiteral("rtmps://live.twitch.tv/app")},
		{QStringLiteral("sfo"), QStringLiteral("US West (San Francisco)"),
		 QStringLiteral("rtmps://live.twitch.tv/app")},
		{QStringLiteral("nyc"), QStringLiteral("US East (New York)"),
		 QStringLiteral("rtmps://live.twitch.tv/app")},
		{QStringLiteral("ams"), QStringLiteral("EU Central (Amsterdam)"),
		 QStringLiteral("rtmps://live.twitch.tv/app")},
		{QStringLiteral("syd"), QStringLiteral("Asia Pacific (Sydney)"),
		 QStringLiteral("rtmps://live.twitch.tv/app")},
	};
}

/* Fully mask secrets for logs / diagnostics. keepTail is clamped to 0 for log use. */
inline QString MaskSecret(const QString &secret, int keepTail = 0)
{
	if (secret.isEmpty())
		return QStringLiteral("(empty)");
	if (keepTail <= 0 || secret.size() <= keepTail)
		return QStringLiteral("****");
	return QStringLiteral("****") + secret.right(keepTail);
}

inline QString SanitizeUserFacingError(const QString &message)
{
	QString s = message;
	/* Strip obvious credential-bearing URL forms from OBS/output errors. */
	s.replace(QRegularExpression(QStringLiteral("rtmps?://[^\\s]+")), QStringLiteral("rtmp(s)://***"));
	s.replace(QRegularExpression(QStringLiteral("(?i)(stream[_ ]?key|password|token)\\s*[:=]\\s*\\S+")),
		  QStringLiteral("\\1=****"));
	return s;
}

inline QString SanitizeUrlForLog(const QString &url)
{
	QString s = url.trimmed();
	const int scheme = s.indexOf(QStringLiteral("://"));
	if (scheme >= 0) {
		const int authEnd = s.indexOf(QLatin1Char('@'), scheme + 3);
		if (authEnd > scheme)
			s = s.left(scheme + 3) + QStringLiteral("***@") + s.mid(authEnd + 1);
	}
	const int q = s.indexOf(QLatin1Char('?'));
	if (q >= 0)
		s = s.left(q) + QStringLiteral("?***");
	return s;
}

inline QString HostnameOnly(const QString &url)
{
	QUrl u(url.trimmed());
	if (!u.isValid() || u.host().isEmpty()) {
		/* Fallback parse */
		QString s = url.trimmed();
		const int scheme = s.indexOf(QStringLiteral("://"));
		if (scheme >= 0)
			s = s.mid(scheme + 3);
		const int slash = s.indexOf(QLatin1Char('/'));
		if (slash >= 0)
			s = s.left(slash);
		const int at = s.indexOf(QLatin1Char('@'));
		if (at >= 0)
			s = s.mid(at + 1);
		return s;
	}
	return u.host();
}

inline bool ProtocolSupported(const QString &url, QString *error = nullptr)
{
	const QString s = url.trimmed().toLower();
	if (s.startsWith(QStringLiteral("rtmps://")) || s.startsWith(QStringLiteral("rtmp://")))
		return true;
	if (error)
		*error = QStringLiteral("Only RTMP and RTMPS are supported for Vertical Streaming.");
	return false;
}

inline bool ValidateServerUrl(const QString &url, QString *error = nullptr, QString *warning = nullptr)
{
	const QString s = url.trimmed();
	if (s.isEmpty()) {
		if (error)
			*error = QStringLiteral("Server URL is required.");
		return false;
	}
	if (s.contains(QChar('\0'))) {
		if (error)
			*error = QStringLiteral("Server URL contains invalid characters.");
		return false;
	}
	if (!ProtocolSupported(s, error))
		return false;
	QUrl u(s);
	if (!u.isValid() || u.host().isEmpty()) {
		if (error)
			*error = QStringLiteral("Server URL is not a valid RTMP/RTMPS address.");
		return false;
	}
	if (s.startsWith(QStringLiteral("rtmp://"), Qt::CaseInsensitive) && warning) {
		*warning = QStringLiteral(
			"This destination uses unencrypted RTMP. Prefer RTMPS when the platform supports it.");
	}
	return true;
}

inline bool ValidateDestination(const StreamDestination &d, QString *error = nullptr, QString *field = nullptr,
				QString *warning = nullptr)
{
	if (d.name.trimmed().isEmpty() && d.platform == StreamPlatform::CustomRtmp) {
		/* name optional for presets; custom encouraged but not required */
	}
	if (!ValidateServerUrl(d.server, error, warning)) {
		if (field)
			*field = QStringLiteral("server");
		return false;
	}
	if (d.streamKey.trimmed().isEmpty()) {
		if (error)
			*error = QStringLiteral("Stream key is required for the Vertical Streaming destination.");
		if (field)
			*field = QStringLiteral("key");
		return false;
	}
	return true;
}

inline StreamDestination MakeDefaultDestination(StreamPlatform platform)
{
	StreamDestination d;
	d.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	d.platform = platform;
	d.name = PlatformDisplayName(platform);
	switch (platform) {
	case StreamPlatform::YouTube:
		d.server = SuggestedYouTubeServer();
		break;
	case StreamPlatform::Twitch:
		d.server = QStringLiteral("rtmps://live.twitch.tv/app");
		d.twitchIngestId = QStringLiteral("auto");
		d.useRecommendedTwitchIngest = true;
		break;
	case StreamPlatform::TikTok:
	case StreamPlatform::Instagram:
	case StreamPlatform::CustomRtmp:
		d.server.clear();
		break;
	}
	return d;
}

inline QString VerticalLiveStatusLabel(VerticalLiveStatus s)
{
	switch (s) {
	case VerticalLiveStatus::Offline:
		return QStringLiteral("Offline");
	case VerticalLiveStatus::Connecting:
		return QStringLiteral("Connecting");
	case VerticalLiveStatus::Live:
		return QStringLiteral("Live");
	case VerticalLiveStatus::Reconnecting:
		return QStringLiteral("Reconnecting");
	case VerticalLiveStatus::Stopping:
		return QStringLiteral("Stopping");
	case VerticalLiveStatus::Error:
		return QStringLiteral("Error");
	}
	return QStringLiteral("Unknown");
}

} // namespace vsp
