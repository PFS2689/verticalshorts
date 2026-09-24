#pragma once

#include <QFrame>

class QListWidget;
class ShortsDock;

class VerticalScenesDock : public QFrame {
	Q_OBJECT

public:
	explicit VerticalScenesDock(ShortsDock *workspace, QWidget *parent = nullptr);

private slots:
	void RefreshList();
	void OnSelectionChanged();
	void OnAdd();
	void OnRemove();
	void OnDuplicate();
	void OnRename();

private:
	void BuildUI();

	ShortsDock *workspace = nullptr;
	QListWidget *scenesList = nullptr;
	bool refreshing = false;
};
