#include "SettingWidget.h"
#include "ui_SettingWidget.h"

#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>


QDataStream& operator<<(QDataStream& stream, const HotKey &key) {
    stream << static_cast<quint32>(key.modifiers) << key.key;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, HotKey &key) {
    quint32 tmp;
    stream >> tmp;
    key.modifiers = static_cast<Qt::KeyboardModifiers>(tmp);
    stream >> key.key;
    return stream;
}

SettingWidget::SettingWidget(QWidget *parent): QWidget(parent), ui(new Ui::SettingWidget) {
    ui->setupUi(this);

    connect(ui->cancel, &QPushButton::clicked, this, &SettingWidget::hide);
    connect(ui->ok, &QPushButton::clicked, this, &SettingWidget::confirm);
    connect(ui->savePath, &QLabel::linkActivated, this, &SettingWidget::openLink);
    connect(ui->autoSavePath, &QLabel::linkActivated, this, &SettingWidget::openLink);
}

SettingWidget::~SettingWidget() {
    saveConfig();
    delete ui;
}

void SettingWidget::readConfig() {
    QFile file{getConfigPath()};
    if (file.open(QFile::ReadOnly)) {
        QByteArray array = file.readAll();
        file.close();
        QDataStream stream{array};
        stream >> m_auto_save_key >> m_auto_save_path >> m_auto_save_mode;
        stream >> m_capture >> m_record;
        checkData(m_auto_save_key);
        checkData(m_capture);
        checkData(m_record);
    } else {
        qWarning() << "打开配置文件失败：" << file.errorString();
    }
}

void SettingWidget::saveConfig() {
    QFile file{getConfigPath()};
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QByteArray array;
        QDataStream stream{array};
        stream << m_auto_save_key << m_auto_save_path << m_auto_save_mode;
        stream << m_capture << m_record;
        file.write(array);
        file.close();
    } else {
        QString error = QString("write config failed: %1").arg(file.errorString());
        qWarning() << error;
    }
}

const QString& SettingWidget::getConfigPath() const {
    static QString applicationPath = QDir::toNativeSeparators(QApplication::applicationDirPath() + QDir::separator() + QApplication::applicationName() + ".data");
    if (QFile::exists(applicationPath)) {
        return applicationPath;
    } else {
        static QString userPath = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        if (! QFile::exists(userPath)) {
            qDebug() << QString("创建文件夹%1: %2").arg(userPath, QDir{}.mkdir(userPath) ? "成功" : "失败");
        }
        return userPath + QDir::separator() + QApplication::applicationName() + ".data";;
    }
}

void SettingWidget::cleanAutoSaveKey() {
    m_auto_save_key = {};
    updateKey1(m_auto_save_key);
}

void SettingWidget::cleanCaptureKey() {
    m_capture = {};
    updateKey2(m_capture);
}

void SettingWidget::cleanRecordKey() {
    m_record = {};
    updateKey3(m_record);
}

void SettingWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    const QString &path = getConfigPath();
    ui->savePath->setText(QString("<a href=\"%1\" style=\"color:black; text-decoration: none;\">配置路径: %2</a>").arg(path, QFileInfo{path}.absolutePath()));
    updateKey1(m_auto_save_key);
    ui->saveMode->setCurrentIndex(m_auto_save_mode);
    if (m_auto_save_path.length() == 0) {
        ui->autoSavePath->setText(QString("保存到: <a href=\"%1\" style=\"color:black; text-decoration: none;\">%2</a>").arg(m_auto_save_path, QFileInfo{m_auto_save_path}.absolutePath()));
    } else {
        ui->autoSavePath->setText("保存到: 未设置");
    }
    updateKey2(m_capture);
    updateKey3(m_record);
}

void SettingWidget::openLink(const QString &link) {
#if defined(Q_OS_LINUX)
    if (system(QString("nautilus %1 >/dev/null 2>&1 &").arg(link).toStdString().c_str()) != 0) {
        if (system(QString("thunar %1 >/dev/null 2>&1 &").arg(link).toStdString().c_str()) != 0) {
            QFileInfo info{link};
            QString path = info.path();
            if (system(QString("dolphin %1 >/dev/null 2>&1 &").arg(path).toStdString().c_str()) != 0) {
                qDebug() << system(QString("xdg-open %1 >/dev/null 2>&1 &").arg(path).toStdString().c_str());
            }
        }
    }
#else
    QProcess process;
    process.setProgram("cmd.exe");
    if (QFile::exists(link)) {
        process.setArguments({"/C", QString("explorer /select,%1 >NUL 2>&1").arg(link)});
    } else {
        process.setArguments({"/C", QString("explorer %1 >NUL 2>&1").arg(QDir::toNativeSeparators(QFileInfo(link).path()))});
    }
    process.start();
    process.waitForFinished();
    // system(QString("explorer /select,%1 >NUL 2>&1").arg(link).toStdString().c_str());
#endif
}

