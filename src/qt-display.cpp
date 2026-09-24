#include "qt-display.hpp"
#include "display-helpers.hpp"

#include <obs-module.h>

#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>

#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <obs-nix-platform.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
#include <qpa/qplatformnativeinterface.h>
#endif
#endif

#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(v) ((void)(v))
#endif

static inline long long color_to_int(const QColor &color)
{
	auto shift = [&](unsigned val, int s) { return ((val & 0xff) << s); };
	return shift(color.red(), 0) | shift(color.green(), 8) | shift(color.blue(), 16) | shift(color.alpha(), 24);
}

static inline QColor rgba_to_color(uint32_t rgba)
{
	return QColor::fromRgb(rgba & 0xFF, (rgba >> 8) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 24) & 0xFF);
}

OBSQTDisplay::OBSQTDisplay(QWidget *parent, Qt::WindowFlags flags) : QWidget(parent, flags)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	/* Force native window creation before connecting to QWindow signals. */
	(void)winId();

	/* Dark widget palette — if OBS display is not ready, never show Qt white. */
	QPalette pal = palette();
	pal.setColor(QPalette::Window, QColor(0x28, 0x28, 0x28));
	pal.setColor(QPalette::Base, QColor(0x28, 0x28, 0x28));
	setPalette(pal);
	setAutoFillBackground(true);

	auto windowVisible = [this](bool visible) {
		if (!visible) {
#if !defined(_WIN32) && !defined(__APPLE__)
			display = nullptr;
#endif
			return;
		}

		if (!display) {
			CreateDisplay(true);
		} else {
			QSize size = GetPixelSize(this);
			obs_display_resize(display, size.width(), size.height());
		}
	};

	auto screenChanged = [this](QScreen *) {
		CreateDisplay(true);
		if (display) {
			QSize size = GetPixelSize(this);
			obs_display_resize(display, size.width(), size.height());
		}
	};

	if (windowHandle()) {
		connect(windowHandle(), &QWindow::visibleChanged, windowVisible);
		connect(windowHandle(), &QWindow::screenChanged, screenChanged);
	} else {
		blog(LOG_ERROR, "[obs-shorts-vertical] OBSQTDisplay: windowHandle() is null after winId()");
	}
}

QColor OBSQTDisplay::GetDisplayBackgroundColor() const
{
	return rgba_to_color(backgroundColor);
}

void OBSQTDisplay::SetDisplayBackgroundColor(const QColor &color)
{
	uint32_t newBackgroundColor = (uint32_t)color_to_int(color);
	if (newBackgroundColor != backgroundColor) {
		backgroundColor = newBackgroundColor;
		UpdateDisplayBackgroundColor();
	}
}

void OBSQTDisplay::UpdateDisplayBackgroundColor()
{
	if (display)
		obs_display_set_background_color(display, backgroundColor);
}

bool QTToGSWindow(QWindow *window, gs_window &gswindow)
{
	bool success = true;

#ifdef _WIN32
	gswindow.hwnd = (HWND)window->winId();
	success = gswindow.hwnd != nullptr;
#elif defined(__APPLE__)
	gswindow.view = (id)window->winId();
	success = gswindow.view != nullptr;
#else
	switch (obs_get_nix_platform()) {
	case OBS_NIX_PLATFORM_X11_EGL:
		gswindow.id = window->winId();
		gswindow.display = obs_get_nix_platform_display();
		break;
	case OBS_NIX_PLATFORM_WAYLAND:
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
	{
		QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
		gswindow.display = native->nativeResourceForWindow("surface", window);
		success = gswindow.display != nullptr;
	}
#else
		gswindow.display = (void *)window->winId();
		success = gswindow.display != nullptr;
#endif
		break;
	default:
		success = false;
		break;
	}
#endif
	return success;
}

