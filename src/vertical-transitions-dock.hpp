#pragma once

#include <QFrame>

class QComboBox;
class QSpinBox;
class ShortsDock;

class VerticalTransitionsDock : public QFrame {
	Q_OBJECT

public:
	explicit VerticalTransitionsDock(ShortsDock *workspace, QWidget *parent = nullptr);

private slots:
	void RefreshTransitions();
	void OnTransitionChanged(int index);
	void OnDurationChanged(int ms);
	void OnPreview();
	void OnTrigger();

private:
	void BuildUI();

	ShortsDock *workspace = nullptr;
	QComboBox *transitionCombo = nullptr;
	QSpinBox *transitionDuration = nullptr;
	bool refreshing = false;
};
