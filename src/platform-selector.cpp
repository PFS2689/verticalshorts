#include "platform-selector.hpp"
#include "platform-logo.hpp"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>

namespace {

constexpr int kLogoPx = 22;
constexpr int kRowH = 40;
constexpr int kClosedH = 44;

QString ThemeStylesheet(bool dark)
{
	const QString border = dark ? QStringLiteral("#3c4043") : QStringLiteral("#c4c7c5");
	const QString bg = dark ? QStringLiteral("#2b2b2f") : QStringLiteral("#ffffff");
	const QString bgHover = dark ? QStringLiteral("#3a3a40") : QStringLiteral("#f1f3f4");
	const QString bgSelected = dark ? QStringLiteral("#394457") : QStringLiteral("#e8f0fe");
	const QString text = dark ? QStringLiteral("#e8eaed") : QStringLiteral("#1f1f1f");
	const QString accent = dark ? QStringLiteral("#8ab4f8") : QStringLiteral("#1a73e8");

	return QStringLiteral(
		       "QFrame#vspPlatformClosed {"
		       "  background: %1;"
		       "  border: 1px solid %2;"
		       "  border-radius: 10px;"
		       "}"
		       "QFrame#vspPlatformClosed[hovered=\"true\"] {"
		       "  border: 1px solid %3;"
		       "}"
		       "QFrame#vspPlatformPopup {"
		       "  background: %1;"
		       "  border: 1px solid %2;"
		       "  border-radius: 12px;"
		       "}"
		       "QFrame#vspPlatformRow {"
		       "  background: transparent;"
		       "  border-radius: 8px;"
		       "}"
		       "QFrame#vspPlatformRow[hovered=\"true\"] {"
		       "  background: %4;"
		       "}"
		       "QFrame#vspPlatformRow[selected=\"true\"] {"
		       "  background: %5;"
		       "}"
		       "QLabel#vspPlatformName {"
		       "  color: %6;"
		       "  font-size: 13px;"
		       "  font-weight: 600;"
		       "}"
		       "QLabel#vspPlatformChevron {"
		       "  color: %6;"
		       "  font-size: 14px;"
		       "}")
		.arg(bg, border, accent, bgHover, bgSelected, text);
}

} // namespace

PlatformSelector::PlatformSelector(QWidget *parent) : QWidget(parent)
{
	items = {{vsp::StreamPlatform::YouTube, QStringLiteral("YouTube")},
		 {vsp::StreamPlatform::Twitch, QStringLiteral("Twitch")},
		 {vsp::StreamPlatform::TikTok, QStringLiteral("TikTok")},
		 {vsp::StreamPlatform::Instagram, QStringLiteral("Instagram")},
		 {vsp::StreamPlatform::CustomRtmp, QStringLiteral("Custom RTMP Server")}};

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	closedFrame = new QFrame(this);
	closedFrame->setObjectName("vspPlatformClosed");
	closedFrame->setCursor(Qt::PointingHandCursor);
	closedFrame->setFixedHeight(kClosedH);
	closedFrame->installEventFilter(this);
	auto *row = new QHBoxLayout(closedFrame);
	row->setContentsMargins(12, 8, 12, 8);
	row->setSpacing(12);

	closedLogo = new QLabel(closedFrame);
	closedLogo->setFixedSize(kLogoPx, kLogoPx);
	closedLogo->setAlignment(Qt::AlignCenter);
	closedLogo->setScaledContents(false);

	closedText = new QLabel(closedFrame);
	closedText->setObjectName("vspPlatformName");
	closedText->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

	chevron = new QLabel(QStringLiteral("\u25BE"), closedFrame); /* small black down-pointing triangle */
	chevron->setObjectName("vspPlatformChevron");
	chevron->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

	row->addWidget(closedLogo, 0, Qt::AlignVCenter);
	row->addWidget(closedText, 1, Qt::AlignVCenter);
	row->addWidget(chevron, 0, Qt::AlignVCenter);
	root->addWidget(closedFrame);

	hoverAnim = new QPropertyAnimation(this, "highlightOpacity", this);
	hoverAnim->setDuration(140);

	ApplyThemeStyles();
	RebuildClosedRow();
}

void PlatformSelector::setHighlightOpacity(qreal v)
{
	hoverOpacity = v;
	closedFrame->setProperty("hovered", v > 0.35);
	closedFrame->style()->unpolish(closedFrame);
	closedFrame->style()->polish(closedFrame);
}

void PlatformSelector::setPlatform(vsp::StreamPlatform platform)
{
	current = platform;
	RebuildClosedRow();
	if (popup) {
		for (int i = 0; i < items.size(); ++i) {
			const bool sel = items[i].platform == current;
			popupRows[i]->setProperty("selected", sel);
			popupRows[i]->style()->unpolish(popupRows[i]);
			popupRows[i]->style()->polish(popupRows[i]);
		}
	}
}

void PlatformSelector::RebuildClosedRow()
{
	const QPixmap pm = vsp::LoadPlatformLogoPixmap(current, QSize(kLogoPx, kLogoPx));
	closedLogo->setPixmap(pm);
	for (const auto &it : items) {
		if (it.platform == current) {
			closedText->setText(it.name);
			break;
		}
	}
	closedLogo->setAccessibleName(closedText->text());
	closedFrame->setAccessibleName(closedText->text());
}