void OBSQTDisplay::CreateDisplay(bool force)
{
	if (display || destroying)
		return;

	if (!windowHandle()) {
		(void)winId();
		if (!windowHandle()) {
			blog(LOG_ERROR, "[obs-shorts-vertical] CreateDisplay: no QWindow/native handle");
			return;
		}
	}

	if (!windowHandle()->isExposed() && !force) {
		if (!createLogged) {
			blog(LOG_INFO, "[obs-shorts-vertical] CreateDisplay: window not exposed yet (will retry)");
			createLogged = true;
		}
		return;
	}

	QSize size = GetPixelSize(this);
	if (size.width() < 2)
		size.setWidth(2);
	if (size.height() < 2)
		size.setHeight(2);

	gs_init_data info = {};
	info.cx = size.width();
	info.cy = size.height();
	/* OBS Studio 32 frontend qt-display uses GS_BGRA — keep parity. */
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;

	if (!QTToGSWindow(windowHandle(), info.window)) {
		blog(LOG_ERROR,
		     "[obs-shorts-vertical] CreateDisplay: QTToGSWindow failed (exposed=%d size=%dx%d)",
		     (int)windowHandle()->isExposed(), size.width(), size.height());
		return;
	}

	display = obs_display_create(&info, backgroundColor);
	if (!display) {
		blog(LOG_ERROR,
		     "[obs-shorts-vertical] obs_display_create FAILED (size=%dx%d format=GS_BGRA bg=0x%08X)",
		     size.width(), size.height(), backgroundColor);
		return;
	}

	obs_display_set_enabled(display, true);
	obs_display_set_background_color(display, backgroundColor);

	blog(LOG_INFO,
	     "[obs-shorts-vertical] obs_display_create OK: size=%dx%d enabled=%d bg=0x%08X hwnd/view ready",
	     size.width(), size.height(), (int)obs_display_enabled(display), backgroundColor);

	/* Once OBS owns the surface, stop Qt from painting over it. */
	setAutoFillBackground(false);

	emit DisplayCreated(this);
}

void OBSQTDisplay::paintEvent(QPaintEvent *event)
{
	CreateDisplay(false);

	if (!display) {
		/* Never leave a white Qt fallback. Dark OBS-style fill until display attaches. */
		QPainter p(this);
		p.fillRect(rect(), QColor(0x28, 0x28, 0x28));
		return;
	}

	QWidget::paintEvent(event);
}

void OBSQTDisplay::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	CreateDisplay(true);
	if (display) {
		QSize size = GetPixelSize(this);
		obs_display_resize(display, (std::max)(2, size.width()), (std::max)(2, size.height()));
	}
}

void OBSQTDisplay::moveEvent(QMoveEvent *event)
{
	QWidget::moveEvent(event);
	OnMove();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool OBSQTDisplay::nativeEvent(const QByteArray &, void *message, qintptr *)
#else
bool OBSQTDisplay::nativeEvent(const QByteArray &, void *message, long *)
#endif
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	switch (msg.message) {
	case WM_DISPLAYCHANGE:
		OnDisplayChange();
		break;
	}
#else
	UNUSED_PARAMETER(message);
#endif
	return false;
}

void OBSQTDisplay::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	CreateDisplay(true);

	if (isVisible() && display) {
		QSize size = GetPixelSize(this);
		obs_display_resize(display, (std::max)(2, size.width()), (std::max)(2, size.height()));
	}

	emit DisplayResized();
}

QPaintEngine *OBSQTDisplay::paintEngine() const
{
	/* When the OBS display owns the HWND, disable Qt painting.
	 * Before display creation, allow Qt so we can paint a dark fallback
	 * instead of the default white native window. */
	return display ? nullptr : QWidget::paintEngine();
}

void OBSQTDisplay::OnMove()
{
	if (display)
		obs_display_update_color_space(display);
}

void OBSQTDisplay::OnDisplayChange()
{
	if (display)
		obs_display_update_color_space(display);
}
