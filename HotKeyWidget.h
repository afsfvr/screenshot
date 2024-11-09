#ifndef HOTKEYWIDGET_H
#define HOTKEYWIDGET_H

#include <QWidget>
#include <QDebug>

namespace Ui {
class HotKeyWidget;
}

struct HotKey {
    Qt::KeyboardModifiers modifiers;
    quint32 key;
    bool operator==(const HotKey &key) { return this->modifiers == key.modifiers && this->key == key.key; }
};

class HotKeyWidget : public QWidget {
    Q_OBJECT

public:
    explicit HotKeyWidget(HotKey *capture, HotKey *record, QWidget *parent = nullptr);
    ~HotKeyWidget();
protected:
    void showEvent(QShowEvent *event);
private slots:
    void switchHotKey();
    void updateHotKey();
signals:
    void capture();
    void record();
private:
    Ui::HotKeyWidget *ui;
    HotKey *m_capture;
    HotKey *m_record;
    HotKey m_hotkey;
};

#endif // HOTKEYWIDGET_H
