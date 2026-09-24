#include "platform-logo.hpp"

#include <QApplication>
#include <QFile>
#include <QPainter>
#include <QPalette>
#include <QSvgRenderer>

namespace vsp {
namespace {

QPixmap RenderSvgResource(const QString &path, const QSize &pixelSize)
{
	QPixmap pm(pixelSize);
	pm.fill(Qt::transparent);
	if (!QFile::exists(path))
		return pm;

	QSvgRenderer renderer(path);
	if (!renderer.isValid())
		return pm;

	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	renderer.render(&p);
	return pm;
}

} // namespace

bool IsDarkObsTheme()
{
	const QPalette pal = QApplication::palette();
	return pal.color(QPalette::Window).lightness() < 128;
}

QPixmap LoadPlatformLogoPixmap(StreamPlatform p, const QSize &logicalSize)
{
	const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
	const QSize pixelSize(qMax(1, int(logicalSize.width() * dpr + 0.5)),
			      qMax(1, int(logicalSize.height() * dpr + 0.5)));

	const bool dark = IsDarkObsTheme();
	QString path = PlatformLogoSvgResource(p, dark);
	QPixmap pm = RenderSvgResource(path, pixelSize);
	if (pm.isNull() || pm.size().isEmpty() || !QFile::exists(path)) {
		path = PlatformPlaceholderSvgResource();
		pm = RenderSvgResource(path, pixelSize);
	}
	pm.setDevicePixelRatio(dpr);
	return pm;
}

QIcon LoadPlatformLogo(StreamPlatform p, const QSize &logicalSize)
{
	QIcon icon;
	const QPixmap pm = LoadPlatformLogoPixmap(p, logicalSize);
	if (!pm.isNull())
		icon.addPixmap(pm);
	/* Also register @2x-ish size for menus that request larger icons */
	if (logicalSize != QSize(48, 48)) {
		const QPixmap large = LoadPlatformLogoPixmap(p, QSize(48, 48));
		if (!large.isNull())
			icon.addPixmap(large);
	}
	return icon;
}

} // namespace vsp
