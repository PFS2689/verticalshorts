#include "audio-mixer-panel.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QSpinBox>
#include <cmath>

namespace {

float PeakToNorm(float peakDb)
{
	/* Map approx [-60, 0] dB to [0, 1] for display */
	if (!std::isfinite(peakDb))
		return 0.0f;
	const float n = (peakDb + 60.0f) / 60.0f;
	if (n < 0.0f)
		return 0.0f;
	if (n > 1.0f)
		return 1.0f;
	return n;
}

float MulToDb(float mul)
{
	if (mul < 0.000001f)
		return -60.0f;
	return 20.0f * std::log10(mul);
}

} // namespace

VolumeMeterWidget::VolumeMeterWidget(QWidget *parent) : QWidget(parent)
{
	setMinimumHeight(10);
	setMaximumHeight(14);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void VolumeMeterWidget::SetLevels(float magnitudeNorm, float peakNorm)
{
	magnitude = magnitudeNorm;
	peak = peakNorm;
	update();
}

void VolumeMeterWidget::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(30, 30, 30));
	const int w = width();
	const int h = height();
	const int magW = (int)std::lround(magnitude * w);
	const int peakX = (int)std::lround(peak * w);
	p.fillRect(0, 0, magW, h, QColor(40, 180, 70));
	if (peak > 0.92f)
		p.fillRect(std::max(0, peakX - 2), 0, 2, h, QColor(220, 40, 40));
	else if (peakX > 0)
		p.fillRect(std::max(0, peakX - 2), 0, 2, h, QColor(230, 200, 40));
}

AudioMixerPanel::AudioMixerPanel(QWidget *parent) : QWidget(parent)
{
	layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);
	layout->setSpacing(6);
}

AudioMixerPanel::~AudioMixerPanel()
{
	Clear();
}

void AudioMixerPanel::Clear()
{
	for (auto &row : rows)
		RemoveCallbacks(*row);
	rows.clear();
	QLayoutItem *child;
	while ((child = layout->takeAt(0)) != nullptr) {
		if (child->widget())
			child->widget()->deleteLater();
		delete child;
	}
}

QList<MixerSourceState> AudioMixerPanel::SnapshotStates() const
{
	QList<MixerSourceState> out;
	for (const auto &row : rows) {
		MixerSourceState s;
		s.name = row->name;
		s.uuid = row->uuid;
		if (row->source) {
			s.volume = obs_source_get_volume(row->source);
			s.muted = obs_source_muted(row->source);
			s.hasAudio = (obs_source_get_output_flags(row->source) & OBS_SOURCE_AUDIO) != 0;
		}
		out.push_back(s);
	}
	return out;
}

void AudioMixerPanel::RemoveCallbacks(Row &row)
{
	if (row.volmeter) {
		obs_volmeter_remove_callback(row.volmeter, VolmeterCallback, &row);
		obs_volmeter_destroy(row.volmeter);
		row.volmeter = nullptr;
	}
	row.source = nullptr;
}

void AudioMixerPanel::Refresh()
{
	Clear();

	obs_enum_sources(
		[](void *param, obs_source_t *source) -> bool {
			auto *self = static_cast<AudioMixerPanel *>(param);
			const uint32_t flags = obs_source_get_output_flags(source);
			if ((flags & OBS_SOURCE_AUDIO) == 0)
				return true;
			self->AddSourceRow(source);
			return true;
		},
		this);

	layout->addStretch(1);
}

