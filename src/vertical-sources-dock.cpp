#include "vertical-sources-dock.hpp"
#include "shorts-dock.hpp"

#include <obs-module.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QSize>
#include <QSizePolicy>
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
	btn->setFixedSize(28, 24);
	return btn;
}

class SourceRowWidget : public QWidget {
public:
	SourceRowWidget(qint64 itemId_, bool visible, bool locked, const QString &name, ShortsDock *workspace_,
			QWidget *parent = nullptr)
		: QWidget(parent),
		  itemId(itemId_),
		  workspace(workspace_)
	{
		auto *lay = new QHBoxLayout(this);
		lay->setContentsMargins(2, 0, 4, 0);
		lay->setSpacing(4);

		visBtn = MakeToolButton(this, visible ? QStringLiteral("◉") : QStringLiteral("○"),
					Translate("ToggleVisible"));
		visBtn->setFixedSize(22, 20);
		lockBtn = MakeToolButton(this, locked ? QStringLiteral("■") : QStringLiteral("□"),
					 Translate("ToggleLock"));
		lockBtn->setFixedSize(22, 20);

		auto *label = new QLabel(name, this);
		label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
		if (!visible) {
			QPalette pal = label->palette();
			pal.setColor(QPalette::WindowText, pal.color(QPalette::Disabled, QPalette::WindowText));
			label->setPalette(pal);
		}

		lay->addWidget(visBtn, 0);
		lay->addWidget(lockBtn, 0);
		lay->addWidget(label, 1);

		connect(visBtn, &QToolButton::clicked, this, [this]() {
			if (workspace)
				workspace->RequestToggleSourceVisibleById(itemId);
		});
		connect(lockBtn, &QToolButton::clicked, this, [this]() {
			if (workspace)
				workspace->RequestToggleSourceLockById(itemId);
		});
	}

protected:
	void mouseDoubleClickEvent(QMouseEvent *event) override
	{
		if (workspace) {
			workspace->RequestSelectSource(itemId);
			workspace->RequestSourceProperties();
		}
		QWidget::mouseDoubleClickEvent(event);
	}

private:
	qint64 itemId = 0;
	ShortsDock *workspace = nullptr;
	QToolButton *visBtn = nullptr;
	QToolButton *lockBtn = nullptr;
};

} // namespace

VerticalSourcesDock::VerticalSourcesDock(ShortsDock *workspace_, QWidget *parent)
	: QFrame(parent),
	  workspace(workspace_)
{
	setObjectName(QStringLiteral("VerticalSourcesDock"));
	setFocusPolicy(Qt::StrongFocus);
	BuildUI();

	if (workspace) {
		connect(workspace, &ShortsDock::verticalSourcesChanged, this, &VerticalSourcesDock::RefreshSources);
		RefreshSources();
	}
}

void VerticalSourcesDock::BuildUI()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 4, 4, 4);
	root->setSpacing(4);

	sourcesList = new QListWidget(this);
	sourcesList->setObjectName(QStringLiteral("VerticalSourcesList"));
	sourcesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	sourcesList->setDragDropMode(QAbstractItemView::InternalMove);
	sourcesList->setDefaultDropAction(Qt::MoveAction);
	sourcesList->setDragEnabled(true);
	sourcesList->setAcceptDrops(true);
	sourcesList->setDropIndicatorShown(true);
	sourcesList->setContextMenuPolicy(Qt::CustomContextMenu);
	sourcesList->setAlternatingRowColors(true);
	sourcesList->setUniformItemSizes(true);
	sourcesList->setSpacing(1);

	connect(sourcesList, &QListWidget::itemSelectionChanged, this, &VerticalSourcesDock::OnSelectionChanged);
	connect(sourcesList, &QListWidget::itemDoubleClicked, this, &VerticalSourcesDock::OnItemDoubleClicked);
	connect(sourcesList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
		ShowSourceContextMenu(sourcesList->viewport()->mapToGlobal(pos));
	});
	if (sourcesList->model()) {
		connect(sourcesList->model(), &QAbstractItemModel::rowsMoved, this, &VerticalSourcesDock::OnRowsMoved);
	}

	root->addWidget(sourcesList, 1);

	auto *sourceBtns = new QHBoxLayout();
	addBtn = MakeToolButton(this, QStringLiteral("+"), Translate("AddSource"));
	removeBtn = MakeToolButton(this, QStringLiteral("\u2212"), Translate("RemoveSource"));
	connect(addBtn, &QToolButton::clicked, this, &VerticalSourcesDock::OnAdd);
	connect(removeBtn, &QToolButton::clicked, this, &VerticalSourcesDock::OnRemove);
	sourceBtns->addWidget(addBtn);
	sourceBtns->addWidget(removeBtn);
	sourceBtns->addStretch(1);
	root->addLayout(sourceBtns);
}

