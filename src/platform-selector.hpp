#pragma once

#include "stream-destination.hpp"

#include <QFrame>
#include <QList>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QPropertyAnimation;

class PlatformSelector : public QWidget {
	Q_OBJECT
	Q_PROPERTY(qreal highlightOpacity READ highlightOpacity WRITE setHighlightOpacity)
public:
	explicit PlatformSelector(QWidget *parent = nullptr);

	void setPlatform(vsp::StreamPlatform platform);
	vsp::StreamPlatform platform() const { return current; }
	int currentData() const { return (int)current; }

signals:
	void platformChanged(int platform);

public:
	qreal highlightOpacity() const { return hoverOpacity; }
	void setHighlightOpacity(qreal v);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private slots:
	void TogglePopup();
	void SelectIndex(int index);

private:
	struct Item {
		vsp::StreamPlatform platform;
		QString name;
	};

	void RebuildClosedRow();
	void EnsurePopup();
	void PositionPopup();
	void ClosePopup();
	void ApplyThemeStyles();

	vsp::StreamPlatform current = vsp::StreamPlatform::YouTube;
	QList<Item> items;

	QFrame *closedFrame = nullptr;
	QLabel *closedLogo = nullptr;
	QLabel *closedText = nullptr;
	QLabel *chevron = nullptr;

	QFrame *popup = nullptr;
	QList<QFrame *> popupRows;
	qreal hoverOpacity = 0.0;
	QPropertyAnimation *hoverAnim = nullptr;
};
