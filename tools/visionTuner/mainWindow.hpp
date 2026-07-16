#pragma once

#include "pipelineStep.hpp"

#include <QImage>
#include <QMainWindow>
#include <QPushButton>
#include <QWidget>

#include <functional>
#include <opencv2/core/mat.hpp>

class QComboBox;

namespace tengen {

class CvMatrixView : public QWidget {
public:
	explicit CvMatrixView(QWidget* parent = nullptr);
	void setMat(const cv::Mat& mat);
	bool saveImage(const QString& filename);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	static QImage matToQImage(const cv::Mat& mat);

	QImage m_image{};
};

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow() override;

	void setImage(const cv::Mat& image);

signals:
	void imageSourceChanged(ImageSource source);
	void videoCaptureClicked();
	void videoModeChanged(VideoMode mode);
	void pipelineStepChanged(PipelineStep step);

private slots:
	void onPipelineStepChange();
	void onSourceChange();
	void onSaveClicked();

private:
	void buildLayout();

private:
	CvMatrixView* m_matrixView{nullptr};
	QComboBox* m_sourceCombo{nullptr};
	QPushButton* m_captureButton{nullptr};
	QPushButton* m_saveImageButton{nullptr};

	QComboBox* m_stepCombo{nullptr};
	std::function<void(PipelineStep)> m_stepChangedCallback{};
};

} // namespace tengen