void SettingWidget::confirm() {
    HotKey key1;
    HotKey key2;
    HotKey key3;

    m_auto_save_mode = ui->saveMode->currentIndex();
    m_auto_save_path = ui->savePath->text().trimmed();

    QString msg = "是否删除";
    key1.modifiers = Qt::NoModifier;
    if (ui->control1->isChecked()) {
        key1.modifiers |= Qt::ControlModifier;
    }
    if (ui->alt1->isChecked()) {
        key1.modifiers |= Qt::AltModifier;
    }
    if (ui->shift1->isChecked()) {
        key1.modifiers |= Qt::ShiftModifier;
    }
    key1.key = 'A' + ui->key1->currentIndex();
    checkData(key1);
    if (m_auto_save_path.length() == 0) {
        key1.modifiers = Qt::NoModifier;
    }
    if (key1 != m_auto_save_key) {
        if (key1.modifiers == Qt::NoModifier) {
            msg.append("自动保存、");
        }
    }

    key2.modifiers = Qt::NoModifier;
    if (ui->control2->isChecked()) {
        key2.modifiers |= Qt::ControlModifier;
    }
    if (ui->alt2->isChecked()) {
        key2.modifiers |= Qt::AltModifier;
    }
    if (ui->shift2->isChecked()) {
        key2.modifiers |= Qt::ShiftModifier;
    }
    key2.key = 'A' + ui->key2->currentIndex();
    checkData(key2);
    if (key2 != m_capture) {
        if (key2.modifiers == Qt::NoModifier) {
            msg.append("截图、");
        }
    }

    key3.modifiers = Qt::NoModifier;
    if (ui->control3->isChecked()) {
        key3.modifiers |= Qt::ControlModifier;
    }
    if (ui->alt3->isChecked()) {
        key3.modifiers |= Qt::AltModifier;
    }
    if (ui->shift3->isChecked()) {
        key3.modifiers |= Qt::ShiftModifier;
    }
    key3.key = 'A' + ui->key3->currentIndex();
    checkData(key3);
    if (key3 != m_record) {
        if (key2.modifiers == Qt::NoModifier) {
            msg.append("GIF、");
        }
    }

    if (msg.back() == QChar(0x3001)) {
        msg.remove(msg.length() - 1, 2);
        if (QMessageBox::question(this, "是否删除快捷键", msg) != QMessageBox::Yes) {
            return;
        }
    }

    if (key1 != m_auto_save_key) {
        m_auto_save_key = key1;
        emit autoSaveChanged(m_auto_save_key, m_auto_save_mode, m_auto_save_path);
    }

    if (key2 != m_capture) {
        m_capture = key2;
        emit captureChanged(m_capture);
    }

    if (key3 != m_record) {
        m_record = key3;
        emit recordChanged(m_record);
    }
}

void SettingWidget::checkData(HotKey &key) {
    if ((key.modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) == Qt::NoModifier
        || m_record.key < 'A' || m_record.key > 'Z') {
        key.modifiers = Qt::NoModifier;
        key.key = 'A';
    }
}

void SettingWidget::updateKey1(const HotKey &key) {
    ui->control1->setChecked(key.modifiers & Qt::ControlModifier);
    ui->alt1->setChecked(key.modifiers & Qt::AltModifier);
    ui->shift1->setChecked(key.modifiers & Qt::ShiftModifier);
    ui->key1->setCurrentIndex(key.key - 'A');
}

void SettingWidget::updateKey2(const HotKey &key) {
    ui->control2->setChecked(key.modifiers & Qt::ControlModifier);
    ui->alt2->setChecked(key.modifiers & Qt::AltModifier);
    ui->shift2->setChecked(key.modifiers & Qt::ShiftModifier);
    ui->key2->setCurrentIndex(key.key - 'A');
}

void SettingWidget::updateKey3(const HotKey &key) {
    ui->control3->setChecked(key.modifiers & Qt::ControlModifier);
    ui->alt3->setChecked(key.modifiers & Qt::AltModifier);
    ui->shift3->setChecked(key.modifiers & Qt::ShiftModifier);
    ui->key3->setCurrentIndex(key.key - 'A');
}
