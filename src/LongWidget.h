#ifndef LONGWIDGET_H
#define LONGWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QImage>

class LongWidget : public QWidget {
    Q_OBJECT
public:
    explicit LongWidget(const QImage &image, const QRect &rect, const QSize &size, QWidget *parent = nullptr);
    ~LongWidget();

    void showTool();

protected:
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private slots:
    void edit();
    void save();

private:
    void init();
    void join();
    QImage screenshot();

    QImage m_image;
    QWidget *m_widget;
    QRect m_screen;
    QSize m_size;
    int m_id;
};

#endif // LONGWIDGET_H