void PlatformSelector::ApplyThemeStyles()
{
	const QString ss = ThemeStylesheet(vsp::IsDarkObsTheme());
	setStyleSheet(ss);
	if (popup)
		popup->setStyleSheet(ss);
}

void PlatformSelector::EnsurePopup()
{
	if (popup)
		return;

	popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
	popup->setObjectName("vspPlatformPopup");
	popup->setAttribute(Qt::WA_DeleteOnClose, false);
	popup->installEventFilter(this);
	auto *lay = new QVBoxLayout(popup);
	lay->setContentsMargins(8, 8, 8, 8);
	lay->setSpacing(4);

	for (int i = 0; i < items.size(); ++i) {
		auto *row = new QFrame(popup);
		row->setObjectName("vspPlatformRow");
		row->setCursor(Qt::PointingHandCursor);
		row->setFixedHeight(kRowH);
		row->setProperty("selected", items[i].platform == current);
		row->installEventFilter(this);
		auto *hl = new QHBoxLayout(row);
		hl->setContentsMargins(10, 6, 10, 6);
		hl->setSpacing(12);

		auto *logo = new QLabel(row);
		logo->setObjectName("vspPlatformLogo");
		logo->setFixedSize(kLogoPx, kLogoPx);
		logo->setAlignment(Qt::AlignCenter);
		logo->setPixmap(vsp::LoadPlatformLogoPixmap(items[i].platform, QSize(kLogoPx, kLogoPx)));

		auto *name = new QLabel(items[i].name, row);
		name->setObjectName("vspPlatformName");
		name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

		hl->addWidget(logo, 0, Qt::AlignVCenter);
		hl->addWidget(name, 1, Qt::AlignVCenter);
		lay->addWidget(row);
		popupRows.push_back(row);

		row->setProperty("platformIndex", i);
	}

	ApplyThemeStyles();
}

void PlatformSelector::PositionPopup()
{
	if (!popup)
		return;
	popup->setFixedWidth(qMax(width(), 260));
	popup->adjustSize();
	const QPoint global = closedFrame->mapToGlobal(QPoint(0, closedFrame->height() + 4));
	QRect geo(global, popup->sizeHint());
	if (QScreen *screen = QApplication::screenAt(global)) {
		const QRect avail = screen->availableGeometry();
		if (geo.bottom() > avail.bottom())
			geo.moveTop(closedFrame->mapToGlobal(QPoint(0, 0)).y() - geo.height() - 4);
		if (geo.right() > avail.right())
			geo.moveRight(avail.right() - 4);
	}
	popup->setGeometry(geo);
}

void PlatformSelector::ClosePopup()
{
	if (popup)
		popup->hide();
	chevron->setText(QStringLiteral("\u25BE"));
}

void PlatformSelector::TogglePopup()
{
	EnsurePopup();
	ApplyThemeStyles();
	/* Refresh logos for theme changes */
	for (int i = 0; i < items.size(); ++i) {
		if (auto *logo = popupRows[i]->findChild<QLabel *>(QStringLiteral("vspPlatformLogo")))
			logo->setPixmap(vsp::LoadPlatformLogoPixmap(items[i].platform, QSize(kLogoPx, kLogoPx)));
		popupRows[i]->setProperty("selected", items[i].platform == current);
		popupRows[i]->setProperty("hovered", false);
		popupRows[i]->style()->unpolish(popupRows[i]);
		popupRows[i]->style()->polish(popupRows[i]);
	}
	if (popup->isVisible()) {
		ClosePopup();
		return;
	}
	PositionPopup();
	popup->show();
	chevron->setText(QStringLiteral("\u25B4"));
}

void PlatformSelector::SelectIndex(int index)
{
	if (index < 0 || index >= items.size())
		return;
	const auto p = items[index].platform;
	const bool changed = p != current;
	setPlatform(p);
	ClosePopup();
	if (changed)
		emit platformChanged((int)p);
}

bool PlatformSelector::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == closedFrame) {
		if (event->type() == QEvent::Enter) {
			hoverAnim->stop();
			hoverAnim->setStartValue(hoverOpacity);
			hoverAnim->setEndValue(1.0);
			hoverAnim->start();
		} else if (event->type() == QEvent::Leave) {
			hoverAnim->stop();
			hoverAnim->setStartValue(hoverOpacity);
			hoverAnim->setEndValue(0.0);
			hoverAnim->start();
		} else if (event->type() == QEvent::MouseButtonRelease) {
			auto *me = static_cast<QMouseEvent *>(event);
			if (me->button() == Qt::LeftButton)
				TogglePopup();
			return true;
		}
	}

	for (QFrame *row : popupRows) {
		if (obj != row)
			continue;
		if (event->type() == QEvent::Enter) {
			row->setProperty("hovered", true);
			row->style()->unpolish(row);
			row->style()->polish(row);
		} else if (event->type() == QEvent::Leave) {
			row->setProperty("hovered", false);
			row->style()->unpolish(row);
			row->style()->polish(row);
		} else if (event->type() == QEvent::MouseButtonRelease) {
			auto *me = static_cast<QMouseEvent *>(event);
			if (me->button() == Qt::LeftButton)
				SelectIndex(row->property("platformIndex").toInt());
			return true;
		}
	}

	if (obj == popup && event->type() == QEvent::Hide)
		chevron->setText(QStringLiteral("\u25BE"));

	return QWidget::eventFilter(obj, event);
}

void PlatformSelector::hideEvent(QHideEvent *event)
{
	ClosePopup();
	QWidget::hideEvent(event);
}
