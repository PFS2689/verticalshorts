#include "vertical-sources-dock.hpp"
#include "shorts-dock.hpp"

#include <obs-module.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSize>
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kNameRole = Qt::UserRole + 4;

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

QIcon VisibilityIcon(const QWidget *w, bool visible)
{
	const QStyle *style = w->style();
	if (visible) {
		QIcon icon = QIcon::fromTheme(QStringLiteral("view-visible"));
		if (icon.isNull())
			icon = QIcon::fromTheme(QStringLiteral("visibility"));
		if (icon.isNull())
			icon = style->standardIcon(QStyle::SP_DialogYesButton);
		return icon;
	}
	QIcon icon = QIcon::fromTheme(QStringLiteral("view-hidden"));
	if (icon.isNull())
		icon = QIcon::fromTheme(QStringLiteral("hint"));
	if (icon.isNull())
		icon = style->standardIcon(QStyle::SP_DialogNoButton);
	return icon;
}

QIcon LockIcon(const QWidget *w, bool locked)
{
	const QStyle *style = w->style();
	if (locked) {
		QIcon icon = QIcon::fromTheme(QStringLiteral("object-locked"));
		if (icon.isNull())
			icon = QIcon::fromTheme(QStringLiteral("lock"));
		if (icon.isNull())
			icon = style->standardIcon(QStyle::SP_BrowserStop);
		return icon;
	}
	QIcon icon = QIcon::fromTheme(QStringLiteral("object-unlocked"));
	if (icon.isNull())
		icon = QIcon::fromTheme(QStringLiteral("unlock"));
	if (icon.isNull())
		icon = style->standardIcon(QStyle::SP_ArrowForward);
	return icon;
}

/* Eliding label: takes remaining row width; never paints over siblings. */
class ElidedNameLabel : public QLabel {
public:
	explicit ElidedNameLabel(QWidget *parent = nullptr) : QLabel(parent)
	{
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		setTextFormat(Qt::PlainText);
		setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
	}

	void setFullText(const QString &text)
	{
		fullText = text;
		updateElide();
	}

protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QLabel::resizeEvent(event);
		updateElide();
	}

private:
	void updateElide()
	{
		const QFontMetrics fm(font());
		const QString elided = fm.elidedText(fullText, Qt::ElideRight, qMax(0, width()));
		QLabel::setText(elided);
		setToolTip(elided != fullText ? fullText : QString());
	}

	QString fullText;
};

class SourceRowWidget : public QWidget {
public:
	SourceRowWidget(qint64 itemId_, bool visible, bool locked, const QString &name, ShortsDock *workspace_,
			QWidget *parent = nullptr)
		: QWidget(parent),
		  itemId(itemId_),
		  workspace(workspace_)
	{
		setObjectName(QStringLiteral("VerticalSourceRow"));
		setAutoFillBackground(false);

		auto *lay = new QHBoxLayout(this);
		lay->setContentsMargins(4, 1, 6, 1);
		lay->setSpacing(6);
		lay->setAlignment(Qt::AlignVCenter);

		const int iconPx = qMax(16, fontMetrics().height());
		const QSize iconSize(iconPx, iconPx);

		visBtn = new QToolButton(this);
		visBtn->setObjectName(QStringLiteral("SourceVisibilityButton"));
		visBtn->setAutoRaise(true);
		visBtn->setFocusPolicy(Qt::NoFocus);
		visBtn->setIconSize(iconSize);
		visBtn->setFixedSize(iconPx + 6, iconPx + 4);
		visBtn->setIcon(VisibilityIcon(this, visible));
		visBtn->setToolTip(Translate(visible ? "HideSource" : "ShowSource"));
		visBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

		lockBtn = new QToolButton(this);
		lockBtn->setObjectName(QStringLiteral("SourceLockButton"));
		lockBtn->setAutoRaise(true);
		lockBtn->setFocusPolicy(Qt::NoFocus);
		lockBtn->setIconSize(iconSize);
		lockBtn->setFixedSize(iconPx + 6, iconPx + 4);
		lockBtn->setIcon(LockIcon(this, locked));
		lockBtn->setToolTip(Translate(locked ? "UnlockSource" : "LockSource"));
		lockBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

		nameLabel = new ElidedNameLabel(this);
		nameLabel->setFullText(name);
		if (!visible) {
			QPalette pal = nameLabel->palette();
			pal.setColor(QPalette::WindowText, pal.color(QPalette::Disabled, QPalette::WindowText));
			nameLabel->setPalette(pal);
		}

		lay->addWidget(visBtn, 0, Qt::AlignVCenter);
		lay->addWidget(lockBtn, 0, Qt::AlignVCenter);
		lay->addWidget(nameLabel, 1);

		connect(visBtn, &QToolButton::clicked, this, [this]() {
			if (workspace)
				workspace->RequestToggleSourceVisibleById(itemId);
		});
		connect(lockBtn, &QToolButton::clicked, this, [this]() {
			if (workspace)
				workspace->RequestToggleSourceLockById(itemId);
		});
	}

	QSize sizeHint() const override
	{
		const int h = qMax(visBtn ? visBtn->sizeHint().height() : 20, fontMetrics().height()) + 6;
		return QSize(200, h);
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
	ElidedNameLabel *nameLabel = nullptr;
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
	sourcesList->setSpacing(0);
	sourcesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	sourcesList->setTextElideMode(Qt::ElideNone);

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
		const QString name = item->data(kNameRole).toString();
		/* Critical: keep DisplayRole empty so QListWidget does not paint text over the row widget. */
		item->setText(QString());
		item->setData(Qt::DisplayRole, QVariant());
		item->setToolTip(name);

		auto *row = new SourceRowWidget(id, visible, locked, name, workspace, sourcesList);
		item->setSizeHint(row->sizeHint());
		sourcesList->setItemWidget(item, row);
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
	QAction *editTf =
		transformMenu->addAction(Translate("EditTransform"), workspace, &ShortsDock::RequestEditTransform);
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
	menu.addAction(visible ? Translate("HideSource") : Translate("ShowSource"), workspace,
		       &ShortsDock::RequestToggleSourceVisible);
	menu.addAction(locked ? Translate("UnlockSource") : Translate("LockSource"), workspace,
		       &ShortsDock::RequestToggleSourceLock);

	menu.addSeparator();
	menu.addAction(Translate("MoveSourceUp"), workspace, &ShortsDock::RequestSourceMoveUp);
	menu.addAction(Translate("MoveSourceDown"), workspace, &ShortsDock::RequestSourceMoveDown);
	menu.addAction(Translate("MoveSourceTop"), workspace, &ShortsDock::RequestSourceMoveTop);
	menu.addAction(Translate("MoveSourceBottom"), workspace, &ShortsDock::RequestSourceMoveBottom);

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
