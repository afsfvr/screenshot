#include "SettingWidget.h"
#include "ui_SettingWidget.h"

#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QFileDialog>
#include <QImageWriter>

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
    setWindowTitle("设置");

    connect(ui->cancel, &QPushButton::clicked, this, &SettingWidget::hide);
    connect(ui->ok, &QPushButton::clicked, this, &SettingWidget::confirm);
    connect(ui->savePath, &QLabel::linkActivated, this, &SettingWidget::openFile);
    connect(ui->autoSavePath, &QLabel::linkActivated, this, &SettingWidget::choosePath);

    const QString &path = getConfigPath();
    ui->savePath->setText(QString("<a href=\"%1\" style=\"color:black; text-decoration: none;\">配置路径: %2</a>").arg(path, QDir::toNativeSeparators(QFileInfo{path}.absolutePath())));
    ui->savePath->setToolTip(path);

    const QList<QByteArray> baMimeTypes = QImageWriter::supportedImageFormats();
    int index = 0;
    for (int i = 0; i < baMimeTypes.size(); ++i) {
        QString type = QString::fromUtf8(baMimeTypes[i]);
        if (type.compare("png", Qt::CaseInsensitive) == 0) {
            index = i;
        }
        ui->format->addItem(type);
    }
    ui->format->setCurrentIndex(index);
    m_save_format = ui->format->currentText();
}

SettingWidget::~SettingWidget() {
    saveConfig();
    delete ui;
}

void SettingWidget::readConfig() {
    QFile file{getConfigPath()};
    if (file.open(QFile::ReadOnly)) {
        QDataStream stream{&file};
        QString format;
        stream >> m_auto_save_key >> m_auto_save_path >> format >> m_full_screen;
        stream >> m_capture >> m_record;
        checkData(m_auto_save_key);
        checkData(m_capture);
        checkData(m_record);
        if (m_auto_save_path.length() == 0) {
            m_auto_save_key.modifiers = Qt::NoModifier;
            m_auto_save_key.key = 'A';
        }
        if (m_auto_save_key.modifiers != Qt::NoModifier) {
            emit autoSaveChanged(m_auto_save_key, m_full_screen, m_auto_save_path);
            updateKey1();
        }
        for (int i = 0; i < ui->format->count(); ++i) {
            if (format.compare(ui->format->itemText(i), Qt::CaseInsensitive) == 0) {
                ui->format->setCurrentIndex(i);
                m_save_format = format;
                break;
            }
        }
        if (m_capture.modifiers != Qt::NoModifier) {
            emit captureChanged(m_capture);
            updateKey2();
        }
        if (m_record.modifiers != Qt::NoModifier) {
            emit recordChanged(m_record);
            updateKey3();
        }
        file.close();
    } else {
        qWarning() << "打开配置文件失败：" << file.errorString();
    }
}

void SettingWidget::saveConfig() {
    QFile file{getConfigPath()};
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QDataStream stream{&file};
        stream << m_auto_save_key << m_auto_save_path << m_save_format << m_full_screen;
        stream << m_capture << m_record;
        file.flush();
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
        static QString userDir = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        if (! QFile::exists(userDir)) {
            qDebug() << QString("创建文件夹%1: %2").arg(userDir, QDir{}.mkdir(userDir) ? "成功" : "失败");
        }
        static QString userPath = userDir + QDir::separator() + QApplication::applicationName() + ".data";
        return userPath;
    }
}

void SettingWidget::cleanAutoSaveKey() {
    m_auto_save_key = {};
    updateKey1();
}

void SettingWidget::cleanCaptureKey() {
    m_capture = {};
    updateKey2();
}

void SettingWidget::cleanRecordKey() {
    m_record = {};
    updateKey3();
}

void SettingWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    updateKey1();
    updateKey2();
    updateKey3();
}

void SettingWidget::openFile(const QString &link) {
#if defined(Q_OS_LINUX)
    if (system(QString("nautilus %1 >/dev/null 2>&1 &").arg(link).toStdString().c_str()) != 0) {
        if (system(QString("thunar %1 >/dev/null 2>&1 &").arg(link).toStdString().c_str()) != 0) {
            QString path = QDir::toNativeSeparators(QFileInfo{link}.path());
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
        process.setArguments({"/C", QString("explorer %1 >NUL 2>&1").arg(QDir::toNativeSeparators(QFileInfo{link}.path()))});
    }
    process.start();
    process.waitForFinished();
    // system(QString("explorer /select,%1 >NUL 2>&1").arg(link).toStdString().c_str());
#endif
}

void SettingWidget::choosePath(const QString &link) {
    QString path = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, "选择路径", link));
    if (path.length() > 0 && QFile::exists(path)) {
        m_auto_save_path = path;
        ui->autoSavePath->setText(QString("<a href=\"%1\" style=\"color:black; text-decoration: none;\">保存路径:%1</a>").arg(path));
        ui->autoSavePath->setToolTip(path);
    }
}