void AudioMixerPanel::AddSourceRow(obs_source_t *source)
{
	auto row = std::make_unique<Row>();
	row->name = QString::fromUtf8(obs_source_get_name(source));
	const char *uuid = obs_source_get_uuid(source);
	row->uuid = uuid ? QString::fromUtf8(uuid) : row->name;
	row->source = OBSSource(source); /* addrefs */

	auto *widget = new QWidget(this);
	row->widget = widget;
	auto *outer = new QVBoxLayout(widget);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(2);

	auto *top = new QHBoxLayout();
	row->nameLabel = new QLabel(row->name, widget);
	row->nameLabel->setMinimumWidth(64);
	row->muteBtn = new QPushButton(obs_source_muted(source) ? QStringLiteral("Unmute") : QStringLiteral("Mute"),
				       widget);
	row->muteBtn->setCheckable(true);
	row->muteBtn->setChecked(obs_source_muted(source));
	row->dbLabel = new QLabel(widget);
	row->peakLabel = new QLabel(widget);
	row->peakLabel->setMinimumWidth(42);
	top->addWidget(row->nameLabel, 1);
	top->addWidget(row->dbLabel);
	top->addWidget(row->peakLabel);
	top->addWidget(row->muteBtn);
	outer->addLayout(top);

	row->meter = new VolumeMeterWidget(widget);
	outer->addWidget(row->meter);

	row->slider = new QSlider(Qt::Horizontal, widget);
	row->slider->setRange(0, 100);
	row->slider->setValue((int)std::lround(obs_source_get_volume(source) * 100.0f));
	row->dbLabel->setText(QStringLiteral("%1 dB").arg(MulToDb(obs_source_get_volume(source)), 0, 'f', 1));
	outer->addWidget(row->slider);

	Row *raw = row.get();

	connect(row->muteBtn, &QPushButton::toggled, this, [this, raw](bool checked) {
		if (!raw->source)
			return;
		obs_source_set_muted(raw->source, checked);
		raw->muteBtn->setText(checked ? QStringLiteral("Unmute") : QStringLiteral("Mute"));
		emit sourceMutedChanged(raw->name, checked);
	});
	connect(row->slider, &QSlider::valueChanged, this, [this, raw](int value) {
		if (!raw->source)
			return;
		const float vol = (float)value / 100.0f;
		obs_source_set_volume(raw->source, vol);
		raw->dbLabel->setText(QStringLiteral("%1 dB").arg(MulToDb(vol), 0, 'f', 1));
		emit sourceVolumeChanged(raw->name, vol);
	});

	widget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(widget, &QWidget::customContextMenuRequested, this, [this, raw, widget](const QPoint &pos) {
		ShowContextMenu(raw, widget->mapToGlobal(pos));
	});

	row->volmeter = obs_volmeter_create(OBS_FADER_LOG);
	if (row->volmeter) {
		obs_volmeter_attach_source(row->volmeter, source);
		obs_volmeter_add_callback(row->volmeter, VolmeterCallback, raw);
	}

	layout->addWidget(widget);
	rows.push_back(std::move(row));
}

void AudioMixerPanel::VolmeterCallback(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
				       const float peak[MAX_AUDIO_CHANNELS], const float *)
{
	auto *row = static_cast<Row *>(param);
	if (!row || !row->widget)
		return;
	float mag = magnitude[0];
	float pk = peak[0];
	for (int i = 1; i < MAX_AUDIO_CHANNELS; i++) {
		mag = std::max(mag, magnitude[i]);
		pk = std::max(pk, peak[i]);
	}
	const QString uuid = row->uuid;
	Q_UNUSED(uuid);

	/* Marshal to UI thread via the row widget */
	QMetaObject::invokeMethod(
		row->widget,
		[row, mag, pk]() {
			if (!row->meter)
				return;
			row->meter->SetLevels(PeakToNorm(mag), PeakToNorm(pk));
			if (row->peakLabel)
				row->peakLabel->setText(QStringLiteral("%1").arg(pk, 0, 'f', 1));
		},
		Qt::QueuedConnection);
}

void AudioMixerPanel::OnMeterLevels(const QString &, float, float) {}

void AudioMixerPanel::ShowContextMenu(Row *row, const QPoint &globalPos)
{
	if (!row || !row->source)
		return;
	QMenu menu(this);
	menu.addAction(QStringLiteral("Filters…"), this, [this, row]() { OpenFilters(row); });
	menu.addAction(QStringLiteral("Properties…"), this, [this, row]() { OpenProperties(row); });
	menu.addAction(QStringLiteral("Advanced Audio Properties…"), this, [this, row]() { OpenAdvancedAudio(row); });
	menu.addSeparator();
	menu.addAction(QStringLiteral("Rename…"), this, [this, row]() {
		bool ok = false;
		const QString name = QInputDialog::getText(this, QStringLiteral("Rename"), QStringLiteral("Name"),
							   QLineEdit::Normal, row->name, &ok);
		if (!ok || name.trimmed().isEmpty() || !row->source)
			return;
		obs_source_set_name(row->source, name.trimmed().toUtf8().constData());
		row->name = name.trimmed();
		row->nameLabel->setText(row->name);
	});
	menu.exec(globalPos);
}

