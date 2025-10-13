#include "SettingWidget.h"
#include "ui_SettingWidget.h"

#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QFileDialog>
#include <QImageWriter>
#ifdef OCR
#include "Ocr.h"
#endif
#if defined(Q_OS_LINUX)
#include <pwd.h>
#elif defined(Q_OS_WINDOWS)
#include <windows.h>
#endif

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

SettingWidget::SettingWidget(QWidget *parent): QWidget(parent), ui(new Ui::SettingWidget), m_scale_ctrl{true} {
    ui->setupUi(this);
    setWindowTitle("设置");

    connect(ui->cancel, &QPushButton::clicked, this, &SettingWidget::hide);
    connect(ui->ok, &QPushButton::clicked, this, &SettingWidget::confirm);
    connect(ui->savePath, &QLabel::linkActivated, this, &SettingWidget::openFile);
    connect(ui->autoSavePath, &QLabel::linkActivated, this, &SettingWidget::choosePath);
    connect(ui->key_help, &QPushButton::clicked, this, &SettingWidget::keyHelp);

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
    ui->ocr_setting->setVisible(false);

#ifdef Q_OS_LINUX
    QString outPath = QDir::toNativeSeparators(getUserHomePath() + "/.config/screenshot/screenshot.png");
    if (! QFile::exists(outPath)) {
        QImage image{":/images/screenshot.ico"};
        image.save(outPath, "png");
    }
#endif
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
        stream >> m_capture >> m_record >> m_scale_ctrl;
#ifdef OCR
        QByteArray ocrArray;
        stream >> ocrArray;
        OcrInstance->restore(ocrArray);
#endif
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
        ui->radioButton->setChecked(m_scale_ctrl);
        ui->radioButton_2->setChecked(! m_scale_ctrl);
        emit scaleKeyChanged(m_scale_ctrl);
        file.close();
    } else {
        qWarning() << "打开配置文件失败：" << file.errorString();
    }
}

void SettingWidget::saveConfig() {
    qDebug() << "save config";
    QFile file{getConfigPath()};
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QDataStream stream{&file};
        stream << m_auto_save_key << m_auto_save_path << m_save_format << m_full_screen;
        stream << m_capture << m_record << m_scale_ctrl;
#ifdef OCR
        QByteArray ocrArray = OcrInstance->save();
        stream << ocrArray;
#endif
        file.flush();
        file.close();
    } else {
        QString error = QString("write config failed: %1").arg(file.errorString());
        qWarning() << error;
    }
}

const QString& SettingWidget::getConfigPath() const {
    static QString applicationPath = QDir::toNativeSeparators(QApplication::applicationDirPath() + "/screenshot.data");
    if (QFile::exists(applicationPath)) {
        return applicationPath;
    } else {
#if defined(Q_OS_WINDOWS)
        static QString userDir = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/screenshot");
#elif defined(Q_OS_LINUX)
        static QString userDir = QDir::toNativeSeparators(getUserHomePath() + "/.config/screenshot");
#endif
        if (! QFile::exists(userDir)) {
            qDebug() << QString("创建文件夹%1: %2").arg(userDir, QDir{}.mkpath(userDir) ? "成功" : "失败");
        }
        static QString userPath = QDir::toNativeSeparators(userDir + "/screenshot.data");
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
    bool b1 = isSelfStart(true);
    bool b2 = isSelfStart(false);
    if (b1 && b2) {
        ui->self_start->setChecked(true);
        ui->all_user->setChecked(true);
        setSelfStart(false, false);
    } else if (b1) {
        ui->self_start->setChecked(true);
        ui->all_user->setChecked(true);
    } else if (b2) {
        ui->self_start->setChecked(true);
        ui->all_user->setChecked(false);
    } else {
        ui->self_start->setChecked(false);
        ui->all_user->setChecked(false);
    }
#ifdef OCR
    if (! ui->ocr_setting->isVisible()) {
        QWidget *w = OcrInstance->getSettingWidget();
        if (w) {
            ui->ocr_setting->setVisible(true);
            w->setParent(this);
            w->setWindowModality(Qt::WindowModal);
            w->setAttribute(Qt::WA_ShowModal, true);
            w->setWindowFlag(Qt::Dialog, true);
            connect(this, &SettingWidget::destroyed, w, &QWidget::deleteLater);
            connect(ui->ocr_setting, &QPushButton::clicked, w, &QWidget::show, static_cast<Qt::ConnectionType>(Qt::AutoConnection | Qt::UniqueConnection));
        }
    }
#endif
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
        ui->autoSavePath->setText(QString("<a href=\"%1\" style=\"color:black; text-decoration: none;\">保存路径:%1</a>").arg(path));
        ui->autoSavePath->setToolTip(path);
    }
}

