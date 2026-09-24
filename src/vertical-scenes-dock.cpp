#include "vertical-scenes-dock.hpp"
#include "shorts-dock.hpp"

#include <obs-module.h>

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString Translate(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QToolButton *MakeToolButton(QWidget *parent, const QString &text, const QString &tip)
{
	auto *btn = new QToolButton(parent);
	btn->setText(text);
	btn->setToolTip(tip);
	btn->setAutoRaise(true);
	return btn;
}

} // namespace

VerticalScenesDock::VerticalScenesDock(ShortsDock *workspace_, QWidget *parent) : QFrame(parent), workspace(workspace_)
{
	setObjectName(QStringLiteral("VerticalScenesDock"));
	BuildUI();

	if (workspace) {
		connect(workspace, &ShortsDock::verticalScenesChanged, this, &VerticalScenesDock::RefreshList);
		RefreshList();
	}
}

void VerticalScenesDock::BuildUI()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 4, 4, 4);
	root->setSpacing(4);

	scenesList = new QListWidget(this);
	scenesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	connect(scenesList, &QListWidget::itemSelectionChanged, this, &VerticalScenesDock::OnSelectionChanged);
	root->addWidget(scenesList, 1);

	auto *sceneBtns = new QHBoxLayout();
	auto *addSceneBtn = MakeToolButton(this, QStringLiteral("+"), Translate("AddScene"));
	auto *removeSceneBtn = MakeToolButton(this, QStringLiteral("\u2212"), Translate("RemoveScene"));
	auto *dupSceneBtn = MakeToolButton(this, QStringLiteral("Dup"), Translate("DuplicateScene"));
	auto *renameSceneBtn = MakeToolButton(this, QStringLiteral("Ren"), Translate("RenameScene"));
	connect(addSceneBtn, &QToolButton::clicked, this, &VerticalScenesDock::OnAdd);
	connect(removeSceneBtn, &QToolButton::clicked, this, &VerticalScenesDock::OnRemove);
	connect(dupSceneBtn, &QToolButton::clicked, this, &VerticalScenesDock::OnDuplicate);
	connect(renameSceneBtn, &QToolButton::clicked, this, &VerticalScenesDock::OnRename);
	sceneBtns->addWidget(addSceneBtn);
	sceneBtns->addWidget(removeSceneBtn);
	sceneBtns->addWidget(dupSceneBtn);
	sceneBtns->addWidget(renameSceneBtn);
	sceneBtns->addStretch(1);
	root->addLayout(sceneBtns);
}

void VerticalScenesDock::RefreshList()
{
	if (!workspace || !scenesList)
		return;

	refreshing = true;
	workspace->PopulateScenesList(scenesList);
	refreshing = false;
}

void VerticalScenesDock::OnSelectionChanged()
{
	if (refreshing || !workspace || !scenesList)
		return;

	QListWidgetItem *item = scenesList->currentItem();
	if (!item)
		return;

	const QString uuid = item->data(Qt::UserRole).toString();
	if (uuid.isEmpty())
		return;

	workspace->RequestSelectScene(uuid);
}

void VerticalScenesDock::OnAdd()
{
	if (workspace)
		workspace->RequestAddScene();
}

void VerticalScenesDock::OnRemove()
{
	if (workspace)
		workspace->RequestRemoveScene();
}

void VerticalScenesDock::OnDuplicate()
{
	if (workspace)
		workspace->RequestDuplicateScene();
}

void VerticalScenesDock::OnRename()
{
	if (workspace)
		workspace->RequestRenameScene();
}