void SettingWidget::confirm() {
    HotKey key1;
    HotKey key2;
    HotKey key3;

    m_full_screen = ui->fullScreen->isChecked();

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
    if (key1.modifiers != m_auto_save_key.modifiers && key1.modifiers == Qt::NoModifier) {
        msg.append("自动保存、");
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
    if (key2.modifiers != m_capture.modifiers && key2.modifiers == Qt::NoModifier) {
        msg.append("截图、");
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
    if (key3.modifiers != m_record.modifiers && key2.modifiers == Qt::NoModifier) {
        msg.append("GIF、");
    }

    if (msg.back() == QChar(0x3001)) {
        msg.remove(msg.length() - 1, 2);
        if (QMessageBox::question(this, "是否删除快捷键", msg) != QMessageBox::Yes) {
            return;
        }
    }

    this->hide();

    m_save_format = ui->format->currentText();
    if (key1 != m_auto_save_key) {
        m_auto_save_key = key1;
        if (key1.modifiers == Qt::NoModifier) {
            emit autoSaveChanged(m_auto_save_key, m_full_screen, m_auto_save_path);
        }
    } else {
        key1.modifiers = Qt::NoModifier;
    }
    if (key2 != m_capture) {
        m_capture = key2;
        if (key2.modifiers == Qt::NoModifier) {
            emit captureChanged(m_capture);
        }
    } else {
        key2.modifiers = Qt::NoModifier;
    }
    if (key3 != m_record) {
        m_record = key3;
        if (key3.modifiers == Qt::NoModifier) {
            emit recordChanged(m_record);
        }
    } else {
        key3.modifiers = Qt::NoModifier;
    }

    if (key1.modifiers != Qt::NoModifier) {
        emit autoSaveChanged(m_auto_save_key, m_full_screen, m_auto_save_path);
    }
    if (key2.modifiers != Qt::NoModifier) {
        emit captureChanged(m_capture);
    }
    if (key3.modifiers != Qt::NoModifier) {
        emit recordChanged(m_record);
    }
}

void SettingWidget::checkData(HotKey &key) {
    if ((key.modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) == Qt::NoModifier
        || key.key < 'A' || key.key > 'Z') {
        key.modifiers = Qt::NoModifier;
        key.key = 'A';
    }
}

void SettingWidget::updateKey1() {
    ui->control1->setChecked(m_auto_save_key.modifiers & Qt::ControlModifier);
    ui->alt1->setChecked(m_auto_save_key.modifiers & Qt::AltModifier);
    ui->shift1->setChecked(m_auto_save_key.modifiers & Qt::ShiftModifier);
    ui->key1->setCurrentIndex(m_auto_save_key.key - 'A');
    ui->fullScreen->setChecked(m_full_screen);
    ui->program->setChecked(! m_full_screen);
    if (m_auto_save_path.length() == 0) {
        ui->autoSavePath->setText(QString("<a href=\"%1\" style=\"color:black; text-decoration: none;\">保存路径: 未设置</a>").arg(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)));
        ui->autoSavePath->setToolTip("未设置");
    } else {
        ui->autoSavePath->setText(QString("<a href=\"%1\" style=\"color:black; text-decoration: none;\">保存路径:%1</a>").arg(m_auto_save_path));
        ui->autoSavePath->setToolTip(m_auto_save_path);
    }
}

void SettingWidget::updateKey2() {
    ui->control2->setChecked(m_capture.modifiers & Qt::ControlModifier);
    ui->alt2->setChecked(m_capture.modifiers & Qt::AltModifier);
    ui->shift2->setChecked(m_capture.modifiers & Qt::ShiftModifier);
    ui->key2->setCurrentIndex(m_capture.key - 'A');
}

void SettingWidget::updateKey3() {
    ui->control3->setChecked(m_record.modifiers & Qt::ControlModifier);
    ui->alt3->setChecked(m_record.modifiers & Qt::AltModifier);
    ui->shift3->setChecked(m_record.modifiers & Qt::ShiftModifier);
    ui->key3->setCurrentIndex(m_record.key - 'A');
}
