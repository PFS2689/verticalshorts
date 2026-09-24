#include "vertical-production-dock.hpp"
#include "shorts-dock.hpp"
#include "vertical-scenes-dock.hpp"
#include "vertical-sources-dock.hpp"
#include "vertical-transitions-dock.hpp"

#include <obs-module.h>

#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

QString Translate(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QWidget *WrapSection(QWidget *parent, const QString &title, QWidget *body)
{
	auto *wrap = new QWidget(parent);
	auto *lay = new QVBoxLayout(wrap);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(2);
	auto *label = new QLabel(title, wrap);
	QFont f = label->font();
	f.setBold(true);
	label->setFont(f);
	lay->addWidget(label, 0);
	lay->addWidget(body, 1);
	return wrap;
}

} // namespace

VerticalProductionDock::VerticalProductionDock(ShortsDock *workspace_, QWidget *parent)
	: QFrame(parent),
	  workspace(workspace_)
{
	setObjectName(QStringLiteral("VerticalProductionDock"));
	BuildUI();
}

void VerticalProductionDock::BuildUI()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(4, 4, 4, 4);
	root->setSpacing(4);

	splitter = new QSplitter(Qt::Vertical, this);
	splitter->setChildrenCollapsible(false);
	splitter->setHandleWidth(6);

	scenesPanel = new VerticalScenesDock(workspace, splitter);
	sourcesPanel = new VerticalSourcesDock(workspace, splitter);
	transitionsPanel = new VerticalTransitionsDock(workspace, splitter);

	splitter->addWidget(WrapSection(splitter, Translate("VerticalScenesSection"), scenesPanel));
	splitter->addWidget(WrapSection(splitter, Translate("VerticalSourcesSection"), sourcesPanel));
	splitter->addWidget(WrapSection(splitter, Translate("VerticalTransitionsSection"), transitionsPanel));

	/* Prefer more space for scenes/sources; transitions stay compact. */
	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 3);
	splitter->setStretchFactor(2, 1);
	splitter->setSizes({180, 280, 120});

	root->addWidget(splitter, 1);
}
