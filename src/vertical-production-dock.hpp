#pragma once

#include <QFrame>

class ShortsDock;
class VerticalScenesDock;
class VerticalSourcesDock;
class VerticalTransitionsDock;
class QSplitter;

/* Combined Vertical Scenes + Sources + Transitions dock. */
class VerticalProductionDock : public QFrame {
	Q_OBJECT

public:
	explicit VerticalProductionDock(ShortsDock *workspace, QWidget *parent = nullptr);

private:
	void BuildUI();

	ShortsDock *workspace = nullptr;
	QSplitter *splitter = nullptr;
	VerticalScenesDock *scenesPanel = nullptr;
	VerticalSourcesDock *sourcesPanel = nullptr;
	VerticalTransitionsDock *transitionsPanel = nullptr;
};
