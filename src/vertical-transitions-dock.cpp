#include "vertical-transitions-dock.hpp"
#include "shorts-dock.hpp"

#include <obs-module.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QString Translate(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace

VerticalTransitionsDock::VerticalTransitionsDock(ShortsDock *workspace_, QWidget *parent)
	: QFrame(parent),
	  workspace(workspace_)
{
	setObjectName(QStringLiteral("VerticalTransitionsDock"));
	BuildUI();

	if (workspace) {
		connect(workspace, &ShortsDock::verticalTransitionsChanged, this,
			&VerticalTransitionsDock::RefreshTransitions);
		RefreshTransitions();
	}
}

void VerticalTransitionsDock::BuildUI()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 4, 4, 4);
	root->setSpacing(4);

	root->addWidget(new QLabel(Translate("Transitions"), this));

	transitionCombo = new QComboBox(this);
	connect(transitionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&VerticalTransitionsDock::OnTransitionChanged);
	root->addWidget(transitionCombo);

	auto *durRow = new QHBoxLayout();
	durRow->addWidget(new QLabel(Translate("TransitionDuration"), this));
	transitionDuration = new QSpinBox(this);
	transitionDuration->setRange(0, 10000);
	transitionDuration->setSingleStep(50);
	connect(transitionDuration, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&VerticalTransitionsDock::OnDurationChanged);
	durRow->addWidget(transitionDuration, 1);
	root->addLayout(durRow);

	auto *actionRow = new QHBoxLayout();
	auto *previewBtn = new QPushButton(Translate("TransitionPreview"), this);
	auto *triggerBtn = new QPushButton(Translate("TransitionTrigger"), this);
	connect(previewBtn, &QPushButton::clicked, this, &VerticalTransitionsDock::OnPreview);
	connect(triggerBtn, &QPushButton::clicked, this, &VerticalTransitionsDock::OnTrigger);
	actionRow->addWidget(previewBtn);
	actionRow->addWidget(triggerBtn);
	actionRow->addStretch(1);
	root->addLayout(actionRow);

	root->addStretch(1);
}

void VerticalTransitionsDock::RefreshTransitions()
{
	if (!workspace || !transitionCombo || !transitionDuration)
		return;

	refreshing = true;
	workspace->PopulateTransitions(transitionCombo, transitionDuration);
	refreshing = false;
}

void VerticalTransitionsDock::OnTransitionChanged(int index)
{
	if (refreshing || !workspace || !transitionCombo || index < 0)
		return;

	const QString name = transitionCombo->itemData(index).toString();
	if (name.isEmpty())
		return;

	workspace->RequestSetTransition(name);
}

void VerticalTransitionsDock::OnDurationChanged(int ms)
{
	if (refreshing || !workspace)
		return;

	workspace->RequestSetTransitionDuration(ms);
}

void VerticalTransitionsDock::OnPreview()
{
	if (workspace)
		workspace->RequestPreviewTransition();
}

void VerticalTransitionsDock::OnTrigger()
{
	if (workspace)
		workspace->RequestTriggerTransition();
}
