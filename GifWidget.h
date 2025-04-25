#ifndef GIFWIDGET_H
#define GIFWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QTimerEvent>
#include <QSpinBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QQueue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "gif.h"

class QComboBox;
struct GifFrameData {
    GifWriter* writer;
    uint8_t* image;
    int width;
    int height;
    int delay;
};

void func(std::atomic<bool> *run, std::mutex *mutex, std::condition_variable *cond, QQueue<GifFrameData> *queue);

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
    void updateGIF();
    void init();
    QImage screenshot();
    void start();

    QString m_tmp;
    QString m_path;
    GifWriter *m_writer;
    int m_timerId;
    int m_updateTimerId;
    int m_delay;
    QRect m_screen;
    QSize m_size;
    qint64 m_startTime;
    qint64 m_preTime;
    QMenu *m_menu;
    QMenu *system_menu;
    QAction *m_action;

    QWidget *m_widget;
    QHBoxLayout *m_layout;
    QPushButton *m_button;
    QSpinBox *m_spin;
    QLabel *m_label;
    QComboBox *m_box;


    std::atomic<bool> m_run;
    std::thread *m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    QQueue<GifFrameData> m_queue;
};

#endif // GIFWIDGET_H
