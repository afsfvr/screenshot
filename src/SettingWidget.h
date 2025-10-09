#ifndef SETTINGWIDGET_H
#define SETTINGWIDGET_H

#include <QWidget>
#include <QDebug>
#include <QDataStream>
#include <QProcess>

namespace Ui {
class SettingWidget;
}

struct HotKey {
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    quint32 key = 'A';
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
    inline const QString& saveFormat() const { return m_save_format; }
    inline bool fullScreen() const { return m_full_screen; }
    inline const HotKey& capture() const { return m_capture; }
    inline const HotKey& record() const { return m_record; }
    inline bool scaleCtrl() const { return m_scale_ctrl; }

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void openFile(const QString &link);
    void choosePath(const QString &link);
    void confirm();
    void keyHelp();

private:
    void checkData(HotKey &key);
    void updateKey1();
    void updateKey2();
    void updateKey3();
    QString setSelfStart(bool start, bool allUser);
    bool isSelfStart(bool allUser);
#if defined(Q_OS_WINDOWS)
    QString getWindowsError(quint32 error) const;
#elif defined(Q_OS_LINUX)
    QString getUserHomePath() const;
#endif

signals:
    void autoSaveChanged(const HotKey &key, quint8 mode, const QString &path);
    void captureChanged(const HotKey &key);
    void recordChanged(const HotKey &key);
    void scaleKeyChanged(bool value);

private:
    Ui::SettingWidget *ui;
    HotKey m_auto_save_key;
    QString m_auto_save_path;
    QString m_save_format;
    bool m_full_screen;
    HotKey m_capture;
    HotKey m_record;
    bool m_scale_ctrl;
};

#endif // SETTINGWIDGET_H
