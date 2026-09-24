#pragma once

#include <QFrame>
#include <QList>

class QContextMenuEvent;
class QKeyEvent;
class QListWidget;
class QListWidgetItem;
class QToolButton;
class ShortsDock;

class VerticalSourcesDock : public QFrame {
	Q_OBJECT

public:
	explicit VerticalSourcesDock(ShortsDock *workspace, QWidget *parent = nullptr);

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
	void RefreshSources();
	void OnSelectionChanged();
	void OnAdd();
	void OnRemove();
	void OnItemDoubleClicked(QListWidgetItem *item);
	void OnRowsMoved();
	void ShowSourceContextMenu(const QPoint &globalPos);

private:
	void BuildUI();
	void RebuildRowWidgets();
	void SelectItemId(qint64 itemId);
	qint64 CurrentItemId() const;
	QList<qint64> TopToBottomIds() const;

	ShortsDock *workspace = nullptr;
	QListWidget *sourcesList = nullptr;
	QToolButton *addBtn = nullptr;
	QToolButton *removeBtn = nullptr;
	bool refreshing = false;
	bool reordering = false;
};