void VerticalSourcesDock::RefreshSources()
{
	if (!workspace || !sourcesList || reordering)
		return;

	const qint64 keepId = CurrentItemId();
	refreshing = true;
	workspace->PopulateSourcesList(sourcesList);
	RebuildRowWidgets();
	if (keepId != 0)
		SelectItemId(keepId);
	refreshing = false;
}

void VerticalSourcesDock::RebuildRowWidgets()
{
	if (!sourcesList || !workspace)
		return;
	for (int i = 0; i < sourcesList->count(); i++) {
		QListWidgetItem *item = sourcesList->item(i);
		if (!item)
			continue;
		const qint64 id = item->data(Qt::UserRole).toLongLong();
		const bool visible = item->data(Qt::UserRole + 1).toBool();
		const bool locked = item->data(Qt::UserRole + 2).toBool();
		const QString name = item->text();
		/* Keep text for accessibility/DnD; widget shows the OBS-like row. */
		item->setSizeHint(QSize(item->sizeHint().width(), 26));
		sourcesList->setItemWidget(item, new SourceRowWidget(id, visible, locked, name, workspace, sourcesList));
	}
}

void VerticalSourcesDock::SelectItemId(qint64 itemId)
{
	if (!sourcesList)
		return;
	for (int i = 0; i < sourcesList->count(); i++) {
		QListWidgetItem *item = sourcesList->item(i);
		if (item && item->data(Qt::UserRole).toLongLong() == itemId) {
			sourcesList->setCurrentItem(item);
			return;
		}
	}
}

qint64 VerticalSourcesDock::CurrentItemId() const
{
	if (!sourcesList)
		return 0;
	QListWidgetItem *item = sourcesList->currentItem();
	return item ? item->data(Qt::UserRole).toLongLong() : 0;
}

QList<qint64> VerticalSourcesDock::TopToBottomIds() const
{
	QList<qint64> ids;
	if (!sourcesList)
		return ids;
	for (int i = 0; i < sourcesList->count(); i++) {
		QListWidgetItem *item = sourcesList->item(i);
		if (item)
			ids.push_back(item->data(Qt::UserRole).toLongLong());
	}
	return ids;
}

void VerticalSourcesDock::OnSelectionChanged()
{
	if (refreshing || !workspace || !sourcesList)
		return;

	QListWidgetItem *item = sourcesList->currentItem();
	if (!item)
		return;

	const qint64 itemId = item->data(Qt::UserRole).toLongLong();
	workspace->RequestSelectSource(itemId);
}

void VerticalSourcesDock::OnAdd()
{
	if (!workspace)
		return;
	workspace->ShowAddSourceMenu(addBtn ? static_cast<QWidget *>(addBtn) : this);
}

void VerticalSourcesDock::OnRemove()
{
	if (workspace)
		workspace->RequestRemoveSource();
}

void VerticalSourcesDock::OnItemDoubleClicked(QListWidgetItem *item)
{
	if (!workspace || !item)
		return;
	workspace->RequestSelectSource(item->data(Qt::UserRole).toLongLong());
	workspace->RequestSourceProperties();
}

void VerticalSourcesDock::OnRowsMoved()
{
	if (refreshing || reordering || !workspace)
		return;
	reordering = true;
	workspace->RequestReorderSources(TopToBottomIds());
	reordering = false;
	RebuildRowWidgets();
}