void AudioMixerPanel::OpenFilters(Row *row)
{
	if (row && row->source)
		obs_frontend_open_source_filters(row->source);
}

void AudioMixerPanel::OpenProperties(Row *row)
{
	if (row && row->source)
		obs_frontend_open_source_properties(row->source);
}

void AudioMixerPanel::OpenAdvancedAudio(Row *row)
{
	if (!row || !row->source)
		return;

	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("Advanced Audio — %1").arg(row->name));
	auto *form = new QFormLayout(&dlg);

	auto *balance = new QSlider(Qt::Horizontal, &dlg);
	balance->setRange(0, 100);
	balance->setValue((int)std::lround(obs_source_get_balance_value(row->source) * 100.0f));
	form->addRow(QStringLiteral("Balance"), balance);

	auto *syncMs = new QSpinBox(&dlg);
	syncMs->setRange(-10000, 10000);
	syncMs->setSuffix(QStringLiteral(" ms"));
	syncMs->setValue((int)(obs_source_get_sync_offset(row->source) / 1000000));
	form->addRow(QStringLiteral("Sync Offset"), syncMs);

	auto *mono = new QCheckBox(QStringLiteral("Force Mono"), &dlg);
#ifdef OBS_SOURCE_FLAG_FORCE_MONO
	mono->setChecked((obs_source_get_flags(row->source) & OBS_SOURCE_FLAG_FORCE_MONO) != 0);
#else
	mono->setEnabled(false);
	mono->setToolTip(QStringLiteral("Force mono flag unavailable in this OBS build."));
#endif
	form->addRow(mono);

	auto *monitor = new QComboBox(&dlg);
	monitor->addItem(QStringLiteral("None"), (int)OBS_MONITORING_TYPE_NONE);
	monitor->addItem(QStringLiteral("Monitor Only"), (int)OBS_MONITORING_TYPE_MONITOR_ONLY);
	monitor->addItem(QStringLiteral("Monitor and Output"), (int)OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
	const int curMon = (int)obs_source_get_monitoring_type(row->source);
	for (int i = 0; i < monitor->count(); ++i) {
		if (monitor->itemData(i).toInt() == curMon) {
			monitor->setCurrentIndex(i);
			break;
		}
	}
	form->addRow(QStringLiteral("Audio Monitoring"), monitor);

	const uint32_t mixers = obs_source_get_audio_mixers(row->source);
	QCheckBox *tracks[6] = {};
	auto *trackRow = new QHBoxLayout();
	for (int i = 0; i < 6; i++) {
		tracks[i] = new QCheckBox(QString::number(i + 1), &dlg);
		tracks[i]->setChecked((mixers & (1u << i)) != 0);
		trackRow->addWidget(tracks[i]);
	}
	form->addRow(QStringLiteral("Tracks"), trackRow);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	if (dlg.exec() != QDialog::Accepted)
		return;

	obs_source_set_balance_value(row->source, (float)balance->value() / 100.0f);
	obs_source_set_sync_offset(row->source, (int64_t)syncMs->value() * 1000000);
	obs_source_set_monitoring_type(row->source,
				       (obs_monitoring_type)monitor->currentData().toInt());
	uint32_t newMix = 0;
	for (int i = 0; i < 6; i++) {
		if (tracks[i]->isChecked())
			newMix |= (1u << i);
	}
	obs_source_set_audio_mixers(row->source, newMix);

#ifdef OBS_SOURCE_FLAG_FORCE_MONO
	uint32_t flags = obs_source_get_flags(row->source);
	if (mono->isChecked())
		flags |= OBS_SOURCE_FLAG_FORCE_MONO;
	else
		flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
	obs_source_set_flags(row->source, flags);
#endif
}
