#ifndef TOPWIDGET_H
#define TOPWIDGET_H

#include <QWidget>
#include <QMenu>
#include <QTextEdit>
#include <QLabel>

#include "BaseWindow.h"
#ifdef OCR
#include "Ocr.h"
#endif

class TopWidget : public BaseWindow {
    Q_OBJECT
public:
    explicit TopWidget(QImage &image, const QRect &rect, QMenu *menu);
    explicit TopWidget(QImage &&image, QVector<Shape*> &vector, const QRect &rect, QMenu *menu);
    explicit TopWidget(QImage &image, QPainterPath &&path, QVector<Shape*> &vector, const QRect &rect, QMenu *menu);
    virtual ~TopWidget();
    void showTool();

public slots:
#ifdef OCR
    void ocrStart();
    void ocrEnd(const QVector<Ocr::OcrResult> &result);
#endif
#ifdef Q_OS_LINUX
    void mouseRelease(QSharedPointer<QMouseEvent> event);
#endif

protected:
#ifdef OCR
    bool eventFilter(QObject *watched, QEvent *event) override;
#endif
    void timerEvent(QTimerEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    inline void mouseDoubleClickEvent(QMouseEvent *event) override { QWidget::mouseDoubleClickEvent(event); }
    void focusOutEvent(QFocusEvent *event) override;
    inline bool isValid() const override { return true; }
    QRect getGeometry() const override;

private slots:
    void save(const QString &path="") override;
    void end() override;
    void topChange(bool top);
    void moveTop();
#ifdef OCR
    void copyText();
    void editText();
#endif

private:
    void init();
    bool contains(const QPoint &point);

private:
    QMenu *tray_menu;
    QMenu *m_menu;

#ifdef OCR
    void hideWidget();
    int m_ocr_timer = -1;
    QPoint m_center;
    int m_radius, m_radius1, m_angle;
    QVector<Ocr::OcrResult> m_ocr;
    QWidget *m_widget = nullptr;
    QTextEdit *m_text = nullptr;
    QLabel *m_label = nullptr;
    QPushButton *m_button = nullptr;
#endif

#ifdef Q_OS_LINUX
    bool m_move = false;
#endif

    int m_offsetY = 0;
    const int m_max_offset = 0;
    int m_scroll_timer = -1;
};

#endif // TOPWIDGET_H