void SettingWidget::confirm() {
    bool b1 = isSelfStart(true);
    bool b2 = isSelfStart(false);
    if (ui->self_start->isChecked()) {
        if (ui->all_user->isChecked()) {
            if (b2) {
                QString error = setSelfStart(false, false);
                if (! error.isEmpty()) {
                    QMessageBox::warning(this, "取消用户自启动失败", error);
                    return;
                }
            }
            if (! b1) {
                QString error = setSelfStart(true, true);
                if (! error.isEmpty()) {
                    QMessageBox::warning(this, "设置全局自启动失败", error);
                    return;
                }
            }
        } else {
            if (b1) {
                QString error = setSelfStart(false, true);
                if (!error.isEmpty()) {
                    QMessageBox::warning(this, "取消全局自启动失败", error);
                    return;
                }
            }
            if (! b2) {
                QString error = setSelfStart(true, false);
                if (!error.isEmpty()) {
                    QMessageBox::warning(this, "设置用户自启动失败", error);
                    return;
                }
            }
        }
    } else {
        if (b1) {
            QString error = setSelfStart(false, true);
            if (!error.isEmpty()) {
                QMessageBox::warning(this, "取消全局自启动失败", error);
                return;
            }
        }
        if (b2) {
            QString error = setSelfStart(false, false);
            if (!error.isEmpty()) {
                QMessageBox::warning(this, "取消用户自启动失败", error);
                return;
            }
        }
    }

    HotKey key1;
    HotKey key2;
    HotKey key3;
    bool save = false;
    const QString savePath = (ui->autoSavePath->toolTip() == "未设置" ? "" : ui->autoSavePath->toolTip());

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
    if (savePath.length() == 0) {
        if (key1.modifiers != Qt::NoModifier) {
            QMessageBox::warning(this, "错误", "未设置自动保存路径");
            return;
        }
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
    if (key3.modifiers != m_record.modifiers && key3.modifiers == Qt::NoModifier) {
        msg.append("GIF、");
    }

    if (msg.back() == QChar(0x3001)) {
        msg.remove(msg.length() - 1, 2);
        if (QMessageBox::question(this, "是否删除快捷键", msg) != QMessageBox::Yes) {
            return;
        }
    }

    this->hide();

    if (key1 != m_auto_save_key) {
        m_auto_save_key = key1;
        if (key1.modifiers == Qt::NoModifier) {
            save = true;
            emit autoSaveChanged(m_auto_save_key, m_full_screen, savePath);
        }
    } else {
        key1.modifiers = Qt::NoModifier;
    }
    if (key2 != m_capture) {
        m_capture = key2;
        if (key2.modifiers == Qt::NoModifier) {
            save = true;
            emit captureChanged(m_capture);
        }
    } else {
        key2.modifiers = Qt::NoModifier;
    }
    if (key3 != m_record) {
        m_record = key3;
        if (key3.modifiers == Qt::NoModifier) {
            save = true;
            emit recordChanged(m_record);
        }
    } else {
        key3.modifiers = Qt::NoModifier;
    }

    if (key1.modifiers != Qt::NoModifier) {
        save = true;
        emit autoSaveChanged(m_auto_save_key, m_full_screen, savePath);
    }
    if (key2.modifiers != Qt::NoModifier) {
        save = true;
        emit captureChanged(m_capture);
    }
    if (key3.modifiers != Qt::NoModifier) {
        save = true;
        emit recordChanged(m_record);
    }

    if (m_full_screen != ui->fullScreen->isChecked()) {
        save = true;
        m_full_screen = ui->fullScreen->isChecked();
    }
    if (m_save_format != ui->format->currentText()) {
        save = true;
        m_save_format = ui->format->currentText();
    }
    if (m_auto_save_path != savePath) {
        save = true;
        m_auto_save_path = savePath;
    }
    if (m_scale_ctrl != ui->radioButton->isChecked()) {
        save = true;
        m_scale_ctrl = ui->radioButton->isChecked();
        emit scaleKeyChanged(m_scale_ctrl);
    }

    if (save) {
        saveConfig();
    }
}

void SettingWidget::keyHelp() {
    QString helpText = QString("<h3>截图和置顶窗口</h3>"
                               "<ul>"
                               "<li><b>ESC</b>: 关闭</li>"
                               "<li><b>Ctrl+C</b>: 复制图片到剪贴板</li>"
                               "<li><b>Ctrl+S</b>: 保存图片到电脑</li>"
                               "<li><b>Ctrl+Z</b>: 撤销对图片的编辑</li>"
                               "<li><b>Ctrl+Y</b>: 恢复对图片的编辑</li>"
                               "<li><b>C</b>: 复制鼠标位置的 RGB 到剪贴板</li>"
                               "<li><b>← ↑ → ↓</b>: 微调位置</li>"
                               "</ul>"
                               "<h3>置顶窗口</h3>"
                               "<ul>"
                               "<li><b>%1</b>: 长截图上下滑动</li>"
                               "<li><b>%2</b>: 调整窗口大小</li>"
                               "</ul>")
                           .arg(m_scale_ctrl ? "鼠标滚轮" : "Ctrl + 鼠标滚轮", m_scale_ctrl ? "Ctrl + 鼠标滚轮" : "鼠标滚轮");
    QMessageBox *box = new QMessageBox{this};
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowTitle("按键帮助");
    box->setWindowModality(Qt::WindowModal);
    box->setText(helpText);
    QIcon icon = box->windowIcon();
    QSize size = icon.actualSize(QSize(64, 64));
    box->setIconPixmap(icon.pixmap(size));
    box->show();
    // QMessageBox::about(this, "按键帮助", helpText);
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

QString SettingWidget::setSelfStart(bool start, bool allUser) {
#if defined(Q_OS_LINUX)
    QFile file;
    if (allUser) {
        if (start) {
            file.setFileName(QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/screenshot.tmp"));
        } else {
            file.setFileName("/etc/xdg/autostart/screenshot.desktop");
        }
    } else {
        QString path = QDir::toNativeSeparators(getUserHomePath() + "/.config/autostart");
        QDir{}.mkpath(path);
        file.setFileName(QDir::toNativeSeparators(path + "/screenshot.desktop"));
    }
    if (start) {
        if (! file.open(QFile::WriteOnly | QFile::Truncate)) {
            return file.errorString();
        }
        QString imgPath = QDir::toNativeSeparators(getUserHomePath() + "/.config/screenshot/screenshot.png");
        QString content = QString("[Desktop Entry]\n"
                                  "Type=Application\n"
                                  "Name=截图工具\n"
                                  "Comment=一个简单的截图工具\n"
                                  "Exec=%1\n"
                                  "Icon=%2\n"
                                  "Terminal=false\n"
                                  "X-GNOME-Autostart-enabled=true\n"
                                  "X-GNOME-Autostart-Delay=3\n"
                                  "Categories=Utility;Graphics;\n"
                                  "StartupNotify=false\n"
                                  "NoDisplay=false\n").arg(QCoreApplication::applicationFilePath(), imgPath);
        file.write(content.toUtf8());
        file.close();
        file.setPermissions(file.permissions() | QFileDevice::ExeOwner);
        if (allUser) {
            QString command;
            command = QString("pkexec bash -c 'mv \"%1\" /etc/xdg/autostart/screenshot.desktop'").arg(file.fileName());
            int ret = system(command.toStdString().c_str());
            file.remove();
            if (ret != 0) {
                return QString("Failed to create file, code: %1").arg(ret);
            }
        }
        return {};
    } else {
        if (file.exists()) {
            if (allUser) {
                QString command = QString("pkexec bash -c 'rm \"%1\"'").arg(file.fileName());
                int ret = system(command.toStdString().c_str());
                if (ret != 0) {
                    return QString("Failed to remove file, code: %1").arg(ret);
                }
            } else {
                if (! file.remove()) {
                    return file.errorString();
                }
            }
        }
        return {};
    }
#elif defined(Q_OS_WINDOWS)
    if (allUser) {
        wchar_t path[1024];
        GetModuleFileNameW(NULL, path, 1024);

        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = path;
        sei.nShow = SW_NORMAL;
        sei.lpParameters = start ? L"--enable-startup" : L"--disable-startup";
        sei.fMask = SEE_MASK_NOCLOSEPROCESS; // 获取进程句柄
        if (! ShellExecuteExW(&sei)) {
            return getWindowsError(GetLastError());
        }
        WaitForSingleObject(sei.hProcess, INFINITE);
        DWORD exitCode = 0;
        if (! GetExitCodeProcess(sei.hProcess, &exitCode)) {
            exitCode = static_cast<DWORD>(-1);
        }
        CloseHandle(sei.hProcess);
        return getWindowsError(exitCode);
    }
    HKEY hKey = nullptr;
    LONG result = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_QUERY_VALUE,
        nullptr,
        &hKey,
        nullptr
        );

    if (result != ERROR_SUCCESS) {
        return QString("RegCreateKeyExW failed: %1").arg(getWindowsError(result));
    }

    if (start) {
        std::wstring wAppPath = QString("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())).toStdWString();

        result = RegSetValueExW(
            hKey,
            L"screenshot",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(wAppPath.c_str()),
            static_cast<DWORD>((wAppPath.size() + 1) * sizeof(wchar_t))
            );

        RegCloseKey(hKey);

        if (result != ERROR_SUCCESS) {
            return QString("RegSetValueExW failed: %1").arg(getWindowsError(result));
        }
    } else {
        result = RegDeleteValueW(hKey, L"screenshot");
        RegCloseKey(hKey);

        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
            return QString("RegDeleteValueW failed: %1").arg(getWindowsError(result));
        }
    }

    return {};
#endif
}

bool SettingWidget::isSelfStart(bool allUser) {
#if defined(Q_OS_LINUX)
    QFile file;
    if (allUser) {
        file.setFileName("/etc/xdg/autostart/screenshot.desktop");
    } else {
        file.setFileName(QDir::toNativeSeparators(getUserHomePath() + "/.config/autostart/screenshot.desktop"));
    }
    if (! file.exists() || ! file.open(QFile::ReadOnly | QFile::Text)) return false;
    const QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    file.close();
    const QString appPath = QCoreApplication::applicationFilePath();
    for (QString line: lines) {
        line = line.trimmed();
        if (line.startsWith("Exec=") && line.contains(appPath)) {
            return true;
        }
    }
    return false;
#elif defined(Q_OS_WINDOWS)
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExW(
        allUser ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_QUERY_VALUE,
        &hKey
        );

    if (result != ERROR_SUCCESS) {
        return false;
    }

    wchar_t value[512];
    DWORD type = 0, dataSize = sizeof(value);
    result = RegQueryValueExW(
        hKey,
        L"screenshot",
        nullptr,
        &type,
        reinterpret_cast<LPBYTE>(value),
        &dataSize
        );
    RegCloseKey(hKey);

    if (result == ERROR_SUCCESS && type == REG_SZ) {
        QString regPath = QString::fromWCharArray(value).trimmed();
        if (regPath.startsWith('"') && regPath.endsWith('"')) {
            regPath = regPath.mid(1, regPath.size() - 2);
        }
        return regPath.toLower().replace('/', '\\') == QCoreApplication::applicationFilePath().toLower().replace('/', '\\');
    }
    return false;
#endif
}

#if defined(Q_OS_WINDOWS)
QString SettingWidget::getWindowsError(quint32 error) const {
    if (error == 0) return {};
    wchar_t *wstr = nullptr;
    int count = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        error,
        0,
        reinterpret_cast<LPWSTR>(&wstr),
        0,
        nullptr
        );
    QString ret;
    if (count != 0) {
        ret = QString::fromWCharArray(wstr, count);
        LocalFree(wstr);
    }
    return ret.trimmed();
}
#elif defined(Q_OS_LINUX)
QString SettingWidget::getUserHomePath() const {
    static QString homePath;
    if (homePath.isEmpty()) {
        QFile loginUid{"/proc/self/loginuid"};
        if (loginUid.open(QFile::ReadOnly)) {
            bool ok;
            quint32 uid = loginUid.readAll().toUInt(&ok);
            loginUid.close();
            if (ok) {
                struct passwd *pw = getpwuid(uid);
                if (pw) homePath = QString::fromStdString(pw->pw_dir);
            }
        }
        if (homePath.isEmpty()) {
            homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        }
    }
    return homePath;
}
#endif
