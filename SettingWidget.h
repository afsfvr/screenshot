#ifndef SETTINGWIDGET_H
#define SETTINGWIDGET_H

#include <QWidget>
#include <QDebug>
#include <QDataStream>

namespace Ui {
class SettingWidget;
}

struct HotKey {
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    quint32 key = 0;
    inline bool operator==(const HotKey &key) { return this->modifiers == key.modifiers && this->key == key.key; }
    inline bool operator!=(const HotKey &key) { return this->modifiers != key.modifiers || this->key != key.key; }
    friend QDataStream& operator<<(QDataStream& stream, const HotKey &key);
    friend QDataStream& operator>>(QDataStream& stream, HotKey &key);
};

class SettingWidget : public QWidget {
    Q_OBJECT

public:
    explicit SettingWidget(QWidget *parent = nullptr);
    ~SettingWidget();

    void readConfig();
    void saveConfig();
    const QString& getConfigPath() const;
    void cleanAutoSaveKey();
    void cleanCaptureKey();
    void cleanRecordKey();
    void setConfigPath(const QString &path);

    inline const HotKey& autoSave() const { return m_auto_save_key; }
    inline const QString& autoSavePath() const { return m_auto_save_path; }
    inline bool autoSaveMode() const { return m_auto_save_mode; }
    inline const HotKey& capture() const { return m_capture; }
    inline const HotKey& record() const { return m_record; }

protected:
    void showEvent(QShowEvent *event);

private slots:
    void openLink(const QString &link);
    void confirm();

private:
    void checkData(HotKey &key);
    void updateKey1(const HotKey &key);
    void updateKey2(const HotKey &key);
    void updateKey3(const HotKey &key);

signals:
    void autoSaveChanged(const HotKey &key, bool mode, const QString &path);
    void captureChanged(const HotKey &key);
    void recordChanged(const HotKey &key);

private:
    Ui::SettingWidget *ui;
    HotKey m_auto_save_key;
    QString m_auto_save_path;
    bool m_auto_save_mode;
    HotKey m_capture;
    HotKey m_record;
};

#endif // SETTINGWIDGET_H
