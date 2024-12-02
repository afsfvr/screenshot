#include "HotKeyWidget.h"
#include "ui_HotKeyWidget.h"

#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>

HotKeyWidget::HotKeyWidget(HotKey *capture, HotKey *record, QWidget *parent):
    QWidget(parent), ui(new Ui::HotKeyWidget), m_capture(capture), m_record(record) {

    ui->setupUi(this);

    connect(ui->radioButton, &QRadioButton::toggled, this, &HotKeyWidget::switchHotKey);
    connect(ui->cancel, &QPushButton::clicked, this, &HotKeyWidget::hide);
    connect(ui->ok, &QPushButton::clicked, this, &HotKeyWidget::updateHotKey);
    connect(ui->label, &QLabel::linkActivated, this, [](const QString &link){
#if defined(Q_OS_LINUX)
        if (system(QString("nautilus %1 >/dev/null 2>&1").arg(link).toStdString().c_str()) != 0) {
            if (system(QString("thunar %1 >/dev/null 2>&1").arg(link).toStdString().c_str()) != 0) {
                QFileInfo info{link};
                QString path = info.path();
                if (system(QString("dolphin %1 >/dev/null 2>&1").arg(path).toStdString().c_str()) != 0) {
                    qDebug() << system(QString("xdg-open %1 >/dev/null 2>&1").arg(path).toStdString().c_str());
                }
            }
        }
#else
        QProcess process;
        process.setProgram("cmd.exe");
        process.setArguments({"/C", QString("explorer /select,%1 >NUL 2>&1").arg(link)});
        process.start();
        process.waitForFinished();
        // system(QString("explorer /select,%1 >NUL 2>&1").arg(link).toStdString().c_str());
#endif
    });
}

HotKeyWidget::~HotKeyWidget() {
    delete ui;
}

void HotKeyWidget::setConfigPath(const QString &path) {
    ui->label->setText(QString("<a href=\"%1\" style=\"color:black; text-decoration: none;\">配置路径: %2</a>").arg(path, QFileInfo{path}.absolutePath()));
}

void HotKeyWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    ui->radioButton->setChecked(true);
    switchHotKey();
}

void HotKeyWidget::switchHotKey() {
    if (ui->radioButton->isChecked()) {
        m_hotkey = *m_capture;
    } else {
        m_hotkey = *m_record;
    }
    ui->control->setChecked(m_hotkey.modifiers & Qt::ControlModifier);
    ui->alt->setChecked(m_hotkey.modifiers & Qt::AltModifier);
    ui->shift->setChecked(m_hotkey.modifiers & Qt::ShiftModifier);
    ui->comboBox->setCurrentIndex(m_hotkey.key - 'A');
    setWindowTitle(QString("修改%1快捷键").arg(ui->radioButton->isChecked() ? "截图" : "GIF"));
}

void HotKeyWidget::updateHotKey() {
    m_hotkey.modifiers = Qt::NoModifier;
    if (ui->control->isChecked()) {
        m_hotkey.modifiers |= Qt::ControlModifier;
    }
    if (ui->alt->isChecked()) {
        m_hotkey.modifiers |= Qt::AltModifier;
    }
    if (ui->shift->isChecked()) {
        m_hotkey.modifiers |= Qt::ShiftModifier;
    }
    m_hotkey.key = 'A' + ui->comboBox->currentIndex();
    if (m_hotkey.key < 'A' || m_hotkey.key > 'Z') {
        m_hotkey.key = 'A';
        ui->comboBox->setCurrentIndex(0);
    }

    if (m_hotkey.modifiers == Qt::NoModifier) {
        if (QMessageBox::question(this, "是否删除快捷键", QString("是否删除%1快捷键").arg(ui->radioButton->isChecked() ? "截图" : "GIF")) != QMessageBox::Yes) {
            return;
        }
    }
    if (ui->radioButton->isChecked()) {
        *m_capture = m_hotkey;
        emit capture();
    } else {
        *m_record = m_hotkey;
        emit record();
    }
    hide();
}
