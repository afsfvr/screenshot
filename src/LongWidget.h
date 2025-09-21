#ifndef LONGWIDGET_H
#define LONGWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QImage>
#include <QMenu>
#include <thread>
#include <QReadWriteLock>
#include <QLabel>

#include "BlockQueue.h"

class MainWindow;
class LongWidget : public QWidget {
    Q_OBJECT
public:
    struct Data {
        QImage image;
        bool down;
    };
    explicit LongWidget(const QImage &image, const QRect &rect, const QSize &size, QMenu *menu, MainWindow *main, qreal ratio);
    ~LongWidget();

    void showTool();

public slots:
    void mouseWheel(bool down);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void edit();
    void save();
    void updateLabel();

private:
    void init();
    void join();
    void stop();
    QImage screenshot();
    QRect getScreenRect(const QRect &rect);

    QImage m_image;
    QWidget *m_widget;
    QLabel *m_label;
    QRect m_screen;
    QSize m_size;

    QMenu *m_tray_menu;
    QAction *m_action;
    MainWindow *m_main;

    QReadWriteLock m_lock;
    BlockQueue<Data> m_queue;
    std::thread *m_thread;
    qreal m_ratio;
};

#endif // LONGWIDGET_H
