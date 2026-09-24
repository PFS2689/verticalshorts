#include "vertical-scenes-dock.hpp"
#include "shorts-dock.hpp"

#include <obs-module.h>

#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
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

VerticalScenesDock::VerticalScenesDock(ShortsDock *workspace_, QWidget *parent)
	: QFrame(parent),
	  workspace(workspace_)
{
	setObjectName(QStringLiteral("VerticalScenesDock"));
	setFocusPolicy(Qt::StrongFocus);
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
	scenesList->setObjectName(QStringLiteral("VerticalScenesList"));
	scenesList->setSelectionMode(QAbstractItemView::SingleSelection);
	scenesList->setContextMenuPolicy(Qt::CustomContextMenu);
	scenesList->setEditTriggers(QAbstractItemView::EditKeyPressed);
	scenesList->setAlternatingRowColors(true);

	connect(scenesList, &QListWidget::itemSelectionChanged, this, &VerticalScenesDock::OnSelectionChanged);
	connect(scenesList, &QListWidget::itemChanged, this, &VerticalScenesDock::OnItemChanged);
	connect(scenesList, &QListWidget::itemDoubleClicked, this, &VerticalScenesDock::OnItemDoubleClicked);
	connect(scenesList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
		ShowSceneContextMenu(scenesList->viewport()->mapToGlobal(pos));
	});
	root->addWidget(scenesList, 1);

	/* Visible controls: + / − only (Dup/Ren moved to right-click). */
	auto *sceneBtns = new QHBoxLayout();
	auto *addSceneBtn = MakeToolButton(this, QStringLiteral("+"), Translate("AddScene"));
	auto *removeSceneBtn = MakeToolButton(this, QStringLiteral("\u2212"), Translate("RemoveScene"));
	connect(addSceneBtn, &QToolButton::clicked, this, &VerticalScenesDock::OnAdd);
	connect(removeSceneBtn, &QToolButton::clicked, this, &VerticalScenesDock::OnRemove);
	sceneBtns->addWidget(addSceneBtn);
	sceneBtns->addWidget(removeSceneBtn);
	sceneBtns->addStretch(1);
	root->addLayout(sceneBtns);
}

void VerticalScenesDock::RefreshList()
{
	if (!workspace || !scenesList || renaming)
		return;

	const QString keep = CurrentUuid();
	refreshing = true;
	workspace->PopulateScenesList(scenesList);
	for (int i = 0; i < scenesList->count(); ++i) {
		QListWidgetItem *item = scenesList->item(i);
		if (!item)
			continue;
		item->setFlags(item->flags() | Qt::ItemIsEditable);
	}
	if (!keep.isEmpty())
		SelectUuid(keep);
	refreshing = false;
}

QString VerticalScenesDock::CurrentUuid() const
{
	if (!scenesList)
		return {};
	QListWidgetItem *item = scenesList->currentItem();
	return item ? item->data(Qt::UserRole).toString() : QString();
}

void VerticalScenesDock::SelectUuid(const QString &uuid)
{
	if (!scenesList || uuid.isEmpty())
		return;
	for (int i = 0; i < scenesList->count(); ++i) {
		QListWidgetItem *item = scenesList->item(i);
		if (item && item->data(Qt::UserRole).toString() == uuid) {
			scenesList->setCurrentItem(item);
			return;
		}
	}
}

void VerticalScenesDock::OnSelectionChanged()
{
	if (refreshing || renaming || !workspace || !scenesList)
		return;

	const QString uuid = CurrentUuid();
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
	if (!workspace)
		return;
	const QString uuid = CurrentUuid();
	if (!uuid.isEmpty())
		workspace->RequestSelectScene(uuid);
	workspace->RequestRemoveScene();
}

void VerticalScenesDock::BeginRenameSelected()
{
	if (!scenesList)
		return;
	QListWidgetItem *item = scenesList->currentItem();
	if (!item)
		return;
	scenesList->setFocus(Qt::OtherFocusReason);
	scenesList->editItem(item);
}

void VerticalScenesDock::OnItemDoubleClicked(QListWidgetItem *item)
{
	if (!item || !scenesList)
		return;
	scenesList->setCurrentItem(item);
	BeginRenameSelected();
}

void VerticalScenesDock::OnItemChanged(QListWidgetItem *item)
{
	if (refreshing || !workspace || !item)
		return;

	const QString uuid = item->data(Qt::UserRole).toString();
	const QString name = item->text().trimmed();
	if (uuid.isEmpty() || name.isEmpty()) {
		/* Restore if the user cleared the name. */
		RefreshList();
		return;
	}

	renaming = true;
	workspace->RequestSelectScene(uuid);
	workspace->RequestRenameSceneTo(name);
	renaming = false;
}

void VerticalScenesDock::ShowSceneContextMenu(const QPoint &globalPos)
{
	if (!workspace || !scenesList)
		return;

	QListWidgetItem *item = scenesList->itemAt(scenesList->viewport()->mapFromGlobal(globalPos));
	if (!item)
		item = scenesList->currentItem();
	if (!item)
		return;

	scenesList->setCurrentItem(item);
	const QString uuid = item->data(Qt::UserRole).toString();
	if (!uuid.isEmpty())
		workspace->RequestSelectScene(uuid);

	QMenu menu(this);
	menu.addAction(Translate("RenameScene"), this, &VerticalScenesDock::BeginRenameSelected);
	menu.addAction(Translate("DuplicateScene"), workspace, &ShortsDock::RequestDuplicateScene);
	menu.addAction(Translate("RemoveScene"), this, &VerticalScenesDock::OnRemove);
	menu.addSeparator();
	menu.addAction(Translate("MoveSourceUp"), workspace, &ShortsDock::RequestSceneMoveUp);
	menu.addAction(Translate("MoveSourceDown"), workspace, &ShortsDock::RequestSceneMoveDown);
	menu.addAction(Translate("MoveSourceTop"), workspace, &ShortsDock::RequestSceneMoveTop);
	menu.addAction(Translate("MoveSourceBottom"), workspace, &ShortsDock::RequestSceneMoveBottom);
	menu.exec(globalPos);
}

void VerticalScenesDock::contextMenuEvent(QContextMenuEvent *event)
{
	ShowSceneContextMenu(event->globalPos());
	event->accept();
}

void VerticalScenesDock::keyPressEvent(QKeyEvent *event)
{
	if (!workspace) {
		QFrame::keyPressEvent(event);
		return;
	}

	switch (event->key()) {
	case Qt::Key_F2:
		BeginRenameSelected();
		event->accept();
		return;
	case Qt::Key_Delete:
	case Qt::Key_Backspace:
		OnRemove();
		event->accept();
		return;
	default:
		break;
	}
	QFrame::keyPressEvent(event);
}