void VerticalSourcesDock::ShowSourceContextMenu(const QPoint &globalPos)
{
	if (!workspace || !sourcesList)
		return;

	QListWidgetItem *item = sourcesList->itemAt(sourcesList->viewport()->mapFromGlobal(globalPos));
	if (!item)
		item = sourcesList->currentItem();
	if (!item)
		return;

	const qint64 itemId = item->data(Qt::UserRole).toLongLong();
	const bool configurable = item->data(Qt::UserRole + 3).toBool();
	const bool visible = item->data(Qt::UserRole + 1).toBool();
	const bool locked = item->data(Qt::UserRole + 2).toBool();
	workspace->RequestSelectSource(itemId);

	QMenu menu(this);

	QAction *props = menu.addAction(Translate("SourceProperties"), workspace, &ShortsDock::RequestSourceProperties);
	props->setEnabled(configurable);
	props->setToolTip(Translate("SourcePropertiesTip"));
	QAction *filters = menu.addAction(Translate("SourceFilters"), workspace, &ShortsDock::RequestSourceFilters);
	filters->setToolTip(Translate("SourceFiltersTip"));
	menu.addSeparator();

	QMenu *transformMenu = menu.addMenu(Translate("Transform"));
	QAction *editTf = transformMenu->addAction(Translate("EditTransform"), workspace, &ShortsDock::RequestEditTransform);
	editTf->setToolTip(Translate("EditTransformTip"));
	transformMenu->addAction(Translate("CopyTransform"), workspace, &ShortsDock::RequestCopyTransform);
	QAction *pasteTf =
		transformMenu->addAction(Translate("PasteTransform"), workspace, &ShortsDock::RequestPasteTransform);
	pasteTf->setEnabled(workspace->HasTransformClipboard());
	transformMenu->addAction(Translate("ResetTransform"), workspace, &ShortsDock::RequestResetTransform);
	transformMenu->addSeparator();
	workspace->AppendTransformFitMenu(transformMenu);
	transformMenu->addSeparator();
	transformMenu->addAction(Translate("Rotate90CW"), workspace, [this]() {
		if (workspace)
			workspace->RequestRotateDegrees(90.0f);
	});
	transformMenu->addAction(Translate("Rotate90CCW"), workspace, [this]() {
		if (workspace)
			workspace->RequestRotateDegrees(-90.0f);
	});
	transformMenu->addAction(Translate("Rotate180"), workspace, [this]() {
		if (workspace)
			workspace->RequestRotateDegrees(180.0f);
	});
	transformMenu->addAction(Translate("FlipHorizontal"), workspace, &ShortsDock::RequestFlipHorizontal);
	transformMenu->addAction(Translate("FlipVertical"), workspace, &ShortsDock::RequestFlipVertical);
	transformMenu->addSeparator();
	transformMenu->addAction(Translate("Crop"), workspace, &ShortsDock::RequestCropDialog);

	menu.addSeparator();
	menu.addAction(Translate("RenameSource"), workspace, &ShortsDock::RequestRenameSource);
	menu.addAction(Translate("DuplicateSource"), workspace, &ShortsDock::RequestDuplicateSource);
	menu.addAction(Translate("CopySource"), workspace, &ShortsDock::RequestCopySource);
	QAction *pasteSrc = menu.addAction(Translate("PasteSource"), workspace, &ShortsDock::RequestPasteSource);
	pasteSrc->setEnabled(workspace->HasSourceClipboard());

	menu.addSeparator();
	QAction *lockAct = menu.addAction(locked ? Translate("UnlockSource") : Translate("LockSource"), workspace,
					  &ShortsDock::RequestToggleSourceLock);
	QAction *visAct = menu.addAction(visible ? Translate("HideSource") : Translate("ShowSource"), workspace,
					 &ShortsDock::RequestToggleSourceVisible);
	Q_UNUSED(lockAct);
	Q_UNUSED(visAct);

	menu.addSeparator();
	QMenu *orderMenu = menu.addMenu(Translate("Order"));
	orderMenu->addAction(Translate("MoveSourceUp"), workspace, &ShortsDock::RequestSourceMoveUp);
	orderMenu->addAction(Translate("MoveSourceDown"), workspace, &ShortsDock::RequestSourceMoveDown);
	orderMenu->addAction(Translate("MoveSourceTop"), workspace, &ShortsDock::RequestSourceMoveTop);
	orderMenu->addAction(Translate("MoveSourceBottom"), workspace, &ShortsDock::RequestSourceMoveBottom);

	menu.addSeparator();
	menu.addAction(Translate("RemoveSource"), workspace, &ShortsDock::RequestRemoveSource);

	menu.exec(globalPos);
}

void VerticalSourcesDock::contextMenuEvent(QContextMenuEvent *event)
{
	ShowSourceContextMenu(event->globalPos());
	event->accept();
}

void VerticalSourcesDock::keyPressEvent(QKeyEvent *event)
{
	if (!workspace) {
		QFrame::keyPressEvent(event);
		return;
	}

	switch (event->key()) {
	case Qt::Key_Delete:
	case Qt::Key_Backspace:
		workspace->RequestRemoveSource();
		event->accept();
		return;
	case Qt::Key_F2:
		workspace->RequestRenameSource();
		event->accept();
		return;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		workspace->RequestSourceProperties();
		event->accept();
		return;
	default:
		break;
	}
	QFrame::keyPressEvent(event);
}
