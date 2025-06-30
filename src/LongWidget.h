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
    explicit LongWidget(const QImage &image, const QRect &rect, const QSize &size, QMenu *menu, MainWindow *main);
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
};

#endif // LONGWIDGET_H
