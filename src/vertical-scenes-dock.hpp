#pragma once

#include <QFrame>

class QContextMenuEvent;
class QKeyEvent;
class QListWidget;
class QListWidgetItem;
class ShortsDock;

class VerticalScenesDock : public QFrame {
	Q_OBJECT

public:
	explicit VerticalScenesDock(ShortsDock *workspace, QWidget *parent = nullptr);

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
	void RefreshList();
	void OnSelectionChanged();
	void OnAdd();
	void OnRemove();
	void OnItemChanged(QListWidgetItem *item);
	void OnItemDoubleClicked(QListWidgetItem *item);
	void ShowSceneContextMenu(const QPoint &globalPos);
	void BeginRenameSelected();

private:
	void BuildUI();
	QString CurrentUuid() const;
	void SelectUuid(const QString &uuid);

	ShortsDock *workspace = nullptr;
	QListWidget *scenesList = nullptr;
	bool refreshing = false;
	bool renaming = false;
};
