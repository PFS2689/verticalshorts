#pragma once

#include <obs.hpp>
#include <obs-audio-controls.h>

#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <vector>

class VolumeMeterWidget;

struct MixerSourceState {
	QString name;
	QString uuid;
	float volume = 1.0f;
	bool muted = false;
	bool hasAudio = true;
};

class AudioMixerPanel : public QWidget {
	Q_OBJECT
public:
	explicit AudioMixerPanel(QWidget *parent = nullptr);
	~AudioMixerPanel() override;

	void Refresh();
	void Clear();
	QList<MixerSourceState> SnapshotStates() const;

signals:
	void sourceMutedChanged(const QString &name, bool muted);
	void sourceVolumeChanged(const QString &name, float volume);

private slots:
	void OnMeterLevels(const QString &uuid, float magnitude, float peak);

private:
	struct Row {
		QString name;
		QString uuid;
		OBSSource source;
		obs_volmeter_t *volmeter = nullptr;
		QWidget *widget = nullptr;
		VolumeMeterWidget *meter = nullptr;
		QSlider *slider = nullptr;
		QPushButton *muteBtn = nullptr;
		QLabel *nameLabel = nullptr;
		QLabel *dbLabel = nullptr;
		QLabel *peakLabel = nullptr;
	};

	void AddSourceRow(obs_source_t *source);
	void RemoveCallbacks(Row &row);
	void ShowContextMenu(Row *row, const QPoint &globalPos);
	void OpenFilters(Row *row);
	void OpenProperties(Row *row);
	void OpenAdvancedAudio(Row *row);
	static void VolmeterCallback(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
				     const float peak[MAX_AUDIO_CHANNELS],
				     const float input_peak[MAX_AUDIO_CHANNELS]);

	QVBoxLayout *layout = nullptr;
	std::vector<std::unique_ptr<Row>> rows;
};

/* Lightweight meter bar painted from OBS volmeter peaks (UI thread). */
class VolumeMeterWidget : public QWidget {
	Q_OBJECT
public:
	explicit VolumeMeterWidget(QWidget *parent = nullptr);
	void SetLevels(float magnitudeNorm, float peakNorm);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	float magnitude = 0.0f;
	float peak = 0.0f;
};
