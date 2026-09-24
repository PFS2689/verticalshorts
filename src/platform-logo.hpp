#pragma once

#include "stream-destination.hpp"

#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace vsp {

/* Bundled SVG resource paths for official / redistributable platform marks. */
inline QString PlatformLogoSvgResource(StreamPlatform p, bool darkTheme = false)
{
	switch (p) {
	case StreamPlatform::YouTube:
		return QStringLiteral(":/vsp/platforms/youtube.svg");
	case StreamPlatform::Twitch:
		return QStringLiteral(":/vsp/platforms/twitch.svg");
	case StreamPlatform::TikTok:
		return darkTheme ? QStringLiteral(":/vsp/platforms/tiktok-dark.svg")
				 : QStringLiteral(":/vsp/platforms/tiktok.svg");
	case StreamPlatform::Instagram:
		/* Brand coral works on light; white mark on dark for contrast. */
		return darkTheme ? QStringLiteral(":/vsp/platforms/instagram-dark.svg")
				 : QStringLiteral(":/vsp/platforms/instagram.svg");
	case StreamPlatform::CustomRtmp:
		return QStringLiteral(":/vsp/platforms/custom-rtmp.svg");
	}
	return QStringLiteral(":/vsp/platforms/placeholder.svg");
}

inline QString PlatformPlaceholderSvgResource()
{
	return QStringLiteral(":/vsp/platforms/placeholder.svg");
}

bool IsDarkObsTheme();
QIcon LoadPlatformLogo(StreamPlatform p, const QSize &logicalSize = QSize(24, 24));
QPixmap LoadPlatformLogoPixmap(StreamPlatform p, const QSize &logicalSize = QSize(24, 24));

} // namespace vsp
