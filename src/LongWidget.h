#ifndef LONGWIDGET_H
#define LONGWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QImage>
#include <QMenu>
#include <thread>
#include <QReadWriteLock>

#include "BlockQueue.h"

class MainWindow;
class LongWidget : public QWidget {
    Q_OBJECT
public:
    explicit LongWidget(const QImage &image, const QRect &rect, const QSize &size, QMenu *menu, MainWindow *main);
    ~LongWidget();

    void showTool();

protected:
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private slots:
    void edit();
    void save();

private:
    friend void mergePicture();
    void init();
    void join();
    QImage screenshot();

    QImage m_image;
    QWidget *m_widget;
    QRect m_screen;
    QSize m_size;
    int m_timer;
    QMenu *m_tray_menu;
    QAction *m_action;
    MainWindow *m_main;

    QReadWriteLock m_lock;
    BlockQueue<QImage> m_queue;
    std::thread *m_thread;
};

#endif // LONGWIDGET_H
