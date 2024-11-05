#ifndef GIFWIDGET_H
#define GIFWIDGET_H

#include <QWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimerEvent>
#include <QSpinBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>

#include "gif.h"

class GifWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GifWidget(const QSize &screenSize, const QRect &rect, QMenu *menu, QWidget *parent = nullptr);
    ~GifWidget();
protected:
    void paintEvent(QPaintEvent *event);
    void timerEvent(QTimerEvent *event);
private slots:
    void buttonClicked();
private:
    void initUI();
    QImage screenshot();
    void start();

    QString m_tmp;
    GifWriter *m_writer;
    int m_timerId;
    int m_delay;
    QRect m_screen;
    QSize m_size;
    qint64 m_startTime;
    QMenu *m_menu;
    QAction *m_action;

    QWidget *m_widget;
    QHBoxLayout *m_layout;
    QPushButton *m_button;
    QSpinBox *m_spin;
    QLabel *m_label;
};

#endif // GIFWIDGET_H
